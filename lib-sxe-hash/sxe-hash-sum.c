#include <string.h>

#include "sxe-hash.h"

/**
 * Compute a hash sum of a fixed length or NUL terminated key
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 32 bit hash value
 */
#ifndef SXE_DISABLE_XXHASH
unsigned (*sxe_hash_sum)(const void *key, size_t length) = sxe_hash_xxh32;    // XXH32 is the default hash algorithm
#else
unsigned (*sxe_hash_sum)(const void *key, size_t length) = NULL;              // Build with SXE_DISABLE_XXHASH for no default
#endif

/**
 * Override the default hash sum function (xxh32)
 *
 * @param new_hash_sum Pointer to a function that takes a key and a length and returns an unsigned sum
 *
 * @return A pointer to the previous hash sum function
 *
 * @note If the function is passed 0 as the length, it should use strlen to compute the length of the key
 */
SXE_HASH_FUNC
sxe_hash_override_sum(unsigned (*new_hash_sum)(const void *key, size_t length))
{
    SXE_HASH_FUNC old_hash_sum = sxe_hash_sum;

    sxe_hash_sum = new_hash_sum;
    return old_hash_sum;
}


/**
 * Compute a 64 bit hash sum of a fixed length or NUL terminated key
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 64 bit hash value
 */
#ifndef SXE_DISABLE_XXHASH
uint64_t (*sxe_hash_64)(const void *key, size_t length) = sxe_hash_xxh64;    // XXH64 is the default 64 bit hash algorithm
#else
uint64_t (*sxe_hash_64)(const void *key, size_t length) = NULL;              // Build with SXE_DISABLE_XXHASH for no default
#endif

/**
 * Override the default 64 bit hash sum function (xxh64)
 *
 * @param new_hash_64 Pointer to a function that takes a key and a length and returns an uint64_t sum
 *
 * @return A pointer to the previous 64 bit hash sum function
 *
 * @note If the function is passed 0 as the length, it should use strlen to compute the length of the key
 */
SXE_HASH_64_FUNC
sxe_hash_override_64(uint64_t (*new_hash_64)(const void *key, size_t length))
{
    SXE_HASH_64_FUNC old_hash_64 = sxe_hash_64;

    sxe_hash_64 = new_hash_64;
    return old_hash_64;
}

/**
 * Compute a 128 bit hash sum of a fixed length or NUL terminated key
 *
 * @param key      Pointer to the key
 * @param length   Length of the key in bytes or 0 to use strlen
 * @param hash_out Pointer to an array of 16 bytes (i.e. uint8_t values)
 */
#ifndef SXE_DISABLE_XXHASH
void (*sxe_hash_128)(const void *key, size_t length, uint8_t *hash_out) = sxe_hash_xxh128;    // XXH128 is the default
#else
void (*sxe_hash_128)(const void *key, size_t length, uint8_t *hash_out) = NULL;    // Build w/SXE_DISABLE_XXHASH for no default
#endif

/**
 * Override the default 128 bit hash sum function (xxh128)
 *
 * @param new_hash_128 Pointer to a function that takes a key and a length and a pointer to an array of 16 uint8_t values
 *
 * @return A pointer to the previous hash sum function
 * 
 * @note If the function is passed 0 as the length, it should use strlen to compute the length of the key
 */
SXE_HASH_128_FUNC
sxe_hash_override_128(void (*new_hash_128)(const void *key, size_t length, uint8_t *hash_out))
{
    SXE_HASH_128_FUNC old_hash_128 = sxe_hash_128;

    sxe_hash_128 = new_hash_128;
    return old_hash_128;
}
