#ifndef SXE_DICT_H
#define SXE_DICT_H

#include <stdbool.h>
#include <stddef.h>

#define SXE_DICT_FLAG_KEYS_BINARY 0x00000000    // Keys are exact copies (no NUL termination)
#define SXE_DICT_FLAG_KEYS_NOCOPY 0x00000001    // Don't copy key values, just save references
#define SXE_DICT_FLAG_KEYS_STRING 0x00000002    // NUL terminate copies of keys
#define SXE_DICT_FLAG_KEYS_HASHED 0x00000004    // Keys are 64 bit hashes

#define sxe_dict_delete(dic) sxe_dict_free(dic)    // Deprecated name

typedef bool (*sxe_dict_iter)(const void *key, size_t key_size, const void **value, void *user);

struct sxe_dict_node;

struct sxe_dict {
    struct sxe_dict_node **table;    // Pointer to the bucket list or NULL if the dictionary is empty
    unsigned               flags;    // SXE_DICT_FLAG_*
    unsigned               size;     // Number of buckets
    unsigned               count;    // Number of entries
    unsigned               load;     // Maximum load factor (count/size) as a percentage. 100 -> count == size
    unsigned               growth;   // Growth factor when load exceeded. 2 is for doubling
};

static inline unsigned
sxe_dict_count(const struct sxe_dict *dict)
{
    return dict->count;
}

#include "sxe-dict-proto.h"

#endif
