/*
 * Virtio pmem Device
 *
 * Copyright Red Hat, Inc. 2018
 * Copyright Pankaj Gupta <pagupta@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.  See the COPYING file in the
 * top-level directory.
 */

#ifndef QEMU_VIRTIO_PMEM_H
#define QEMU_VIRTIO_PMEM_H

#include "hw/virtio/virtio.h"
#include "sysemu/hostmem.h"
#include "standard-headers/linux/virtio_ids.h"

#define TYPE_VIRTIO_PMEM "virtio-pmem"

#define VIRTIO_PMEM(obj) \
        OBJECT_CHECK(VirtIOPMEM, (obj), TYPE_VIRTIO_PMEM)

#define VIRTIO_PMEM_ADDR_PROP "memaddr"
#define VIRTIO_PMEM_MEMDEV_PROP "memdev"

/* VirtIOPMEM device structure */
typedef struct VirtIOPMEM {
    VirtIODevice parent_obj;

    VirtQueue *rq_vq;
    uint64_t start;
    HostMemoryBackend *memdev;
} VirtIOPMEM;

struct virtio_pmem_config {
    uint64_t start;
    uint64_t size;
};
#endif
