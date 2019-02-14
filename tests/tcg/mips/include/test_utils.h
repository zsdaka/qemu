/*
 *  Header file for test utilities
 *
 *  Copyright (C) 2018  Wave Computing, Inc.
 *  Copyright (C) 2018  Aleksandar Markovic <amarkovic@wavecomp.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#define PRINT_RESULTS 0

static inline int32_t check_results(char *instruction_name,
                                    uint32_t test_count,
                                    double elapsed_time,
                                    uint64_t *b128_result,
                                    uint64_t *b128_expect)
{
#if PRINT_RESULTS

    uint32_t i;
    printf("\n");
    for (i = 0; i < test_count; i++) {
        uint64_t a, b;
        memcpy(&a, (b128_result + 2 * i), 8);
        memcpy(&b, (b128_result + 2 * i + 1), 8);
        if (i % 8 != 0) {
            printf("        { 0x%016llxULL, 0x%016llxULL, },\n", a, b);
        } else {
            printf("        { 0x%016llxULL, 0x%016llxULL, },    /* %3d  */\n",
                   a, b, i);
        }
    }
    printf("\n");

    return 0;

#else

    uint32_t i;
    uint32_t pass_count = 0;
    uint32_t fail_count = 0;

    printf("%s:   ", instruction_name);
    for (i = 0; i < test_count; i++) {
        if (b128_result[i] == b128_expect[i]) {
            pass_count++;
        } else {
            fail_count++;
        }
    }

    printf("PASS: %3d   FAIL: %3d   elapsed time: %5.2f ms\n",
           pass_count, fail_count, elapsed_time);

    if (fail_count > 0) {
        return -1;
    } else {
        return 0;
    }

#endif
}

#endif
