/*
 * Copyright 2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <string.h>
#include <stdarg.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/ossl_typ.h>
#include "internal/evp_int.h"
#include "evp_locl.h"

EVP_MAC_CTX *EVP_MAC_CTX_new(const EVP_MAC *mac)
{
    EVP_MAC_CTX *ctx = OPENSSL_zalloc(sizeof(EVP_MAC_CTX));

    if (ctx == NULL
        || (ctx->data = mac->new()) == NULL) {
        EVPerr(EVP_F_EVP_MAC_CTX_NEW, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(ctx);
        ctx = NULL;
    }
    ctx->meth = mac;
    return ctx;
}

static void evp_mac_ctx_cleanup(EVP_MAC_CTX *ctx)
{
    if (ctx != NULL && ctx->data != NULL) {
        ctx->meth->free(ctx->data);
        ctx->data = NULL;
    }
}

void EVP_MAC_CTX_free(EVP_MAC_CTX *ctx)
{
    evp_mac_ctx_cleanup(ctx);
    OPENSSL_free(ctx);
}

int EVP_MAC_CTX_copy(EVP_MAC_CTX *dst, EVP_MAC_CTX *src)
{
    *dst = *src;

    if (src->data != NULL && (dst->data = dst->meth->copy(src->data)) == NULL) {
        EVPerr(EVP_F_EVP_MAC_CTX_COPY, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    return 1;
}

int EVP_MAC_reset(EVP_MAC_CTX *ctx)
{
    if (ctx->data != NULL)
        return ctx->meth->reset(ctx->data);
    return 1;
}

size_t EVP_MAC_size(EVP_MAC_CTX *ctx)
{
    if (ctx->data != NULL)
        return ctx->meth->size(ctx->data);
    /* If the MAC hasn't been initialized yet, we return zero */
    return 0;
}

int EVP_MAC_init(EVP_MAC_CTX *ctx)
{
    return ctx->meth->init(ctx->data);
}

int EVP_MAC_update(EVP_MAC_CTX *ctx, const unsigned char *data, size_t datalen)
{
    return ctx->meth->update(ctx->data, data, datalen);
}

int EVP_MAC_final(EVP_MAC_CTX *ctx, unsigned char *out, size_t *poutlen)
{
    if (poutlen != NULL)
        *poutlen = ctx->meth->size(ctx->data);
    if (out == NULL)
        return 1;
    return ctx->meth->final(ctx->data, out);
}

int EVP_MAC_ctrl(EVP_MAC_CTX *ctx, int cmd, ...)
{
    int ok = -1;
    va_list args;

    if (ctx == NULL || ctx->meth == NULL) {
        EVPerr(EVP_F_EVP_MAC_CTRL, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }

    va_start(args, cmd);
    switch (cmd) {
#if 0
    case ...:
        /* code */
        ok = 1;
        break;
#endif
    default:
        if (ctx->meth->ctrl != NULL)
            ok = ctx->meth->ctrl(ctx->data, cmd, args);
        else
            ok = -2;
        break;
    }
    va_end(args);

    if (ok == -2)
        EVPerr(EVP_F_EVP_MAC_CTRL, EVP_R_COMMAND_NOT_SUPPORTED);
    return ok;
}

int EVP_MAC_ctrl_str(EVP_MAC_CTX *ctx, const char *type, const char *value)
{
    int ok = 1;

    if (ctx == NULL || ctx->meth == NULL || ctx->meth->ctrl_str == NULL) {
        EVPerr(EVP_F_EVP_MAC_CTRL_STR, EVP_R_COMMAND_NOT_SUPPORTED);
        return -2;
    }

    ok = ctx->meth->ctrl_str(ctx->data, type, value);

    if (ok == -2)
        EVPerr(EVP_F_EVP_MAC_CTRL_STR, EVP_R_COMMAND_NOT_SUPPORTED);
    return ok;
}

int EVP_MAC_str2ctrl(EVP_MAC_CTX *ctx, int cmd, const char *value)
{
    size_t len;

    len = strlen(value);
    if (len > INT_MAX)
        return -1;
    return EVP_MAC_ctrl(ctx, cmd, value, len);
}

int EVP_MAC_hex2ctrl(EVP_MAC_CTX *ctx, int cmd, const char *hex)
{
    unsigned char *bin;
    long binlen;
    int rv = -1;

    bin = OPENSSL_hexstr2buf(hex, &binlen);
    if (bin == NULL)
        return 0;
    if (binlen <= INT_MAX)
        rv = EVP_MAC_ctrl(ctx, cmd, bin, (size_t)binlen);
    OPENSSL_free(bin);
    return rv;
}

int EVP_MAC_oneshot(const EVP_MAC *mac, const void *key, size_t keylen,
                    const unsigned char *data, size_t datalen,
                    unsigned char *out, size_t *poutlen)
{
    EVP_MAC_CTX *ctx = NULL;
    int ok = 1;

    if ((ctx = EVP_MAC_CTX_new(mac)) == NULL
        || !EVP_MAC_init(ctx)
        || !EVP_MAC_ctrl(ctx, EVP_MAC_CTRL_SET_KEY, key, keylen)
        || !EVP_MAC_update(ctx, data, datalen)
        || !EVP_MAC_final(ctx, out, poutlen))
        ok = 0;

    EVP_MAC_CTX_free(ctx);
    return ok;
}