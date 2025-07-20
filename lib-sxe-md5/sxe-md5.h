/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD5 Message-Digest Algorithm (RFC 1321).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md5
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See sxe-md5.c for more information.
 */

#ifndef __SXE_MD5_H__
#define __SXE_MD5_H__

#include <stddef.h>
#include <stdint.h>

#include "sxe-log.h"

#define SXE_MD5_SIZE          16
#define SXE_MD5_IN_HEX_LENGTH (2 * SXE_MD5_SIZE)

typedef unsigned int SXE_MD5_u32plus;    /* Any 32-bit or wider unsigned integer data type will do */

/* This is the MD5 context defined by Openwall. It must be at least as big as openssl's to be compatible
 */
typedef struct SXE_MD5 {
    SXE_MD5_u32plus lo, hi;
    SXE_MD5_u32plus a, b, c, d;
    unsigned char buffer[64];
    SXE_MD5_u32plus block[16];
} SXE_MD5;


void SXE_MD5_Init(SXE_MD5 *md5);
void SXE_MD5_Update(SXE_MD5 *md5, const void *data, unsigned long size);
void SXE_MD5_Final(uint8_t *result, SXE_MD5 *md5);

#ifdef WHEN_I_NEED_IT
SXE_RETURN sxe_md5_from_hex(uint8_t md5[SXE_MD5_SIZE], const char *md5_in_hex);
#endif
SXE_RETURN sxe_md5_to_hex(const uint8_t md5[SXE_MD5_SIZE], char *md5_in_hex, unsigned md5_in_hex_length);

#endif
