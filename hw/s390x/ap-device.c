/*
 * Adjunct Processor (AP) matrix device
 *
 * Copyright 2017 IBM Corp.
 * Author(s): Tony Krowiak <akrowiak@linux.vnet.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or (at
 * your option) any later version. See the COPYING file in the top-level
 * directory.
 */
#include "qemu/osdep.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/qdev.h"
#include "hw/s390x/ap-device.h"

static void ap_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "AP device class";
}

static const TypeInfo ap_device_info = {
    .name = AP_DEVICE_TYPE,
    .parent = TYPE_DEVICE,
    .instance_size = sizeof(APDevice),
    .class_size = sizeof(APDeviceClass),
    .class_init = ap_class_init,
    .abstract = true,
};

static void ap_device_register(void)
{
    type_register_static(&ap_device_info);
}

type_init(ap_device_register)
