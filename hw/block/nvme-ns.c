#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qemu/log.h"
#include "hw/block/block.h"
#include "hw/pci/msix.h"
#include "sysemu/sysemu.h"
#include "sysemu/block-backend.h"
#include "qapi/error.h"

#include "hw/qdev-properties.h"
#include "hw/qdev-core.h"

#include "nvme.h"
#include "nvme-ns.h"

static int nvme_ns_init(NvmeNamespace *ns)
{
    NvmeIdNs *id_ns = &ns->id_ns;

    id_ns->lbaf[0].ds = BDRV_SECTOR_BITS;
    id_ns->nuse = id_ns->ncap = id_ns->nsze =
        cpu_to_le64(nvme_ns_nlbas(ns));

    return 0;
}

static int nvme_ns_init_blk(NvmeCtrl *n, NvmeNamespace *ns, NvmeIdCtrl *id,
    Error **errp)
{
    uint64_t perm, shared_perm;

    Error *local_err = NULL;
    int ret;

    perm = BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE;
    shared_perm = BLK_PERM_CONSISTENT_READ | BLK_PERM_WRITE_UNCHANGED |
        BLK_PERM_GRAPH_MOD;

    ret = blk_set_perm(ns->blk, perm, shared_perm, &local_err);
    if (ret) {
        error_propagate_prepend(errp, local_err, "blk_set_perm: ");
        return ret;
    }

    ns->size = blk_getlength(ns->blk);
    if (ns->size < 0) {
        error_setg_errno(errp, -ns->size, "blk_getlength");
        return 1;
    }

    switch (n->conf.wce) {
    case ON_OFF_AUTO_ON:
        n->features.volatile_wc = 1;
        break;
    case ON_OFF_AUTO_OFF:
        n->features.volatile_wc = 0;
    case ON_OFF_AUTO_AUTO:
        n->features.volatile_wc = blk_enable_write_cache(ns->blk);
        break;
    default:
        abort();
    }

    blk_set_enable_write_cache(ns->blk, n->features.volatile_wc);

    return 0;
}

static int nvme_ns_check_constraints(NvmeNamespace *ns, Error **errp)
{
    if (!ns->blk) {
        error_setg(errp, "block backend not configured");
        return 1;
    }

    return 0;
}

int nvme_ns_setup(NvmeCtrl *n, NvmeNamespace *ns, Error **errp)
{
    Error *local_err = NULL;

    if (nvme_ns_check_constraints(ns, &local_err)) {
        error_propagate_prepend(errp, local_err,
            "nvme_ns_check_constraints: ");
        return 1;
    }

    if (nvme_ns_init_blk(n, ns, &n->id_ctrl, &local_err)) {
        error_propagate_prepend(errp, local_err, "nvme_ns_init_blk: ");
        return 1;
    }

    nvme_ns_init(ns);
    if (nvme_register_namespace(n, ns, &local_err)) {
        error_propagate_prepend(errp, local_err, "nvme_register_namespace: ");
        return 1;
    }

    return 0;
}

static void nvme_ns_realize(DeviceState *dev, Error **errp)
{
    NvmeNamespace *ns = NVME_NS(dev);
    BusState *s = qdev_get_parent_bus(dev);
    NvmeCtrl *n = NVME(s->parent);
    Error *local_err = NULL;

    if (nvme_ns_setup(n, ns, &local_err)) {
        error_propagate_prepend(errp, local_err, "nvme_ns_setup: ");
        return;
    }
}

static Property nvme_ns_props[] = {
    DEFINE_NVME_NS_PROPERTIES(NvmeNamespace, params),
    DEFINE_PROP_END_OF_LIST(),
};

static void nvme_ns_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    set_bit(DEVICE_CATEGORY_STORAGE, dc->categories);

    dc->bus_type = TYPE_NVME_BUS;
    dc->realize = nvme_ns_realize;
    device_class_set_props(dc, nvme_ns_props);
    dc->desc = "virtual nvme namespace";
}

static void nvme_ns_instance_init(Object *obj)
{
    NvmeNamespace *ns = NVME_NS(obj);
    char *bootindex = g_strdup_printf("/namespace@%d,0", ns->params.nsid);

    device_add_bootindex_property(obj, &ns->bootindex, "bootindex",
        bootindex, DEVICE(obj), &error_abort);

    g_free(bootindex);
}

static const TypeInfo nvme_ns_info = {
    .name = TYPE_NVME_NS,
    .parent = TYPE_DEVICE,
    .class_init = nvme_ns_class_init,
    .instance_size = sizeof(NvmeNamespace),
    .instance_init = nvme_ns_instance_init,
};

static void nvme_ns_register_types(void)
{
    type_register_static(&nvme_ns_info);
}

type_init(nvme_ns_register_types)
