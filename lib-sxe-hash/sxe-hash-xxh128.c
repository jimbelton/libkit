/* Module that implements the default XXH128 hash function. Calling these functions will require the libxxhash DLL.
 */

#ifndef SXE_DISABLE_XXHASH

#pragma GCC diagnostic ignored "-Waggregate-return"    // To allow use of XXH3_128bits, which returns a structure

#include <string.h>
#include <xxhash.h>

#include "sxe-hash.h"

/**
 * Compute a 128 bit hash sum of a fixed length or NUL terminated key using XXH128 hash
 *
 * @param key      Pointer to the key
 * @param length   Length of the key in bytes or 0 to use strlen
 * @param hash_out Pointer to an array of 16 bytes (i.e. uint8_t)
 */
void
sxe_hash_xxh128(const void *key, size_t length, uint8_t *hash_out)
{
    XXH128_hash_t hash = XXH3_128bits(key, length ?: strlen(key));

    memcpy(hash_out,     &hash.low64,  8);
    memcpy(hash_out + 8, &hash.high64, 8);
}

/**
 * Override the 128 bit hash sum function with xxh128
 *
 * @note This restores the default 128 bit hash sum function
 */
void
sxe_hash_use_xxh128(void)
{
    sxe_hash_override_128(sxe_hash_xxh128);
}

#endif
