/*
 * QLit unit-tests.
 *
 * Copyright (C) 2017 Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include "qemu/osdep.h"

#include "qapi/qmp/qbool.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qlit.h"
#include "qapi/qmp/qnum.h"
#include "qapi/qmp/qstring.h"

static QLitObject qlit = QLIT_QDICT(((QLitDictEntry[]) {
    { "foo", QLIT_QNUM(42) },
    { "bar", QLIT_QSTR("hello world") },
    { "baz", QLIT_QNULL },
    { "bee", QLIT_QLIST(((QLitObject[]) {
        QLIT_QNUM(43),
        QLIT_QNUM(44),
        QLIT_QBOOL(true),
        { },
    }))},
    { },
}));

static QObject *make_qobject(void)
{
    QDict *qdict = qdict_new();
    QList *list = qlist_new();

    qdict_put_int(qdict, "foo", 42);
    qdict_put_str(qdict, "bar", "hello world");
    qdict_put_null(qdict, "baz");

    qlist_append_int(list, 43);
    qlist_append_int(list, 44);
    qlist_append_bool(list, true);
    qdict_put(qdict, "bee", list);

    return QOBJECT(qdict);
}

static void qlit_equal_qobject_test(void)
{
    QObject *qobj = make_qobject();

    g_assert(qlit_equal_qobject(&qlit, qobj));

    qobject_decref(qobj);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/qlit/equal_qobject", qlit_equal_qobject_test);

    return g_test_run();
}
