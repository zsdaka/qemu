/*
 * Generic QObject unit-tests.
 *
 * Copyright (C) 2017 Red Hat Inc.
 *
 * This work is licensed under the terms of the GNU LGPL, version 2.1 or later.
 * See the COPYING.LIB file in the top-level directory.
 */
#include "qemu/osdep.h"

#include "qapi/qmp/types.h"
#include "qemu-common.h"

/* Marks the end of the test_equality() argument list.
 * We cannot use NULL there because that is a valid argument. */
static QObject _test_equality_end_of_arguments;

/**
 * Test whether all variadic QObject *arguments are equal (@expected
 * is true) or whether they are all not equal (@expected is false).
 * Every QObject is tested to be equal to itself (to test
 * reflexivity), all tests are done both ways (to test symmetry), and
 * transitivity is not assumed but checked (each object is compared to
 * every other one).
 *
 * Note that qobject_is_equal() is not really an equivalence relation,
 * so this function may not be used for all objects (reflexivity is
 * not guaranteed, e.g. in the case of a QNum containing NaN).
 */
static void do_test_equality(bool expected, ...)
{
    va_list ap_count, ap_extract;
    QObject **args;
    int arg_count = 0;
    int i, j;

    va_start(ap_count, expected);
    va_copy(ap_extract, ap_count);
    while (va_arg(ap_count, QObject *) != &_test_equality_end_of_arguments) {
        arg_count++;
    }
    va_end(ap_count);

    args = g_new(QObject *, arg_count);
    for (i = 0; i < arg_count; i++) {
        args[i] = va_arg(ap_extract, QObject *);
    }
    va_end(ap_extract);

    for (i = 0; i < arg_count; i++) {
        g_assert(qobject_is_equal(args[i], args[i]) == true);

        for (j = i + 1; j < arg_count; j++) {
            g_assert(qobject_is_equal(args[i], args[j]) == expected);
        }
    }
}

#define test_equality(expected, ...) \
    do_test_equality(expected, __VA_ARGS__, &_test_equality_end_of_arguments)

static void do_free_all(int _, ...)
{
    va_list ap;
    QObject *obj;

    va_start(ap, _);
    while ((obj = va_arg(ap, QObject *)) != NULL) {
        qobject_decref(obj);
    }
    va_end(ap);
}

#define free_all(...) \
    do_free_all(0, __VA_ARGS__, NULL)

static void qobject_is_equal_null_test(void)
{
    test_equality(false, qnull(), NULL);
}

static void qobject_is_equal_num_test(void)
{
    QNum *u0, *i0, *d0, *d0p25, *dnan, *um42, *im42, *dm42;
    QNum *umax, *imax, *umax_exact, *umax_exact_p1;
    QNum *dumax, *dimax, *dumax_exact, *dumax_exact_p1;
    QString *s0, *s_empty;
    QBool *bfalse;

    u0 = qnum_from_uint(0u);
    i0 = qnum_from_int(0);
    d0 = qnum_from_double(0.0);
    d0p25 = qnum_from_double(0.25);
    dnan = qnum_from_double(0.0 / 0.0);
    um42 = qnum_from_uint((uint64_t)-42);
    im42 = qnum_from_int(-42);
    dm42 = qnum_from_int(-42.0);

    /* 2^64 - 1: Not exactly representable as a double (needs 64 bits
     * of precision, but double only has 53).  The double equivalent
     * may be either 2^64 or 2^64 - 2^11. */
    umax = qnum_from_uint(UINT64_MAX);

    /* 2^63 - 1: Not exactly representable as a double (needs 63 bits
     * of precision, but double only has 53).  The double equivalent
     * may be either 2^63 or 2^63 - 2^10. */
    imax = qnum_from_int(INT64_MAX);
    /* 2^64 - 2^11: Exactly representable as a double (the least
     * significant 11 bits are set to 0, so we only need the 53 bits
     * of precision double offers).  This is the maximum value which
     * is exactly representable both as a uint64_t and a double. */
    umax_exact = qnum_from_uint(UINT64_MAX - 0x7ff);

    /* 2^64 - 2^11 + 1: Not exactly representable as a double (needs
     * 64 bits again), but whereas (double)UINT64_MAX may be rounded
     * up to 2^64, this will most likely be rounded down to
     * 2^64 - 2^11. */
    umax_exact_p1 = qnum_from_uint(UINT64_MAX - 0x7ff + 1);

    dumax = qnum_from_double((double)qnum_get_uint(umax));
    dimax = qnum_from_double((double)qnum_get_int(imax));
    dumax_exact = qnum_from_double((double)qnum_get_uint(umax_exact));
    dumax_exact_p1 = qnum_from_double((double)qnum_get_uint(umax_exact_p1));

    s0 = qstring_from_str("0");
    s_empty = qstring_new();
    bfalse = qbool_from_bool(false);

    /* The internal representation should not matter, as long as the
     * precision is sufficient */
    test_equality(true, u0, i0, d0);

    /* No automatic type conversion */
    test_equality(false, u0, s0, s_empty, bfalse, qnull(), NULL);
    test_equality(false, i0, s0, s_empty, bfalse, qnull(), NULL);
    test_equality(false, d0, s0, s_empty, bfalse, qnull(), NULL);

    /* Do not round */
    test_equality(false, u0, d0p25);
    test_equality(false, i0, d0p25);

    /* Do not assume any object is equal to itself -- note however
     * that NaN cannot occur in a JSON object anyway. */
    g_assert(qobject_is_equal(QOBJECT(dnan), QOBJECT(dnan)) == false);

    /* No unsigned overflow */
    test_equality(false, um42, im42);
    test_equality(false, um42, dm42);
    test_equality(true, im42, dm42);


    /*
     * Floating point values must match integers exactly to be
     * considered equal; it does not suffice that converting the
     * integer to a double yields the same value.
     * Each of the following four tests follows the same pattern:
     * 1. Check that both QNum objects compare unequal because they
     *    are (mathematically).  The third test is an exception,
     *    because here they are indeed equal.
     * 2. Check that when converting the integer QNum to a double,
     *    that value is equal to the double QNum.  We can thus see
     *    that the QNum comparison does not simply convert the
     *    integer to a floating point value (in a potentially lossy
     *    operation).
     * 3. Sanity checks: Check that the double QNum has the expected
     *    value (which may be one of two in case it was rounded; the
     *    exact result is then implementation-defined).
     *    If there are multiple valid values, check that they are
     *    distinct values when represented as double (just proving
     *    that our assumptions about the precision of doubles are
     *    correct).
     *
     * The first two tests are interesting because they may involve a
     * double value which is out of the uint64_t or int64_t range,
     * respectively (if it is rounded to 2^64 or 2^63 during
     * conversion).
     *
     * Since both are intended to involve rounding the value up during
     * conversion, we also have the fourth test which is indended to
     * test behavior if the value was rounded down. This is the fourth
     * test.
     *
     * The third test simply proves that the value used in the fourth
     * test is indeed just one above a number that can be exactly
     * represented in a double.
     */

    test_equality(false, umax, dumax);
    g_assert(qnum_get_double(umax) == qnum_get_double(dumax));
    g_assert(qnum_get_double(dumax) == 0x1p64 ||
             qnum_get_double(dumax) == 0x1p64 - 0x1p11);
    g_assert(0x1p64 != 0x1p64 - 0x1p11);

    test_equality(false, imax, dimax);
    g_assert(qnum_get_double(imax) == qnum_get_double(dimax));
    g_assert(qnum_get_double(dimax) == 0x1p63 ||
             qnum_get_double(dimax) == 0x1p63 - 0x1p10);
    g_assert(0x1p63 != 0x1p63 - 0x1p10);

    test_equality(true, umax_exact, dumax_exact);
    g_assert(qnum_get_double(umax_exact) == qnum_get_double(dumax_exact));
    g_assert(qnum_get_double(dumax_exact) == 0x1p64 - 0x1p11);

    test_equality(false, umax_exact_p1, dumax_exact_p1);
    g_assert(qnum_get_double(umax_exact_p1) == qnum_get_double(dumax_exact_p1));
    g_assert(qnum_get_double(dumax_exact_p1) == 0x1p64 ||
             qnum_get_double(dumax_exact_p1) == 0x1p64 - 0x1p11);
    g_assert(0x1p64 != 0x1p64 - 0x1p11);


    free_all(u0, i0, d0, d0p25, dnan, um42, im42, dm42,
             umax, imax, umax_exact, umax_exact_p1,
             dumax, dimax, dumax_exact, dumax_exact_p1,
             s0, s_empty, bfalse);
}

static void qobject_is_equal_bool_test(void)
{
    QBool *btrue_0, *btrue_1, *bfalse_0, *bfalse_1;

    /* Automatic type conversion is tested in the QNum test */

    btrue_0 = qbool_from_bool(true);
    btrue_1 = qbool_from_bool(true);
    bfalse_0 = qbool_from_bool(false);
    bfalse_1 = qbool_from_bool(false);

    test_equality(true, btrue_0, btrue_1);
    test_equality(true, bfalse_0, bfalse_1);
    test_equality(false, btrue_0, bfalse_0);
    test_equality(false, btrue_1, bfalse_1);

    free_all(btrue_0, btrue_1, bfalse_0, bfalse_1);
}

static void qobject_is_equal_string_test(void)
{
    QString *str_base, *str_whitespace_0, *str_whitespace_1, *str_whitespace_2;
    QString *str_whitespace_3, *str_case, *str_built;

    str_base = qstring_from_str("foo");
    str_whitespace_0 = qstring_from_str(" foo");
    str_whitespace_1 = qstring_from_str("foo ");
    str_whitespace_2 = qstring_from_str("foo\b");
    str_whitespace_3 = qstring_from_str("fooo\b");
    str_case = qstring_from_str("Foo");

    /* Should yield "foo" */
    str_built = qstring_from_substr("form", 0, 1);
    qstring_append_chr(str_built, 'o');

    test_equality(false, str_base, str_whitespace_0, str_whitespace_1,
                         str_whitespace_2, str_whitespace_3, str_case);

    test_equality(true, str_base, str_built);

    free_all(str_base, str_whitespace_0, str_whitespace_1, str_whitespace_2,
             str_whitespace_3, str_case, str_built);
}

static void qobject_is_equal_list_test(void)
{
    QList *list_0, *list_1, *list_cloned;
    QList *list_reordered, *list_longer, *list_shorter;

    list_0 = qlist_new();
    list_1 = qlist_new();
    list_reordered = qlist_new();
    list_longer = qlist_new();
    list_shorter = qlist_new();

    qlist_append_int(list_0, 1);
    qlist_append_int(list_0, 2);
    qlist_append_int(list_0, 3);

    qlist_append_int(list_1, 1);
    qlist_append_int(list_1, 2);
    qlist_append_int(list_1, 3);

    qlist_append_int(list_reordered, 1);
    qlist_append_int(list_reordered, 3);
    qlist_append_int(list_reordered, 2);

    qlist_append_int(list_longer, 1);
    qlist_append_int(list_longer, 2);
    qlist_append_int(list_longer, 3);
    qlist_append_obj(list_longer, qnull());

    qlist_append_int(list_shorter, 1);
    qlist_append_int(list_shorter, 2);

    list_cloned = qlist_copy(list_0);

    test_equality(true, list_0, list_1, list_cloned);
    test_equality(false, list_0, list_reordered, list_longer, list_shorter);

    /* With a NaN in it, the list should no longer compare equal to
     * itself */
    qlist_append(list_0, qnum_from_double(0.0 / 0.0));
    g_assert(qobject_is_equal(QOBJECT(list_0), QOBJECT(list_0)) == false);

    free_all(list_0, list_1, list_cloned, list_reordered, list_longer, list_shorter);
}

static void qobject_is_equal_dict_test(void)
{
    Error *local_err = NULL;
    QDict *dict_0, *dict_1, *dict_cloned;
    QDict *dict_different_key, *dict_different_value, *dict_different_null_key;
    QDict *dict_longer, *dict_shorter, *dict_nested;
    QDict *dict_crumpled;

    dict_0 = qdict_new();
    dict_1 = qdict_new();
    dict_different_key = qdict_new();
    dict_different_value = qdict_new();
    dict_different_null_key = qdict_new();
    dict_longer = qdict_new();
    dict_shorter = qdict_new();
    dict_nested = qdict_new();

    qdict_put_int(dict_0, "f.o", 1);
    qdict_put_int(dict_0, "bar", 2);
    qdict_put_int(dict_0, "baz", 3);
    qdict_put_obj(dict_0, "null", qnull());

    qdict_put_int(dict_1, "f.o", 1);
    qdict_put_int(dict_1, "bar", 2);
    qdict_put_int(dict_1, "baz", 3);
    qdict_put_obj(dict_1, "null", qnull());

    qdict_put_int(dict_different_key, "F.o", 1);
    qdict_put_int(dict_different_key, "bar", 2);
    qdict_put_int(dict_different_key, "baz", 3);
    qdict_put_obj(dict_different_key, "null", qnull());

    qdict_put_int(dict_different_value, "f.o", 42);
    qdict_put_int(dict_different_value, "bar", 2);
    qdict_put_int(dict_different_value, "baz", 3);
    qdict_put_obj(dict_different_value, "null", qnull());

    qdict_put_int(dict_different_null_key, "f.o", 1);
    qdict_put_int(dict_different_null_key, "bar", 2);
    qdict_put_int(dict_different_null_key, "baz", 3);
    qdict_put_obj(dict_different_null_key, "none", qnull());

    qdict_put_int(dict_longer, "f.o", 1);
    qdict_put_int(dict_longer, "bar", 2);
    qdict_put_int(dict_longer, "baz", 3);
    qdict_put_int(dict_longer, "xyz", 4);
    qdict_put_obj(dict_longer, "null", qnull());

    qdict_put_int(dict_shorter, "f.o", 1);
    qdict_put_int(dict_shorter, "bar", 2);
    qdict_put_int(dict_shorter, "baz", 3);

    qdict_put(dict_nested, "f", qdict_new());
    qdict_put_int(qdict_get_qdict(dict_nested, "f"), "o", 1);
    qdict_put_int(dict_nested, "bar", 2);
    qdict_put_int(dict_nested, "baz", 3);
    qdict_put_obj(dict_nested, "null", qnull());

    dict_cloned = qdict_clone_shallow(dict_0);

    test_equality(true, dict_0, dict_1, dict_cloned);
    test_equality(false, dict_0, dict_different_key, dict_different_value,
                         dict_different_null_key, dict_longer, dict_shorter,
                         dict_nested);

    dict_crumpled = qobject_to_qdict(qdict_crumple(dict_1, &local_err));
    g_assert(!local_err);
    test_equality(true, dict_crumpled, dict_nested);

    qdict_flatten(dict_nested);
    test_equality(true, dict_0, dict_nested);

    /* Containing an NaN value will make this dict compare unequal to
     * itself */
    qdict_put(dict_0, "NaN", qnum_from_double(0.0 / 0.0));
    g_assert(qobject_is_equal(QOBJECT(dict_0), QOBJECT(dict_0)) == false);

    free_all(dict_0, dict_1, dict_cloned, dict_different_key,
             dict_different_value, dict_different_null_key, dict_longer,
             dict_shorter, dict_nested, dict_crumpled);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/public/qobject_is_equal_null",
                    qobject_is_equal_null_test);
    g_test_add_func("/public/qobject_is_equal_num", qobject_is_equal_num_test);
    g_test_add_func("/public/qobject_is_equal_bool",
                    qobject_is_equal_bool_test);
    g_test_add_func("/public/qobject_is_equal_string",
                    qobject_is_equal_string_test);
    g_test_add_func("/public/qobject_is_equal_list",
                    qobject_is_equal_list_test);
    g_test_add_func("/public/qobject_is_equal_dict",
                    qobject_is_equal_dict_test);

    return g_test_run();
}
