/*
 * Threaded data processing for Qcow2: compression, encryption
 *
 * Copyright (c) 2004-2006 Fabrice Bellard
 * Copyright (c) 2018 Virtuozzo International GmbH. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"

#define ZLIB_CONST
#include <zlib.h>

#ifdef CONFIG_ZSTD
#include <zstd.h>
#include <zstd_errors.h>
#endif

#include "qcow2.h"
#include "block/thread-pool.h"
#include "crypto.h"

static int coroutine_fn
qcow2_co_process(BlockDriverState *bs, ThreadPoolFunc *func, void *arg)
{
    int ret;
    BDRVQcow2State *s = bs->opaque;
    ThreadPool *pool = aio_get_thread_pool(bdrv_get_aio_context(bs));

    qemu_co_mutex_lock(&s->lock);
    while (s->nb_threads >= QCOW2_MAX_THREADS) {
        qemu_co_queue_wait(&s->thread_task_queue, &s->lock);
    }
    s->nb_threads++;
    qemu_co_mutex_unlock(&s->lock);

    ret = thread_pool_submit_co(pool, func, arg);

    qemu_co_mutex_lock(&s->lock);
    s->nb_threads--;
    qemu_co_queue_next(&s->thread_task_queue);
    qemu_co_mutex_unlock(&s->lock);

    return ret;
}


/*
 * Compression
 */

typedef ssize_t (*Qcow2CompressFunc)(void *dest, size_t dest_size,
                                     const void *src, size_t src_size);
typedef struct Qcow2CompressData {
    void *dest;
    size_t dest_size;
    const void *src;
    size_t src_size;
    ssize_t ret;

    Qcow2CompressFunc func;
} Qcow2CompressData;

/*
 * qcow2_zlib_compress()
 *
 * Compress @src_size bytes of data using zlib compression method
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: compressed size on success
 *          -ENOMEM destination buffer is not enough to store compressed data
 *          -EIO    on any other error
 */
static ssize_t qcow2_zlib_compress(void *dest, size_t dest_size,
                                   const void *src, size_t src_size)
{
    ssize_t ret;
    z_stream strm;

    /* best compression, small window, no zlib header */
    memset(&strm, 0, sizeof(strm));
    ret = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                       -12, 9, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        return -EIO;
    }

    /*
     * strm.next_in is not const in old zlib versions, such as those used on
     * OpenBSD/NetBSD, so cast the const away
     */
    strm.avail_in = src_size;
    strm.next_in = (void *) src;
    strm.avail_out = dest_size;
    strm.next_out = dest;

    ret = deflate(&strm, Z_FINISH);
    if (ret == Z_STREAM_END) {
        ret = dest_size - strm.avail_out;
    } else {
        ret = (ret == Z_OK ? -ENOMEM : -EIO);
    }

    deflateEnd(&strm);

    return ret;
}

/*
 * qcow2_zlib_decompress()
 *
 * Decompress some data (not more than @src_size bytes) to produce exactly
 * @dest_size bytes using zlib compression method
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: 0 on success
 *          -EIO on failure
 */
static ssize_t qcow2_zlib_decompress(void *dest, size_t dest_size,
                                     const void *src, size_t src_size)
{
    int ret = 0;
    z_stream strm;

    memset(&strm, 0, sizeof(strm));
    strm.avail_in = src_size;
    strm.next_in = (void *) src;
    strm.avail_out = dest_size;
    strm.next_out = dest;

    ret = inflateInit2(&strm, -12);
    if (ret != Z_OK) {
        return -EIO;
    }

    ret = inflate(&strm, Z_FINISH);
    if ((ret != Z_STREAM_END && ret != Z_BUF_ERROR) || strm.avail_out != 0) {
        /*
         * We approve Z_BUF_ERROR because we need @dest buffer to be filled, but
         * @src buffer may be processed partly (because in qcow2 we know size of
         * compressed data with precision of one sector)
         */
        ret = -EIO;
    }

    inflateEnd(&strm);

    return ret;
}

#ifdef CONFIG_ZSTD
/*
 * qcow2_zstd_compress()
 *
 * Compress @src_size bytes of data using zstd compression method
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: compressed size on success
 *          -ENOMEM destination buffer is not enough to store compressed data
 *          -EIO    on any other error
 */

static ssize_t qcow2_zstd_compress(void *dest, size_t dest_size,
                                   const void *src, size_t src_size)
{
    ssize_t ret;
    uint32_t *c_size = dest;
    /* steal some bytes to store compressed chunk size */
    char *d_buf = ((char *) dest) + sizeof(*c_size);

    /* sanity check that we can store the compressed data length */
    if (dest_size < sizeof(*c_size)) {
        return -ENOMEM;
    }

    dest_size -= sizeof(*c_size);

    ret = ZSTD_compress(d_buf, dest_size, src, src_size, 5);

    if (ZSTD_isError(ret)) {
        if (ZSTD_getErrorCode(ret) == ZSTD_error_dstSize_tooSmall) {
            return -ENOMEM;
        } else {
            return -EIO;
        }
    }

    /* store the compressed chunk size in the very beginning of the buffer */
    *c_size = cpu_to_be32(ret);

    return ret + sizeof(*c_size);
}

/*
 * qcow2_zstd_decompress()
 *
 * Decompress some data (not more than @src_size bytes) to produce exactly
 * @dest_size bytes using zstd compression method
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: 0 on success
 *          -EIO on any error
 */

static ssize_t qcow2_zstd_decompress(void *dest, size_t dest_size,
                                     const void *src, size_t src_size)
{
    ssize_t ret;
    /*
     * zstd decompress wants to know the exact length of the data
     * for that purpose, on the compression the length is stored in
     * the very beginning of the compressed buffer
     */
    uint32_t s_size;
    const char *s_buf = ((const char *) src) + sizeof(s_size);

    /* sanity check that we can read the content length */
    if (src_size < sizeof(s_size)) {
        return -EIO;
    }

    s_size = be32_to_cpu(*(const uint32_t *) src);

    /* sanity check that the buffer is big enough to read the content */
    if (src_size - sizeof(s_size) < s_size) {
        return -EIO;
    }

    ret = ZSTD_decompress(dest, dest_size, s_buf, s_size);

    if (ZSTD_isError(ret)) {
        return -EIO;
    }

    return 0;
}
#endif

static int qcow2_compress_pool_func(void *opaque)
{
    Qcow2CompressData *data = opaque;

    data->ret = data->func(data->dest, data->dest_size,
                           data->src, data->src_size);

    return 0;
}

static ssize_t coroutine_fn
qcow2_co_do_compress(BlockDriverState *bs, void *dest, size_t dest_size,
                     const void *src, size_t src_size, Qcow2CompressFunc func)
{
    Qcow2CompressData arg = {
        .dest = dest,
        .dest_size = dest_size,
        .src = src,
        .src_size = src_size,
        .func = func,
    };

    qcow2_co_process(bs, qcow2_compress_pool_func, &arg);

    return arg.ret;
}

/*
 * qcow2_co_compress()
 *
 * Compress @src_size bytes of data using the compression
 * method defined by the image compression type
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: 0 on success
 *          a negative error code on failure
 */
ssize_t coroutine_fn
qcow2_co_compress(BlockDriverState *bs, void *dest, size_t dest_size,
                  const void *src, size_t src_size)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2CompressFunc fn;

    switch (s->compression_type) {
    case QCOW2_COMPRESSION_TYPE_ZLIB:
        fn = qcow2_zlib_compress;
        break;

#ifdef CONFIG_ZSTD
    case QCOW2_COMPRESSION_TYPE_ZSTD:
        fn = qcow2_zstd_compress;
        break;
#endif
    default:
        return -ENOTSUP;
    }

    return qcow2_co_do_compress(bs, dest, dest_size, src, src_size, fn);
}

/*
 * qcow2_co_decompress()
 *
 * Decompress some data (not more than @src_size bytes) to produce exactly
 * @dest_size bytes using the compression method defined by the image
 * compression type
 *
 * @dest - destination buffer, @dest_size bytes
 * @src - source buffer, @src_size bytes
 *
 * Returns: 0 on success
 *          a negative error code on failure
 */
ssize_t coroutine_fn
qcow2_co_decompress(BlockDriverState *bs, void *dest, size_t dest_size,
                    const void *src, size_t src_size)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2CompressFunc fn;

    switch (s->compression_type) {
    case QCOW2_COMPRESSION_TYPE_ZLIB:
        fn = qcow2_zlib_decompress;
        break;

#ifdef CONFIG_ZSTD
    case QCOW2_COMPRESSION_TYPE_ZSTD:
        fn = qcow2_zstd_decompress;
        break;
#endif
    default:
        return -ENOTSUP;
    }

    return qcow2_co_do_compress(bs, dest, dest_size, src, src_size, fn);
}


/*
 * Cryptography
 */

/*
 * Qcow2EncDecFunc: common prototype of qcrypto_block_encrypt() and
 * qcrypto_block_decrypt() functions.
 */
typedef int (*Qcow2EncDecFunc)(QCryptoBlock *block, uint64_t offset,
                               uint8_t *buf, size_t len, Error **errp);

typedef struct Qcow2EncDecData {
    QCryptoBlock *block;
    uint64_t offset;
    uint8_t *buf;
    size_t len;

    Qcow2EncDecFunc func;
} Qcow2EncDecData;

static int qcow2_encdec_pool_func(void *opaque)
{
    Qcow2EncDecData *data = opaque;

    return data->func(data->block, data->offset, data->buf, data->len, NULL);
}

static int coroutine_fn
qcow2_co_encdec(BlockDriverState *bs, uint64_t file_cluster_offset,
                  uint64_t offset, void *buf, size_t len, Qcow2EncDecFunc func)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2EncDecData arg = {
        .block = s->crypto,
        .offset = s->crypt_physical_offset ?
                      file_cluster_offset + offset_into_cluster(s, offset) :
                      offset,
        .buf = buf,
        .len = len,
        .func = func,
    };

    return qcow2_co_process(bs, qcow2_encdec_pool_func, &arg);
}

int coroutine_fn
qcow2_co_encrypt(BlockDriverState *bs, uint64_t file_cluster_offset,
                 uint64_t offset, void *buf, size_t len)
{
    return qcow2_co_encdec(bs, file_cluster_offset, offset, buf, len,
                             qcrypto_block_encrypt);
}

int coroutine_fn
qcow2_co_decrypt(BlockDriverState *bs, uint64_t file_cluster_offset,
                 uint64_t offset, void *buf, size_t len)
{
    return qcow2_co_encdec(bs, file_cluster_offset, offset, buf, len,
                             qcrypto_block_decrypt);
}
