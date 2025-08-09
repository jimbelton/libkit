/* Module that implements the default XXH32 hash function. Calling these functions will require the libxxhash DLL.
 */

#ifndef SXE_DISABLE_XXHASH

#include <string.h>
#include <xxhash.h>

#include "sxe-hash.h"

/**
 * Compute a hash sum of a fixed length or NUL terminated key using XXH32 hash
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 32 bit hash value
 */
unsigned
sxe_hash_xxh32(const void *key, size_t length)
{
    return XXH32(key, length ?: strlen(key), 17);    // SonarQube False Positive
}

/**
 * Override the hash sum function with xxh32
 *
 * @note This restores the default hash sum function
 */
void
sxe_hash_use_xxh32(void)
{
    sxe_hash_override_sum(sxe_hash_xxh32);
}

#endif
