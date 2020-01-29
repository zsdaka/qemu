/*
 * QEMU abstract of Hardware Dual Stage DMA translation capability
 *
 * Copyright (C) 2020 Intel Corporation.
 *
 * Authors: Liu Yi L <yi.l.liu@intel.com>
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
#include "hw/iommu/dual_stage_iommu.h"

int ds_iommu_pasid_alloc(DualStageIOMMUObject *dsi_obj, uint32_t min,
                         uint32_t max, uint32_t *pasid)
{
    if (!dsi_obj) {
        return -ENOENT;
    }

    if (dsi_obj->ops && dsi_obj->ops->pasid_alloc) {
        return dsi_obj->ops->pasid_alloc(dsi_obj, min, max, pasid);
    }
    return -ENOENT;
}

int ds_iommu_pasid_free(DualStageIOMMUObject *dsi_obj, uint32_t pasid)
{
    if (!dsi_obj) {
        return -ENOENT;
    }

    if (dsi_obj->ops && dsi_obj->ops->pasid_free) {
        return dsi_obj->ops->pasid_free(dsi_obj, pasid);
    }
    return -ENOENT;
}

int ds_iommu_bind_stage1_pgtbl(DualStageIOMMUObject *dsi_obj,
                               DualIOMMUStage1BindData *data)
{
    if (!dsi_obj) {
        return -ENOENT;
    }

    if (dsi_obj->ops && dsi_obj->ops->bind_stage1_pgtbl) {
        return dsi_obj->ops->bind_stage1_pgtbl(dsi_obj, data);
    }
    return -ENOENT;
}

int ds_iommu_unbind_stage1_pgtbl(DualStageIOMMUObject *dsi_obj,
                                 DualIOMMUStage1BindData *data)
{
    if (!dsi_obj) {
        return -ENOENT;
    }

    if (dsi_obj->ops && dsi_obj->ops->unbind_stage1_pgtbl) {
        return dsi_obj->ops->unbind_stage1_pgtbl(dsi_obj, data);
    }
    return -ENOENT;
}

void ds_iommu_object_init(DualStageIOMMUObject *dsi_obj,
                          DualStageIOMMUOps *ops,
                          DualStageIOMMUInfo *uinfo)
{
    dsi_obj->ops = ops;

    dsi_obj->uinfo.pasid_format = uinfo->pasid_format;
}

void ds_iommu_object_destroy(DualStageIOMMUObject *dsi_obj)
{
    dsi_obj->ops = NULL;
}
