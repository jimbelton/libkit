#include <string.h>
#include <xmmintrin.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "sxe-dict.h"
#include "sxe-hash.h"
#include "sxe-util.h"

struct sxe_dict_node {
    struct sxe_dict_node *next;
    union {
        char       *key;
        const char *key_ref;
        uint64_t    key_hash;
    };
    const void     *value;
};

struct sxe_dict_node_with_len {
    struct sxe_dict_node node;
    size_t               len;
};

static inline size_t
sxe_dict_node_size(const struct sxe_dict *dic)
{
    return dic->flags & (SXE_DICT_FLAG_KEYS_STRING | SXE_DICT_FLAG_KEYS_HASHED)
           ? sizeof(struct sxe_dict_node) : sizeof(struct sxe_dict_node_with_len);
}

/* Internal function used to create a new dictionary node, public for hysterical raisins
 */
struct sxe_dict_node *
sxe_dict_node_new(const struct sxe_dict *dic, const char *key, size_t len)
{
    struct sxe_dict_node *node = kit_malloc(sxe_dict_node_size(dic));

    if (!node) {
        SXEL2(": Failed to allocate dictionary node");    /* COVERAGE EXCLUSION: Out of memory */
        return NULL;                                      /* COVERAGE EXCLUSION: Out of memory */
    }

    if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED)
        node->key_hash = (uint64_t)key;
    else if (dic->flags & SXE_DICT_FLAG_KEYS_NOCOPY) {
        node->key_ref = key;

        if (!(dic->flags & SXE_DICT_FLAG_KEYS_STRING))
            ((struct sxe_dict_node_with_len *)node)->len = len;
    } else if (dic->flags & SXE_DICT_FLAG_KEYS_STRING) {
        node->key = kit_malloc(len + 1);
        memcpy(node->key, key, len);
        node->key[len] = '\0';
    } else {    // Copied non string key (i.e. SXE_DICT_FLAG_KEYS_BINARY)
        node->key = kit_malloc(len);
        memcpy(node->key, key, len);
        ((struct sxe_dict_node_with_len *)node)->len = len;
    }

    node->next  = NULL;
    node->value = NULL;
    return node;
}

static void
sxe_dict_node_free(struct sxe_dict *dic, struct sxe_dict_node *node)
{
    struct sxe_dict_node *next = node->next;

    if (!(dic->flags & (SXE_DICT_FLAG_KEYS_NOCOPY | SXE_DICT_FLAG_KEYS_HASHED)))
        kit_free(node->key);

    kit_free(node);

    if (next)
        sxe_dict_node_free(dic, next);
}

/**
 * Initialize a dictionary with full control of it's properties
 *
 * @param dic          Dictionary to initialize
 * @param initial_size The initial size of the dictionary's hash table
 * @param load         The percentage of hash table buckets to inserted values before the table is grown
 * @param growth       The factor to grow by when the load is exceeded
 * @param flags        Either SXE_DICT_FLAG_KEYS_BINARY (exact copies), SXE_DICT_FLAG_KEYS_NOCOPY (reference) or
 *                     SXE_DICT_FLAG_KEYS_STRING (copy with NUL termination), or SXE_DICT_FLAGS_KEYS_HASHED (only hash saved).
 *
 * @return true on success, false if out of memory
 */
bool
sxe_dict_init(struct sxe_dict *dic, unsigned initial_size, unsigned load, unsigned growth, unsigned flags)
{
    dic->size   = initial_size;
    dic->count  = 0;
    dic->table  = initial_size ? MOCKERROR(sxe_dict_init, NULL, ENOMEM,
                                           kit_calloc(sxe_dict_node_size(dic), initial_size)) : NULL;
    dic->load   = load;
    dic->growth = growth;
    dic->flags  = flags;
    return initial_size == 0 || dic->table;
}

/**
 * Create a dictionary that grows at 100% (as many entries as buckets) by a factor of 2, and copies keys without NUL terminating
 *
 * @param initial_size The initial size of the dictionary's hash table
 *
 * @return The dictionary or NULL on out of memory
 */
struct sxe_dict *
sxe_dict_new(unsigned initial_size)
{
    struct sxe_dict *dic = kit_malloc(sizeof(struct sxe_dict));

    if (!dic || MOCKERROR(sxe_dict_new, true, ENOMEM, !sxe_dict_init(dic, initial_size, 100, 2, SXE_DICT_FLAG_KEYS_BINARY))) {
        SXEL2(": Failed to allocate dictionary structure or buckets");
        kit_free(dic);
        return NULL;
    }

    return dic;
}

/**
 * Finalize a dictionary by freeing all memory allocated to it by sxe-dict
 *
 * @param dic Dictionary to finalize
 */
void
sxe_dict_fini(struct sxe_dict *dic)
{
    for (unsigned i = 0; i < dic->size; i++)
        if (dic->table[i])
            sxe_dict_node_free(dic, dic->table[i]);

    kit_free(dic->table);
    dic->table = NULL;
}

/**
 * Free a dictionary after freeing all memory allocated to it by sxe-dict
 *
 * @param dic Dictionary to finalize
 */
void
sxe_dict_free(struct sxe_dict *dic)
{
    if (!dic)
        return;

    sxe_dict_fini(dic);
    kit_free(dic);
}

static void
sxe_dict_reinsert_when_resizing(struct sxe_dict *dic, struct sxe_dict_node *k2)
{
    unsigned n;

    if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED)
        n = k2->key_hash % dic->size;
    else if (dic->flags & SXE_DICT_FLAG_KEYS_STRING)
        n = sxe_hash_64(k2->key, 0) % dic->size;
    else
        n = sxe_hash_64(k2->key, ((struct sxe_dict_node_with_len *)k2)->len) % dic->size;

    if (dic->table[n] == 0) {
        dic->table[n] = k2;
        return;
    }

    struct sxe_dict_node *k = dic->table[n];
    k2->next = k;
    dic->table[n] = k2;
}

bool
sxe_dict_resize(struct sxe_dict *dic, int newsize)
{
    unsigned               oldsize = dic->size;
    struct sxe_dict_node **old     = dic->table;

    if (!(dic->table = MOCKERROR(sxe_dict_resize, NULL, ENOMEM, kit_calloc(sizeof(struct sxe_dict_node*), newsize)))) {
        SXEL2(": Failed to allocate bigger table");
        dic->table = old;
        return false;
    }

    dic->size = newsize;

    for (unsigned i = 0; i < oldsize; i++) {
        struct sxe_dict_node *node = old[i];

        while (node) {
            struct sxe_dict_node *next = node->next;
            node->next = 0;
            sxe_dict_reinsert_when_resizing(dic, node);
            node = next;
        }
    }

    kit_free(old);
    return true;
}

static inline bool
compare_keys(const struct sxe_dict *dic, const struct sxe_dict_node *link, const void *key, size_t len)
{
    if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED) {
        if (link->key == key)
            return true;
    } if (dic->flags & SXE_DICT_FLAG_KEYS_STRING) {
        if (strcmp(link->key, key) == 0)
            return true;
    } else {    // Key is binary
        if (((const struct sxe_dict_node_with_len *)link)->len == len && memcmp(link->key, key, len) == 0)
            return true;
    }

    return false;
}

/**
 * Add a key to a dictionary
 *
 * @param dic The dictionary
 * @param key The key
 * @param len The size of the key, or 0 if it's a string to determine its length with strlen
 *
 * @return A pointer to a value or NULL on out of memory
 *
 * @note If the caller always saves a non-NULL value in the value pointed at by the return, then if there is a collision, the
 *       value pointed to by the return should be something other than NULL.
 */
const void **
sxe_dict_add(struct sxe_dict *dic, const void *key, size_t len)
{
    struct sxe_dict_node **link;

    len = len ?: strlen(key);

    if (dic->table == NULL) {    // If this is a completely empty dictionary
        if (!(dic->table = MOCKERROR(sxe_dict_add, NULL, ENOMEM, kit_calloc(sizeof(struct sxe_dict_node *), 1)))) {
            SXEL2(": Failed to allocate initial table");
            return NULL;
        }

        dic->size  = 1;
    }

    uint64_t hash   = sxe_hash_64(key, len);
    unsigned bucket = hash % dic->size;

    if (dic->table[bucket] != NULL) {
        unsigned load = dic->count * 100 / dic->size;

        if (load >= dic->load)
            if (!sxe_dict_resize(dic, dic->size * dic->growth))
                return NULL;

        bucket = hash % dic->size;
    }

    if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED)
        key = (const void *)hash;

    for (link = &dic->table[bucket]; *link != NULL; link = &((*link)->next))    // For each node in the bucket
        if (compare_keys(dic, *link, key, len))
            return &((*link)->value);

    if ((*link = sxe_dict_node_new(dic, key, len)))
        dic->count++;

    return *link ? &((*link)->value) : NULL;
}

/**
 * Find a key in a dictionary
 *
 * @param dic The dictionary
 * @param key The key
 * @param len The size of the key, or 0 if it's a string to determine its length with strlen
 *
 * @return The value, or NULL if the key is not found.
 */
const void *
sxe_dict_find(const struct sxe_dict *dic, const void *key, size_t len)
{
    struct sxe_dict_node *node;

    if (dic->table == NULL)    // If the dictionary is empty and its initial_size was 0, the key is not found.
        return NULL;

    len = len ?: strlen(key);
    uint64_t hash   = sxe_hash_64((const char *)key, len);
    unsigned bucket = hash % dic->size;

    #if defined(__MINGW32__) || defined(__MINGW64__)
    __builtin_prefetch(gc->table[bucket]);
    #endif

    #if defined(_WIN32) || defined(_WIN64)
    _mm_prefetch((char*)gc->table[bucket], _MM_HINT_T0);
    #endif

    if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED)
        key = (const void *)hash;

    for (node = dic->table[bucket]; node != NULL; node = node->next)
        if (compare_keys(dic, node, key, len))
            return node->value;

    return NULL;
}

/**
 * Function to walk across the dictionary, visiting each entry
 *
 * @param dic  Pointer to the dictionary
 * @param func The function called for each entry
 * @param user An arbitrary object pointer that is passed to the function
 *
 * @return false if a call to the function returned false, aborting the walk, true if all entries were visited
 */
bool
sxe_dict_walk(const struct sxe_dict *dic, sxe_dict_iter func, void *user)
{
    struct sxe_dict_node *node;

    for (unsigned i = 0; i < dic->size; i++) {
        for (node = dic->table[i]; node != NULL; node = node->next)
            if (dic->flags & SXE_DICT_FLAG_KEYS_HASHED) {
                if (!func(&node->key, sizeof(uint64_t), &node->value, user))
                    return false;
            } else if (dic->flags & SXE_DICT_FLAG_KEYS_STRING) {
                if (!func(node->key, strlen(node->key), &node->value, user))
                    return false;
            } else {
                if (!func(node->key, ((struct sxe_dict_node_with_len *)node)->len, &node->value, user))
                    return false;
            }
    }

    return true;
}
