/*_
 * Copyright (c) 2018 Hirochika Asai <asai@jar.jp>
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "../pctrie.h"
#include "radix.h"
#include <stdio.h>
#include <sys/time.h>

/* Macro for testing */
#define TEST_FUNC(str, func, ret)                \
    do {                                         \
        printf("%s: ", str);                     \
        if ( 0 == func() ) {                     \
            printf("passed");                    \
        } else {                                 \
            printf("failed");                    \
            ret = -1;                            \
        }                                        \
        printf("\n");                            \
    } while ( 0 )

#define TEST_PROGRESS()                              \
    do {                                             \
        printf(".");                                 \
        fflush(stdout);                              \
    } while ( 0 )

/*
 * Xorshift
 */
static __inline__ uint32_t
xor128(void)
{
    static uint32_t x = 123456789;
    static uint32_t y = 362436069;
    static uint32_t z = 521288629;
    static uint32_t w = 88675123;
    uint32_t t;

    t = x ^ (x<<11);
    x = y;
    y = z;
    z = w;
    return w = (w ^ (w>>19)) ^ (t ^ (t >> 8));
}

static __inline__ double
getmicrotime(void)
{
    struct timeval tv;
    double microsec;

    if ( 0 != gettimeofday(&tv, NULL) ) {
        return 0.0;
    }

    microsec = (double)tv.tv_sec + (1.0 * tv.tv_usec / 1000000);

    return microsec;
}

/*
 * Initialization test
 */
static int
test_init(void)
{
    struct path_compressed_trie *trie;

    /* Initialize */
    trie = path_compressed_trie_init(NULL);
    if ( NULL == trie ) {
        return -1;
    }

    TEST_PROGRESS();

    /* Release */
    path_compressed_trie_release(trie);

    return 0;
}

/*
 * Lookup test
 */
static int
test_lookup(void)
{
    struct path_compressed_trie *trie;
    uint32_t key0 = 0x01234567;
    uint32_t key1 = 0x01020304;
    int ret;
    void *val;

    /* Initialize */
    trie = path_compressed_trie_init(NULL);
    if ( NULL == trie ) {
        return -1;
    }

    /* No entry */
    val = path_compressed_trie_lookup(trie, key0);
    if ( NULL != val ) {
        /* Failed */
        return -1;
    }

    TEST_PROGRESS();

    /* Insert */
    ret = path_compressed_trie_add(trie, key1, 32, (void *)(uint64_t)key1);
    if ( ret < 0 ) {
        /* Failed to insert */
        return -1;
    }

    /* Lookup */
    val = path_compressed_trie_lookup(trie, key1);
    if ( val != (void *)(uint64_t)key1 ) {
        /* Failed */
        return -1;
    }

    /* Lookup */
    val = path_compressed_trie_delete(trie, key1, 32);
    if ( val != (void *)(uint64_t)key1 ) {
        /* Failed */
        return -1;
    }

    /* Lookup */
    val = path_compressed_trie_lookup(trie, key1);
    if ( NULL != val ) {
        /* Failed */
        return -1;
    }

    /* Release */
    path_compressed_trie_release(trie);

    return 0;
}

static int
test_lookup_linx(void)
{
    struct path_compressed_trie *trie;
    struct radix_tree *radix;
    FILE *fp;
    char buf[4096];
    int prefix[4];
    int prefixlen;
    int nexthop[4];
    int ret;
    uint32_t addr1;
    uint64_t addr2;
    ssize_t i;
    double t0;
    double t1;
    uint32_t a;
    uint64_t res0;
    uint64_t res1;

    /* Load from the linx file */
    fp = fopen("tests/linx-rib.20141217.0000-p46.txt", "r");
    if ( NULL == fp ) {
        return -1;
    }

    /* Initialize */
    trie = path_compressed_trie_init(NULL);
    if ( NULL == trie ) {
        return -1;
    }
    radix = radix_tree_init(NULL);
    if ( NULL == radix ) {
        return -1;
    }

    /* Load the full route */
    i = 0;
    while ( !feof(fp) ) {
        if ( !fgets(buf, sizeof(buf), fp) ) {
            continue;
        }
        ret = sscanf(buf, "%d.%d.%d.%d/%d %d.%d.%d.%d", &prefix[0], &prefix[1],
                     &prefix[2], &prefix[3], &prefixlen, &nexthop[0],
                     &nexthop[1], &nexthop[2], &nexthop[3]);
        if ( ret < 0 ) {
            return -1;
        }

        /* Convert to u32 */
        addr1 = ((uint32_t)prefix[0] << 24) + ((uint32_t)prefix[1] << 16)
            + ((uint32_t)prefix[2] << 8) + (uint32_t)prefix[3];
        addr2 = ((uint32_t)nexthop[0] << 24) + ((uint32_t)nexthop[1] << 16)
            + ((uint32_t)nexthop[2] << 8) + (uint32_t)nexthop[3];

        /* Add an entry */
        ret = path_compressed_trie_add(trie, addr1, prefixlen,
                                       (void *)(uint64_t)addr2);
        if ( ret < 0 ) {
            return -1;
        }
        ret = radix_tree_add(radix, addr1, prefixlen, (void *)(uint64_t)addr2);
        if ( ret < 0 ) {
            return -1;
        }
        if ( 0 == i % 10000 ) {
            TEST_PROGRESS();
        }
        i++;
    }

    t0 = getmicrotime();

    for ( i = 0; i < 0x100000000LL; i++ ) {
        if ( 0 == i % 0x10000000ULL ) {
            TEST_PROGRESS();
        }
        a = i;
        res0 = (uint64_t)path_compressed_trie_lookup(trie, a);
        res1 = (uint64_t)radix_tree_lookup(radix, a);
        if ( res0 != res1 ) {
            fprintf(stderr, "%x %llx %llx\n", a, res0, res1);
            return -1;
        }
    }
    t1 = getmicrotime();

    /* Release */
    path_compressed_trie_release(trie);
    radix_tree_release(radix);

    /* Close */
    fclose(fp);

    return 0;
}

static int
test_lookup_linx_performance(void)
{
    struct path_compressed_trie *trie;
    FILE *fp;
    char buf[4096];
    int prefix[4];
    int prefixlen;
    int nexthop[4];
    int ret;
    uint32_t addr1;
    uint64_t addr2;
    ssize_t i;
    uint64_t res;
    double t0;
    double t1;
    uint32_t a;

    /* Load from the linx file */
    fp = fopen("tests/linx-rib.20141217.0000-p46.txt", "r");
    if ( NULL == fp ) {
        return -1;
    }

    /* Initialize */
    trie = path_compressed_trie_init(NULL);
    if ( NULL == trie ) {
        return -1;
    }

    /* Load the full route */
    i = 0;
    while ( !feof(fp) ) {
        if ( !fgets(buf, sizeof(buf), fp) ) {
            continue;
        }
        ret = sscanf(buf, "%d.%d.%d.%d/%d %d.%d.%d.%d", &prefix[0], &prefix[1],
                     &prefix[2], &prefix[3], &prefixlen, &nexthop[0],
                     &nexthop[1], &nexthop[2], &nexthop[3]);
        if ( ret < 0 ) {
            return -1;
        }

        /* Convert to u32 */
        addr1 = ((uint32_t)prefix[0] << 24) + ((uint32_t)prefix[1] << 16)
            + ((uint32_t)prefix[2] << 8) + (uint32_t)prefix[3];
        addr2 = ((uint32_t)nexthop[0] << 24) + ((uint32_t)nexthop[1] << 16)
            + ((uint32_t)nexthop[2] << 8) + (uint32_t)nexthop[3];

        /* Add an entry */
        ret = path_compressed_trie_add(trie, addr1, prefixlen,
                                       (void *)(uint64_t)addr2);
        if ( ret < 0 ) {
            return -1;
        }
        if ( 0 == i % 10000 ) {
            TEST_PROGRESS();
        }
        i++;
    }

    t0 = getmicrotime();

    res = 0;
    for ( i = 0; i < 0x100000000LL; i++ ) {
        if ( 0 == i % 0x10000000ULL ) {
            TEST_PROGRESS();
        }
        a = xor128();
        res ^= (uint64_t)path_compressed_trie_lookup(trie, a);
    }
    t1 = getmicrotime();

    printf("RESULT: %llx\n", res);

    printf("Result[0]: %lf ns/lookup\n", (t1 - t0)/i * 1000000000);
    printf("Result[1]: %lf Mlps\n", 1.0 * i / (t1 - t0) / 1000000);

    /* Release */
    path_compressed_trie_release(trie);

    /* Close */
    fclose(fp);

    return 0;
}

/*
 * Main routine for the basic test
 */
int
main(int argc, const char *const argv[])
{
    int ret;

    /* Reset */
    ret = 0;

    /* Run tests */
    TEST_FUNC("init", test_init, ret);
    TEST_FUNC("lookup", test_lookup, ret);
    TEST_FUNC("lookup_fullroute", test_lookup_linx, ret);
    //TEST_FUNC("performance", test_lookup_linx_performance, ret);

    return 0;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
