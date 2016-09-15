/*
 * QEMU PowerPC PowerNV various definitions
 *
 * Copyright (c) 2014-2016 BenH, IBM Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _PPC_PNV_H
#define _PPC_PNV_H

#include "hw/boards.h"
#include "hw/sysbus.h"
#include "hw/ppc/pnv_xscom.h"
#include "hw/ppc/pnv_lpc.h"

#define TYPE_PNV_CHIP "powernv-chip"
#define PNV_CHIP(obj) OBJECT_CHECK(PnvChip, (obj), TYPE_PNV_CHIP)
#define PNV_CHIP_CLASS(klass) \
     OBJECT_CLASS_CHECK(PnvChipClass, (klass), TYPE_PNV_CHIP)
#define PNV_CHIP_GET_CLASS(obj) \
     OBJECT_GET_CLASS(PnvChipClass, (obj), TYPE_PNV_CHIP)

typedef enum PnvChipType {
    PNV_CHIP_POWER8E,     /* AKA Murano (default) */
    PNV_CHIP_POWER8,      /* AKA Venice */
    PNV_CHIP_POWER8NVL,   /* AKA Naples */
    PNV_CHIP_POWER9,      /* AKA Nimbus */
} PnvChipType;

typedef struct PnvChip {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    uint32_t     chip_id;
    PnvXScom     xscom;

    uint32_t  nr_cores;
    uint64_t  cores_mask;
    void      *cores;

    PnvLpcController lpc;
} PnvChip;

typedef struct PnvChipClass {
    /*< private >*/
    SysBusDeviceClass parent_class;

    /*< public >*/
    const char *cpu_model;
    PnvChipType  chip_type;
    uint64_t     chip_cfam_id;
    uint64_t     cores_mask;

    void (*realize)(PnvChip *dev, Error **errp);
    uint32_t (*core_pir)(PnvChip *chip, uint32_t core_id);
    uint64_t (*xscom_addr)(uint32_t pcba);
    uint32_t (*xscom_pcba)(uint64_t addr);
} PnvChipClass;

#define TYPE_PNV_CHIP_POWER8E TYPE_PNV_CHIP "-POWER8E"
#define PNV_CHIP_POWER8E(obj) \
    OBJECT_CHECK(PnvChipPower8E, (obj), TYPE_PNV_CHIP_POWER8E)

typedef struct PnvChipPower8E {
    PnvChip pnv_chip;
} PnvChipPower8E;

#define TYPE_PNV_CHIP_POWER8 TYPE_PNV_CHIP "-POWER8"
#define PNV_CHIP_POWER8(obj) \
    OBJECT_CHECK(PnvChipPower8, (obj), TYPE_PNV_CHIP_POWER8)

typedef struct PnvChipPower8 {
    PnvChip pnv_chip;
} PnvChipPower8;

#define TYPE_PNV_CHIP_POWER8NVL TYPE_PNV_CHIP "-POWER8NVL"
#define PNV_CHIP_POWER8NVL(obj) \
    OBJECT_CHECK(PnvChipPower8NVL, (obj), TYPE_PNV_CHIP_POWER8NVL)

typedef struct PnvChipPower8NVL {
    PnvChip pnv_chip;
} PnvChipPower8NVL;

#define TYPE_PNV_CHIP_POWER9 TYPE_PNV_CHIP "-POWER9"
#define PNV_CHIP_POWER9(obj) \
    OBJECT_CHECK(PnvChipPower9, (obj), TYPE_PNV_CHIP_POWER9)

typedef struct PnvChipPower9 {
    PnvChip pnv_chip;
} PnvChipPower9;

/*
 * This generates a HW chip id depending on an index:
 *
 *    0x0, 0x1, 0x10, 0x11, 0x20, 0x21, ...
 *
 * Is this correct ?
 */
#define CHIP_HWID(i) ((((i) & 0x3e) << 3) | ((i) & 0x1))

#define TYPE_POWERNV_MACHINE       MACHINE_TYPE_NAME("powernv")
#define POWERNV_MACHINE(obj) \
    OBJECT_CHECK(PnvMachineState, (obj), TYPE_POWERNV_MACHINE)

typedef struct PnvMachineState {
    /*< private >*/
    MachineState parent_obj;

    uint32_t initrd_base;
    long initrd_size;

    uint32_t  num_chips;
    PnvChip   **chips;
} PnvMachineState;

#define POWERNV_FDT_ADDR                0x01000000

#define PNV_TIMEBASE_FREQ           512000000ULL

#endif /* _PPC_PNV_H */
