/*
Copyright (c) 2017. The YARA Authors. All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef YR_CRYPTO_H
#define YR_CRYPTO_H

#define YR_MD5_LEN    16
#define YR_SHA1_LEN   20
#define YR_SHA256_LEN 32

#if defined(HAVE_LIBCRYPTO)
#include <openssl/crypto.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

typedef MD5_CTX yr_md5_ctx;
typedef SHA_CTX yr_sha1_ctx;
typedef SHA256_CTX yr_sha256_ctx;

#define yr_md5_init(ctx)              MD5_Init(ctx)
#define yr_md5_update(ctx, data, len) MD5_Update(ctx, data, len)
#define yr_md5_final(digest, ctx)     MD5_Final(digest, ctx)

#define yr_sha1_init(ctx)              SHA1_Init(ctx)
#define yr_sha1_update(ctx, data, len) SHA1_Update(ctx, data, len)
#define yr_sha1_final(digest, ctx)     SHA1_Final(digest, ctx)

#define yr_sha256_init(ctx)              SHA256_Init(ctx)
#define yr_sha256_update(ctx, data, len) SHA256_Update(ctx, data, len)
#define yr_sha256_final(digest, ctx)     SHA256_Final(digest, ctx)

#elif defined(HAVE_WINCRYPT_H)
#include <windows.h>

#include <wincrypt.h>

extern HCRYPTPROV yr_cryptprov;

typedef HCRYPTHASH yr_md5_ctx;
typedef HCRYPTHASH yr_sha1_ctx;
typedef HCRYPTHASH yr_sha256_ctx;

#define yr_md5_init(ctx) CryptCreateHash(yr_cryptprov, CALG_MD5, 0, 0, ctx)
#define yr_md5_update(ctx, data, len) \
  CryptHashData(*ctx, (const BYTE*) data, len, 0)
#define yr_md5_final(digest, ctx)                         \
  {                                                       \
    DWORD len = YR_MD5_LEN;                               \
    CryptGetHashParam(*ctx, HP_HASHVAL, digest, &len, 0); \
    CryptDestroyHash(*ctx);                               \
  }

#define yr_sha1_init(ctx) CryptCreateHash(yr_cryptprov, CALG_SHA1, 0, 0, ctx)
#define yr_sha1_update(ctx, data, len) \
  CryptHashData(*ctx, (const BYTE*) data, len, 0)
#define yr_sha1_final(digest, ctx)                        \
  {                                                       \
    DWORD len = YR_SHA1_LEN;                              \
    CryptGetHashParam(*ctx, HP_HASHVAL, digest, &len, 0); \
    CryptDestroyHash(*ctx);                               \
  }

#define yr_sha256_init(ctx) \
  CryptCreateHash(yr_cryptprov, CALG_SHA_256, 0, 0, ctx)
#define yr_sha256_update(ctx, data, len) \
  CryptHashData(*ctx, (const BYTE*) data, len, 0)
#define yr_sha256_final(digest, ctx)                      \
  {                                                       \
    DWORD len = YR_SHA256_LEN;                            \
    CryptGetHashParam(*ctx, HP_HASHVAL, digest, &len, 0); \
    CryptDestroyHash(*ctx);                               \
  }

#elif defined(HAVE_COMMONCRYPTO_COMMONCRYPTO_H)
#include <CommonCrypto/CommonDigest.h>

typedef CC_MD5_CTX yr_md5_ctx;
typedef CC_SHA1_CTX yr_sha1_ctx;
typedef CC_SHA256_CTX yr_sha256_ctx;

#define yr_md5_init(ctx)              CC_MD5_Init(ctx)
#define yr_md5_update(ctx, data, len) CC_MD5_Update(ctx, data, len)
#define yr_md5_final(digest, ctx)     CC_MD5_Final(digest, ctx)

#define yr_sha1_init(ctx)              CC_SHA1_Init(ctx)
#define yr_sha1_update(ctx, data, len) CC_SHA1_Update(ctx, data, len)
#define yr_sha1_final(digest, ctx)     CC_SHA1_Final(digest, ctx)

#define yr_sha256_init(ctx)              CC_SHA256_Init(ctx)
#define yr_sha256_update(ctx, data, len) CC_SHA256_Update(ctx, data, len)
#define yr_sha256_final(digest, ctx)     CC_SHA256_Final(digest, ctx)

#elif defined(HAVE_MBEDTLS)
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

typedef mbedtls_md5_context yr_md5_ctx;
typedef mbedtls_sha1_context yr_sha1_ctx;
typedef mbedtls_sha256_context yr_sha256_ctx;

#define yr_md5_init(ctx)                    { mbedtls_md5_init(ctx); mbedtls_md5_starts_ret(ctx); }
#define yr_md5_update(ctx, data, len)       mbedtls_md5_update_ret(ctx, data, len)
#define yr_md5_final(digest, ctx)           { mbedtls_md5_finish_ret(ctx, digest); mbedtls_md5_free(ctx); }

#define yr_sha1_init(ctx)                   { mbedtls_sha1_init(ctx); mbedtls_sha1_starts_ret(ctx); }
#define yr_sha1_update(ctx, data, len)      mbedtls_sha1_update_ret(ctx, data, len)
#define yr_sha1_final(digest, ctx)          { mbedtls_sha1_finish_ret(ctx, digest); mbedtls_sha1_free(ctx); }

#define yr_sha256_init(ctx)                 { mbedtls_sha256_init(ctx); mbedtls_sha256_starts_ret(ctx, false); }
#define yr_sha256_update(ctx, data, len)    mbedtls_sha256_update_ret(ctx, data, len)
#define yr_sha256_final(digest, ctx)        { mbedtls_sha256_finish_ret(ctx, digest); mbedtls_sha256_free(ctx); }

#define HAVE_COMMONCRYPTO_COMMONCRYPTO_H
#endif

#endif
