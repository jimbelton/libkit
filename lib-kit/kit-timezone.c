/*
 * Copyright (c) 2024 Cisco Systems, Inc. and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

/* A thread-safe cache for tzcode timezone objects
 */


#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "kit-alloc.h"
#include "kit-mockfail.h"
#include "kit-timezone.h"
#include "sxe-dict.h"
#include "sxe-log.h"
#include "sxe-util.h"
#include "tzcode.h"
#include "tzfile.h"

struct kit_timezone {
    pthread_mutex_t lock;            // Lock the definition of the timezone
    char           *filename;        // Name of the file with leading :
    size_t          name_len;        // Length of the filename
    time_t          time_checked;    // Time the file was last checked
    struct timespec mtim;            // Modification time when last loaded
    timezone_t      tzdata;          // Timezone specification data
};

static struct sxe_dict *timezone_cache = NULL;
static pthread_mutex_t  cache_lock     = PTHREAD_MUTEX_INITIALIZER;    // Control access to the cache
static unsigned         cache_seconds;

/* Wrap kit_malloc to provide a signature consistant with malloc
 */
static void *
kit_timezone_alloc_memory(size_t size) {
    return kit_malloc(size);
}

/* Wrap kit_free to provide a signature consistant with free
 */
static void
kit_timezone_free_memory(void *mem) {
    return kit_free(mem);
}

/**
 * Initialize the timezone cache
 *
 * @param seconds Number of seconds before checking whether a zoneinfo file has been changed or removed
 */
void
kit_timezone_initialize(unsigned seconds)
{
    SXEA1(!timezone_cache, "Already initialized");
    tz_malloc     = kit_timezone_alloc_memory;
    tz_free       = kit_timezone_free_memory;
    cache_seconds = seconds;
    SXEA1(timezone_cache = kit_malloc(sizeof(struct sxe_dict)),                "Failed to allocate timezone cache");
    SXEA1(sxe_dict_init(timezone_cache, 1, 100, 2, SXE_DICT_FLAG_KEYS_NOCOPY), "Failed to initialize timezone cache");
}

/**
 * Unlock a timezone
 *
 * @param me Pointer to a locked timezone
 */
void
kit_timezone_unlock(const struct kit_timezone *me)
{
    SXEA1(pthread_mutex_unlock(&SXE_CAST_NOCONST(struct kit_timezone *, me)->lock) == 0, "Failed to unlock the timezone");
}

/**
 * Given a timezone, reload its tzdata if needed and lock it
 *
 * @param me A pointer to a timezone
 *
 * @return The pointer to the timezone or NULL if the timezone is no longer a valid or on failure to load
 */
const struct kit_timezone *
kit_timezone_lock(const struct kit_timezone *me)
{
    struct stat          status;
    struct kit_timezone *zone;
    const char          *name = &me->filename[1];
    time_t               now;
    bool                 found;
    char                 path[PATH_MAX];

    zone = SXE_CAST_NOCONST(struct kit_timezone *, me);
    SXEA1(pthread_mutex_lock(&zone->lock) == 0, "Failed to lock the timezone");
    now = time(NULL);                         // Must do this inside the lock

    if (cache_seconds && now - zone->time_checked <= cache_seconds) {    // Don't hit the disk too often
        if (zone->tzdata)
            return zone;

        kit_timezone_unlock(zone);
        return NULL;
    }

    if (name[0] != '/') {
        snprintf(path, sizeof(path), "%s/%s", TZDIR, name);
        name = path;
    }

    found              = stat(name, &status) >= 0;
    zone->time_checked = now;

    if (found && memcmp(&status.st_mtim, &zone->mtim, sizeof(status.st_mtim)) == 0)  // If the file is unchanged
        return zone;

    if (zone->tzdata) {             // Discard the cached tzdata if any
        tz_tzfree(zone->tzdata);
        zone->tzdata = NULL;
    }

    memcpy(&zone->mtim, &status.st_mtim, sizeof(zone->mtim));

    if (!found) {    // If the file is gone, return NULL
        kit_timezone_unlock(zone);
        return NULL;
    }

    if (!(zone->tzdata = tz_tzalloc(zone->filename))) {   // Attempt to reload the tzdata for the timezone
        kit_timezone_unlock(zone);
        return NULL;
    }

    return me;
}

/**
 * Given a timezone name (without a leading colon), get the timezone from the cache, loading it if needed
 *
 * @param name A file name which, if relative, will be looked for in /usr/share/zoneinfo
 * @param len  Length of the filename or 0 to have it computed with strlen
 *
 * @return A pointer to the timezone or NULL if the name is not a valid timezone name or on failure to add or load
 */
const struct kit_timezone *
kit_timezone_load(const char *name, size_t len)
{
    struct stat          status;
    const void          *value, **value_ptr;
    struct kit_timezone *zone = NULL;
    char                *filename = NULL;
    time_t               now;
    char                 path[PATH_MAX];

    SXEA1(timezone_cache, ": kit_timezone is not initialized");
    SXEA1(pthread_mutex_lock(&cache_lock) == 0, "Failed to lock the timezone cache");

    if ((value = sxe_dict_find(timezone_cache, name, len))) {   // Already in the cache
        if (((const struct kit_timezone *)value)->tzdata)
            goto EXIT;

        zone  = SXE_CAST_NOCONST(struct kit_timezone *, value);
        value = NULL;          // Only set once there is tzdata
        now   = time(NULL);

        if (cache_seconds && now - zone->time_checked <= cache_seconds)   // Don't hit the disk too often
            goto EXIT;

        zone->time_checked = now;
    }
    else {
        len = len ?: strlen(name);
        SXEL7(": Cache miss on timezone '%.*s'", (int)len, name);

        if (!(filename = kit_malloc(len + 2)))    // + 2 is room for the leading ':' and the trailing '\0'
            goto ERROR;                           /* COVERAGE EXCLUSION: Out of memory */

        filename[0] = ':';
        memcpy(filename + 1, name, len);
        filename[len + 1] = '\0';

        if (!(zone      = kit_malloc(sizeof(struct kit_timezone)))
         || !(value_ptr = MOCKERROR(kit_timezone_load, NULL, ENOMEM, sxe_dict_add(timezone_cache, filename + 1, len))))
            goto ERROR;

        SXEA1(!*value_ptr, "Entry not found, but was there when added");
        pthread_mutex_init(&zone->lock, NULL);
        zone->filename     = filename;
        zone->name_len     = len;
        zone->time_checked = time(NULL);
        *value_ptr         = zone;
    }

    if (name[0] != '/') {
        snprintf(path, sizeof(path), "%s/%s", TZDIR, zone->filename + 1);
        name = path;
    }

    if (stat(name, &status) >= 0) {
        memcpy(&zone->mtim, &status.st_mtim, sizeof(zone->mtim));

        if ((zone->tzdata = tz_tzalloc(zone->filename)))
            value = zone;
    } else {
        memset(&zone->mtim, 0, sizeof(zone->mtim));
        zone->tzdata = NULL;
    }

    goto EXIT;

ERROR:
    kit_free(zone);
    kit_free(filename);

EXIT:
    SXEA1(pthread_mutex_unlock(&cache_lock) == 0, "Failed to unlock the timezone cache");
    return value;
}

const char *
kit_timezone_get_name(const struct kit_timezone *me, size_t *len_out)
{
    if (len_out)
        *len_out = me->name_len;

    return me->filename + 1;    // Don't include the leading ':'
}

/**
 * Using a timezone's definition, convert a UTC time_t to a local time struct tm
 *
 * @return A pointer to the tm structure or NULL on error
 */
struct tm *
kit_timezone_time_to_localtime(const struct kit_timezone *me, time_t timestamp, struct tm *tm_out)
{
    if (!me || !me->tzdata)
        return NULL;

    return tz_localtime_rz(me->tzdata, &timestamp, tm_out);
}

static bool
kit_timezone_free(const void *key, size_t len, const void **value, void *user)
{
    SXE_UNUSED_PARAMETER(key);
    SXE_UNUSED_PARAMETER(len);
    SXE_UNUSED_PARAMETER(user);
    struct kit_timezone *zone = SXE_CAST_NOCONST(struct kit_timezone *, *value);

    tz_tzfree(zone->tzdata);
    kit_free(zone->filename);
    kit_free(zone);
    return true;
}

void
kit_timezone_finalize(void)
{
    SXEA1(timezone_cache, ": not already initialized");
    sxe_dict_forEach(timezone_cache, kit_timezone_free, NULL);
    sxe_dict_fini(timezone_cache);
    kit_free(timezone_cache);
    timezone_cache = NULL;
}
