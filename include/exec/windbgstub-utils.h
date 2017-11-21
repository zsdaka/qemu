/*
 * windbgstub-utils.h
 *
 * Copyright (c) 2010-2017 Institute for System Programming
 *                         of the Russian Academy of Sciences.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#ifndef WINDBGSTUB_UTILS_H
#define WINDBGSTUB_UTILS_H

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "log.h"
#include "cpu.h"
#include "exec/windbgstub.h"
#include "exec/windbgkd.h"

#define WINDBG_DEBUG(...) do {             \
    if (WINDBG_DEBUG_ON) {                 \
        qemu_log(WINDBG ": " __VA_ARGS__); \
        qemu_log("\n");                    \
    }                                      \
} while (false)

#define WINDBG_ERROR(...) error_report(WINDBG ": " __VA_ARGS__)

#define FMT_ADDR "addr:0x" TARGET_FMT_lx
#define FMT_ERR  "Error:%d"

#define UINT8_P(ptr) ((uint8_t *) (ptr))
#define UINT32_P(ptr) ((uint32_t *) (ptr))
#define PTR(var) UINT8_P(&var)

#define sizeof_field(type, field) sizeof(((type *) NULL)->field)

#define READ_VMEM(cpu, addr, type) ({                         \
    type _t;                                                  \
    cpu_memory_rw_debug(cpu, addr, PTR(_t), sizeof(type), 0); \
    _t;                                                       \
})

#if TARGET_LONG_BITS == 64
# define sttul_p(p, v) stq_p(p, v)
# define ldtul_p(p) ldq_p(p)
#else
# define sttul_p(p, v) stl_p(p, v)
# define ldtul_p(p) ldl_p(p)
#endif

typedef struct InitedAddr {
    target_ulong addr;
    bool is_init;
} InitedAddr;

typedef struct PacketData {
    union {
        struct {
            DBGKD_MANIPULATE_STATE64 m64;
            uint8_t extra[PACKET_MAX_SIZE - sizeof(DBGKD_MANIPULATE_STATE64)];
        };
        uint8_t buf[PACKET_MAX_SIZE];
    };
    uint16_t extra_size;
} PacketData;

InitedAddr *windbg_get_KPCR(void);
InitedAddr *windbg_get_version(void);

bool windbg_on_load(void);

#endif
