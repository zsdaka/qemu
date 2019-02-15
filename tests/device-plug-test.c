/*
 * QEMU device plug/unplug handling
 *
 * Copyright (C) 2019 Red Hat Inc.
 *
 * Authors:
 *  David Hildenbrand <david@redhat.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "libqtest.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qstring.h"

static void device_del_start(QTestState *qtest, const char *id)
{
    qtest_qmp_send(qtest,
                   "{'execute': 'device_del', 'arguments': { 'id': %s } }", id);
}

static void device_del_finish(QTestState *qtest)
{
    QDict *resp = qtest_qmp_receive(qtest);

    g_assert(qdict_haskey(resp, "return"));
    qobject_unref(resp);
}

static void device_del_request(QTestState *qtest, const char *id)
{
    device_del_start(qtest, id);
    device_del_finish(qtest);
}

static void system_reset(QTestState *qtest)
{
    QDict *resp;

    resp = qtest_qmp(qtest, "{'execute': 'system_reset'}");
    g_assert(qdict_haskey(resp, "return"));
    qobject_unref(resp);
}

static void wait_device_deleted_event(QTestState *qtest, const char *id)
{
    QDict *resp, *data;
    QString *qstr;

    /*
     * Other devices might get removed along with the removed device. Skip
     * these. The device of interest will be the last one.
     */
    for (;;) {
        resp = qtest_qmp_eventwait_ref(qtest, "DEVICE_DELETED");
        data = qdict_get_qdict(resp, "data");
        if (!data || !qdict_get(data, "device")) {
            qobject_unref(resp);
            continue;
        }
        qstr = qobject_to(QString, qdict_get(data, "device"));
        g_assert(qstr);
        if (!strcmp(qstring_get_str(qstr), id)) {
            qobject_unref(resp);
            break;
        }
        qobject_unref(resp);
    }
}

static void test_pci_unplug_request(void)
{
    QTestState *qtest = qtest_initf("-device virtio-mouse-pci,id=dev0");

    /*
     * Request device removal. As the guest is not running, the request won't
     * be processed. However during system reset, the removal will be
     * handled, removing the device.
     */
    device_del_request(qtest, "dev0");
    system_reset(qtest);
    wait_device_deleted_event(qtest, "dev0");

    qtest_quit(qtest);
}

static void test_ccw_unplug(void)
{
    QTestState *qtest = qtest_initf("-device virtio-balloon-ccw,id=dev0");

    /*
     * The DEVICE_DELETED events will be sent before the command
     * completes.
     */
    device_del_start(qtest, "dev0");
    wait_device_deleted_event(qtest, "dev0");
    device_del_finish(qtest);

    qtest_quit(qtest);
}

static void test_spapr_cpu_unplug_request(void)
{
    QTestState *qtest;

    qtest = qtest_initf("-cpu power9_v2.0 -smp 1,maxcpus=2 "
                        "-device power9_v2.0-spapr-cpu-core,core-id=1,id=dev0");

    /* similar to test_pci_unplug_request */
    device_del_request(qtest, "dev0");
    system_reset(qtest);
    wait_device_deleted_event(qtest, "dev0");

    qtest_quit(qtest);
}

static void test_spapr_memory_unplug_request(void)
{
    QTestState *qtest;

    qtest = qtest_initf("-m 1G,slots=1,maxmem=2G "
                        "-object memory-backend-ram,id=mem0,size=1G "
                        "-device pc-dimm,id=dev0,memdev=mem0");

    /* similar to test_pci_unplug_request */
    device_del_request(qtest, "dev0");
    system_reset(qtest);
    wait_device_deleted_event(qtest, "dev0");

    qtest_quit(qtest);
}

int main(int argc, char **argv)
{
    const char *arch = qtest_get_arch();

    g_test_init(&argc, &argv, NULL);

    /*
     * We need a system that will process unplug requests during system resets
     * and does not do PCI surprise removal. This holds for x86 ACPI,
     * s390x and spapr.
     */
    qtest_add_func("/device-plug/pci_unplug_request",
                   test_pci_unplug_request);

    if (!strcmp(arch, "s390x")) {
        qtest_add_func("/device-plug/ccw_unplug",
                       test_ccw_unplug);
    }

    if (!strcmp(arch, "ppc64")) {
        qtest_add_func("/device-plug/spapr_cpu_unplug_request",
                       test_spapr_cpu_unplug_request);
    }

    if (!strcmp(arch, "ppc64")) {
        qtest_add_func("/device-plug/spapr_memory_unplug_request",
                       test_spapr_memory_unplug_request);
    }

    return g_test_run();
}
