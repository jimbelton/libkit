/* Module that implements the default XXH64 hash function. Calling these functions will require the libxxhash DLL.
 */

#ifndef SXE_DISABLE_XXHASH

#include <string.h>
#include <xxhash.h>

#include "sxe-hash.h"

/**
 * Compute a 64 bit hash sum of a fixed length or NUL terminated key using XXH64 hash
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 32 bit hash value
 */
uint64_t
sxe_hash_xxh64(const void *key, size_t length)
{
    return XXH64(key, length ?: strlen(key), 17);    // SonarQube False Positive
}

/**
 * Override the hash sum function with xxh64
 *
 * @note This restores the default hash sum function
 */
void
sxe_hash_use_xxh64(void)
{
    sxe_hash_override_64(sxe_hash_xxh64);
}

#endif
