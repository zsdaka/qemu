/*
 *  Copyright (C) 2016-2017 Red Hat, Inc.
 *  Copyright (C) 2005  Anthony Liguori <anthony@codemonkey.ws>
 *
 *  Network Block Device
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NBD_H
#define NBD_H

#include "qapi/qapi-types-block.h"
#include "io/channel-socket.h"
#include "crypto/tlscreds.h"

/* Handshake phase structs - this struct is passed on the wire */

struct NBDOption {
    uint64_t magic; /* NBD_OPTS_MAGIC */
    uint32_t option; /* NBD_OPT_* */
    uint32_t length;
} QEMU_PACKED;
typedef struct NBDOption NBDOption;

struct NBDOptionReply {
    uint64_t magic; /* NBD_REP_MAGIC */
    uint32_t option; /* NBD_OPT_* */
    uint32_t type; /* NBD_REP_* */
    uint32_t length;
} QEMU_PACKED;
typedef struct NBDOptionReply NBDOptionReply;

typedef struct NBDOptionReplyMetaContext {
    NBDOptionReply h; /* h.type = NBD_REP_META_CONTEXT, h.length > 4 */
    uint32_t context_id;
    /* meta context name follows */
} QEMU_PACKED NBDOptionReplyMetaContext;

/* Transmission phase structs
 *
 * Note: these are _NOT_ the same as the network representation of an NBD
 * request and reply!
 */
struct NBDRequest {
    uint64_t handle;
    uint64_t from;
    uint32_t len;
    uint16_t flags; /* NBD_CMD_FLAG_* */
    uint16_t type; /* NBD_CMD_* */
};
typedef struct NBDRequest NBDRequest;

typedef struct NBDSimpleReply {
    uint32_t magic;  /* NBD_SIMPLE_REPLY_MAGIC */
    uint32_t error;
    uint64_t handle;
} QEMU_PACKED NBDSimpleReply;

/* Header of all structured replies */
typedef struct NBDStructuredReplyChunk {
    uint32_t magic;  /* NBD_STRUCTURED_REPLY_MAGIC */
    uint16_t flags;  /* combination of NBD_REPLY_FLAG_* */
    uint16_t type;   /* NBD_REPLY_TYPE_* */
    uint64_t handle; /* request handle */
    uint32_t length; /* length of payload */
} QEMU_PACKED NBDStructuredReplyChunk;

typedef union NBDReply {
    NBDSimpleReply simple;
    NBDStructuredReplyChunk structured;
    struct {
        /* @magic and @handle fields have the same offset and size both in
         * simple reply and structured reply chunk, so let them be accessible
         * without ".simple." or ".structured." specification
         */
        uint32_t magic;
        uint32_t _skip;
        uint64_t handle;
    } QEMU_PACKED;
} NBDReply;

/* Header of chunk for NBD_REPLY_TYPE_OFFSET_DATA */
typedef struct NBDStructuredReadData {
    NBDStructuredReplyChunk h; /* h.length >= 9 */
    uint64_t offset;
    /* At least one byte of data payload follows, calculated from h.length */
} QEMU_PACKED NBDStructuredReadData;

/* Complete chunk for NBD_REPLY_TYPE_OFFSET_HOLE */
typedef struct NBDStructuredReadHole {
    NBDStructuredReplyChunk h; /* h.length == 12 */
    uint64_t offset;
    uint32_t length;
} QEMU_PACKED NBDStructuredReadHole;

/* Header of all NBD_REPLY_TYPE_ERROR* errors */
typedef struct NBDStructuredError {
    NBDStructuredReplyChunk h; /* h.length >= 6 */
    uint32_t error;
    uint16_t message_length;
} QEMU_PACKED NBDStructuredError;

/* Header of NBD_REPLY_TYPE_BLOCK_STATUS */
typedef struct NBDStructuredMeta {
    NBDStructuredReplyChunk h; /* h.length >= 12 (at least one extent) */
    uint32_t context_id;
    /* extents follows */
} QEMU_PACKED NBDStructuredMeta;

/* Extent chunk for NBD_REPLY_TYPE_BLOCK_STATUS */
typedef struct NBDExtent {
    uint32_t length;
    uint32_t flags; /* NBD_STATE_* */
} QEMU_PACKED NBDExtent;

/* Transmission (export) flags: sent from server to client during handshake,
   but describe what will happen during transmission */
#define NBD_FLAG_HAS_FLAGS         (1 << 0) /* Flags are there */
#define NBD_FLAG_READ_ONLY         (1 << 1) /* Device is read-only */
#define NBD_FLAG_SEND_FLUSH        (1 << 2) /* Send FLUSH */
#define NBD_FLAG_SEND_FUA          (1 << 3) /* Send FUA (Force Unit Access) */
#define NBD_FLAG_ROTATIONAL        (1 << 4) /* Use elevator algorithm -
                                               rotational media */
#define NBD_FLAG_SEND_TRIM         (1 << 5) /* Send TRIM (discard) */
#define NBD_FLAG_SEND_WRITE_ZEROES (1 << 6) /* Send WRITE_ZEROES */
#define NBD_FLAG_SEND_DF           (1 << 7) /* Send DF (Do not Fragment) */
#define NBD_FLAG_CAN_MULTI_CONN    (1 << 8) /* Multi-client cache consistent */
#define NBD_FLAG_SEND_RESIZE       (1 << 9) /* Send resize */
#define NBD_FLAG_SEND_CACHE        (1 << 10) /* Send CACHE (prefetch) */

/* New-style handshake (global) flags, sent from server to client, and
   control what will happen during handshake phase. */
#define NBD_FLAG_FIXED_NEWSTYLE   (1 << 0) /* Fixed newstyle protocol. */
#define NBD_FLAG_NO_ZEROES        (1 << 1) /* End handshake without zeroes. */

/* New-style client flags, sent from client to server to control what happens
   during handshake phase. */
#define NBD_FLAG_C_FIXED_NEWSTYLE (1 << 0) /* Fixed newstyle protocol. */
#define NBD_FLAG_C_NO_ZEROES      (1 << 1) /* End handshake without zeroes. */

/* Option requests. */
#define NBD_OPT_EXPORT_NAME       (1)
#define NBD_OPT_ABORT             (2)
#define NBD_OPT_LIST              (3)
/* #define NBD_OPT_PEEK_EXPORT    (4) not in use */
#define NBD_OPT_STARTTLS          (5)
#define NBD_OPT_INFO              (6)
#define NBD_OPT_GO                (7)
#define NBD_OPT_STRUCTURED_REPLY  (8)
#define NBD_OPT_LIST_META_CONTEXT (9)
#define NBD_OPT_SET_META_CONTEXT  (10)

/* Option reply types. */
#define NBD_REP_ERR(value) ((UINT32_C(1) << 31) | (value))

#define NBD_REP_ACK             (1)    /* Data sending finished. */
#define NBD_REP_SERVER          (2)    /* Export description. */
#define NBD_REP_INFO            (3)    /* NBD_OPT_INFO/GO. */
#define NBD_REP_META_CONTEXT    (4)    /* NBD_OPT_{LIST,SET}_META_CONTEXT */

#define NBD_REP_ERR_UNSUP           NBD_REP_ERR(1)  /* Unknown option */
#define NBD_REP_ERR_POLICY          NBD_REP_ERR(2)  /* Server denied */
#define NBD_REP_ERR_INVALID         NBD_REP_ERR(3)  /* Invalid length */
#define NBD_REP_ERR_PLATFORM        NBD_REP_ERR(4)  /* Not compiled in */
#define NBD_REP_ERR_TLS_REQD        NBD_REP_ERR(5)  /* TLS required */
#define NBD_REP_ERR_UNKNOWN         NBD_REP_ERR(6)  /* Export unknown */
#define NBD_REP_ERR_SHUTDOWN        NBD_REP_ERR(7)  /* Server shutting down */
#define NBD_REP_ERR_BLOCK_SIZE_REQD NBD_REP_ERR(8)  /* Need INFO_BLOCK_SIZE */

/* Info types, used during NBD_REP_INFO */
#define NBD_INFO_EXPORT         0
#define NBD_INFO_NAME           1
#define NBD_INFO_DESCRIPTION    2
#define NBD_INFO_BLOCK_SIZE     3

/* Request flags, sent from client to server during transmission phase */
#define NBD_CMD_FLAG_FUA        (1 << 0) /* 'force unit access' during write */
#define NBD_CMD_FLAG_NO_HOLE    (1 << 1) /* don't punch hole on zero run */
#define NBD_CMD_FLAG_DF         (1 << 2) /* don't fragment structured read */
#define NBD_CMD_FLAG_REQ_ONE    (1 << 3) /* only one extent in BLOCK_STATUS
                                          * reply chunk */

/* Supported request types */
enum {
    NBD_CMD_READ = 0,
    NBD_CMD_WRITE = 1,
    NBD_CMD_DISC = 2,
    NBD_CMD_FLUSH = 3,
    NBD_CMD_TRIM = 4,
    NBD_CMD_CACHE = 5,
    NBD_CMD_WRITE_ZEROES = 6,
    NBD_CMD_BLOCK_STATUS = 7,
};

#define NBD_DEFAULT_PORT	10809

/* Maximum size of a single READ/WRITE data buffer */
#define NBD_MAX_BUFFER_SIZE (32 * 1024 * 1024)

/* Maximum size of an export name. The NBD spec requires 256 and
 * suggests that servers support up to 4096, but we stick to only the
 * required size so that we can stack-allocate the names, and because
 * going larger would require an audit of more code to make sure we
 * aren't overflowing some other buffer. */
#define NBD_MAX_NAME_SIZE 256

/* Two types of reply structures */
#define NBD_SIMPLE_REPLY_MAGIC      0x67446698
#define NBD_STRUCTURED_REPLY_MAGIC  0x668e33ef

/* Structured reply flags */
#define NBD_REPLY_FLAG_DONE          (1 << 0) /* This reply-chunk is last */

/* Structured reply types */
#define NBD_REPLY_ERR(value)         ((1 << 15) | (value))

#define NBD_REPLY_TYPE_NONE          0
#define NBD_REPLY_TYPE_OFFSET_DATA   1
#define NBD_REPLY_TYPE_OFFSET_HOLE   2
#define NBD_REPLY_TYPE_BLOCK_STATUS  5
#define NBD_REPLY_TYPE_ERROR         NBD_REPLY_ERR(1)
#define NBD_REPLY_TYPE_ERROR_OFFSET  NBD_REPLY_ERR(2)

/* Extent flags for base:allocation in NBD_REPLY_TYPE_BLOCK_STATUS */
#define NBD_STATE_HOLE (1 << 0)
#define NBD_STATE_ZERO (1 << 1)

/* Extent flags for qemu:dirty-bitmap in NBD_REPLY_TYPE_BLOCK_STATUS */
#define NBD_STATE_DIRTY (1 << 0)

static inline bool nbd_reply_type_is_error(int type)
{
    return type & (1 << 15);
}

/* NBD errors are based on errno numbers, so there is a 1:1 mapping,
 * but only a limited set of errno values is specified in the protocol.
 * Everything else is squashed to EINVAL.
 */
#define NBD_SUCCESS    0
#define NBD_EPERM      1
#define NBD_EIO        5
#define NBD_ENOMEM     12
#define NBD_EINVAL     22
#define NBD_ENOSPC     28
#define NBD_EOVERFLOW  75
#define NBD_ESHUTDOWN  108

/* Details collected by NBD_OPT_EXPORT_NAME and NBD_OPT_GO */
struct NBDExportInfo {
    /* Set by client before nbd_receive_negotiate() */
    bool request_sizes;
    char *x_dirty_bitmap;
    char *name; /* must be non-NULL */

    /* In-out fields, set by client before nbd_receive_negotiate() and
     * updated by server results during nbd_receive_negotiate() */
    bool structured_reply;
    bool base_allocation; /* base:allocation context for NBD_CMD_BLOCK_STATUS */

    /* Set by server results during nbd_receive_negotiate() */
    uint64_t size;
    uint16_t flags;
    uint32_t min_block;
    uint32_t opt_block;
    uint32_t max_block;

    uint32_t meta_base_allocation_id;
};
typedef struct NBDExportInfo NBDExportInfo;

int nbd_receive_negotiate(QIOChannel *ioc, QCryptoTLSCreds *tlscreds,
                          const char *hostname, QIOChannel **outioc,
                          NBDExportInfo *info, Error **errp);
int nbd_init(int fd, QIOChannelSocket *sioc, NBDExportInfo *info,
             Error **errp);
int nbd_send_request(QIOChannel *ioc, NBDRequest *request);
int nbd_receive_reply(QIOChannel *ioc, NBDReply *reply, Error **errp);
int nbd_client(int fd);
int nbd_disconnect(int fd);
int nbd_errno_to_system_errno(int err);

typedef struct NBDExport NBDExport;
typedef struct NBDClient NBDClient;

NBDExport *nbd_export_new(BlockDriverState *bs, uint64_t dev_offset,
                          uint64_t size, const char *name, const char *desc,
                          const char *bitmap, uint16_t nbdflags,
                          void (*close)(NBDExport *), bool writethrough,
                          BlockBackend *on_eject_blk, Error **errp);
void nbd_export_close(NBDExport *exp);
void nbd_export_remove(NBDExport *exp, NbdServerRemoveMode mode, Error **errp);
void nbd_export_get(NBDExport *exp);
void nbd_export_put(NBDExport *exp);

BlockBackend *nbd_export_get_blockdev(NBDExport *exp);

NBDExport *nbd_export_find(const char *name);
void nbd_export_close_all(void);

void nbd_client_new(QIOChannelSocket *sioc,
                    QCryptoTLSCreds *tlscreds,
                    const char *tlsaclname,
                    void (*close_fn)(NBDClient *, bool));
void nbd_client_get(NBDClient *client);
void nbd_client_put(NBDClient *client);

void nbd_server_start(SocketAddress *addr, const char *tls_creds,
                      Error **errp);

/* nbd_read
 * Reads @size bytes from @ioc. Returns 0 on success.
 */
static inline int nbd_read(QIOChannel *ioc, void *buffer, size_t size,
                           Error **errp)
{
    return qio_channel_read_all(ioc, buffer, size, errp) < 0 ? -EIO : 0;
}

static inline bool nbd_reply_is_simple(NBDReply *reply)
{
    return reply->magic == NBD_SIMPLE_REPLY_MAGIC;
}

static inline bool nbd_reply_is_structured(NBDReply *reply)
{
    return reply->magic == NBD_STRUCTURED_REPLY_MAGIC;
}

const char *nbd_reply_type_lookup(uint16_t type);
const char *nbd_opt_lookup(uint32_t opt);
const char *nbd_rep_lookup(uint32_t rep);
const char *nbd_info_lookup(uint16_t info);
const char *nbd_cmd_lookup(uint16_t info);
const char *nbd_err_lookup(int err);

#endif
