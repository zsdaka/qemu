/*
 * Utilities for generating ACPI tables and passing them to Guests
 *
 * Copyright (C) 2019 Intel Corporation
 * Copyright (C) 2019 Red Hat Inc
 *
 * Author: Wei Yang <richardw.yang@linux.intel.com>
 * Author: Igor Mammedov <imammedo@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include <glib/gprintf.h>
#include "hw/acpi/aml-build.h"
#include "hw/acpi/utils.h"

MemoryRegion *acpi_add_rom_blob(FWCfgCallback update, void *opaque,
                                GArray *blob, const char *name,
                                uint64_t max_size)
{
    return rom_add_blob(name, blob->data, acpi_data_len(blob), max_size, -1,
                        name, update, opaque, NULL, true);
}

