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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "pctrie.h"

#define BIT_TEST(k, b)  ((k) & (0x80000000ULL >> (b)))
#define BIT_PREFIX(k, b)    (((uint64_t)(k) >> (32 - (b))) << (32 - (b)))

/*
 * Initialize the data structure for path-compressed trie
 */
struct path_compressed_trie *
path_compressed_trie_init(struct path_compressed_trie *trie)
{
    if ( NULL == trie ) {
        /* Allocate new data structure */
        trie = malloc(sizeof(struct path_compressed_trie));
        if ( NULL == trie ) {
            return NULL;
        }
        trie->_allocated = 1;
    } else {
        trie->_allocated = 0;
    }

    /* Set NULL to the root node */
    trie->root = NULL;

    return trie;
}

/*
 * Release the node and descendant nodes
 */
static void
_free_nodes(struct path_compressed_trie_node *node)
{
    if ( NULL != node ) {
        _free_nodes(node->left);
        _free_nodes(node->right);
        free(node);
    }
}

/*
 * Release the path-compressed trie
 */
void
path_compressed_trie_release(struct path_compressed_trie *trie)
{
    _free_nodes(trie->root);
    if ( trie->_allocated ) {
        free(trie);
    }
}

/*
 * Recursive process of the lookup procedure
 */
static void *
_lookup(struct path_compressed_trie_node *cur,
        struct path_compressed_trie_node *cand, uint32_t key)
{
    struct path_compressed_trie_node *next;

    /* Reaches at the leaf */
    if ( NULL == cur ) {
        return NULL != cand ? cand->data : NULL;
    }
    if ( cur->bit < 0 ||
         BIT_PREFIX(cur->key, cur->bit) != BIT_PREFIX(key, cur->bit) ) {
        if ( BIT_PREFIX(cur->key, cur->prefixlen)
             == BIT_PREFIX(key, cur->prefixlen) ) {
            cand = cur;
        }
        return NULL != cand ? cand->data : NULL;
    }
    if ( NULL != cur->data ) {
        cand = cur;
    }

    if ( BIT_TEST(key, cur->bit) ) {
        /* Right */
        next = cur->right;
    } else {
        /* Left */
        next = cur->left;
    }

    return _lookup(next, cand, key);
}

/*
 * Lookup the data corresponding to the key specified by the argument
 */
void *
path_compressed_trie_lookup(struct path_compressed_trie *trie, uint32_t key)
{
    return _lookup(trie->root, NULL, key);
}

/*
 * Compute the difference
 */
static int
_diff(uint32_t key0, int plen0, uint32_t key1, int plen1, int cache)
{
    int i;

    for ( i = cache; i <= plen0 && i <= plen1; i++ ) {
        if ( BIT_TEST(key0, i) != BIT_TEST(key1, i) ) {
            return i;
        }
    }

    if ( plen0 == plen1 ) {
        return -1;
    } else if ( plen0 < plen1 ) {
        return plen0;
    } else {
        return plen1;
    }
}

/*
 * Create a new node
 */
static struct path_compressed_trie_node *
_new_node(uint32_t key, int prefixlen, void *data)
{
    struct path_compressed_trie_node *n;

    n = malloc(sizeof(struct path_compressed_trie_node));
    if ( NULL == n ) {
        return NULL;
    }
    n->bit = -1;
    n->left = NULL;
    n->right = NULL;
    n->key = key;
    n->prefixlen = prefixlen;
    n->data = data;

    return n;
}

/*
 * Add a data value (recursive)
 */
static int
_add(struct path_compressed_trie_node **cur, uint32_t key, int prefixlen,
     void *data)
{
    struct path_compressed_trie_node *n;
    struct path_compressed_trie_node *c;
    int d;

    if ( NULL == *cur ) {
        /* New node to the leaf */
        n = _new_node(key, prefixlen, data);
        if ( NULL == n ) {
            return -1;
        }
        *cur = n;

        return 0;
    }

    /* Compare the prefixes */
    d = _diff(key, prefixlen, (*cur)->key, (*cur)->prefixlen, 0);
    if ( d < 0 ) {
        /* Same prefixes for key and (*cur)->key */
        return -1;
    }
    if ( (*cur)->bit >= 0 ) {
        if ( d == (*cur)->bit && d == prefixlen ) {
            /* *cur is the node to insert. */
            if ( NULL != (*cur)->data ) {
                /* Already exists. */
                return -1;
            }
            (*cur)->key = key;
            (*cur)->prefixlen = prefixlen;
            (*cur)->data = data;
        } else if ( d < (*cur)->bit ) {
            /* Insert to the parent of *cur */
            if ( d == prefixlen ) {
                n = _new_node(key, prefixlen, data);
                if ( NULL == n ) {
                    return -1;
                }
                n->bit = d;
                if ( BIT_TEST((*cur)->key, d) ) {
                    /* Right */
                    n->right = *cur;
                } else {
                    /* Left */
                    n->left = *cur;
                }
                *cur = n;
            } else {
                n = _new_node(BIT_PREFIX(key, d), d, NULL);
                if ( NULL == n ) {
                    return -1;
                }
                c = _new_node(key, prefixlen, data);
                if ( NULL == n ) {
                    free(n);
                    return -1;
                }
                n->bit = d;
                if ( BIT_TEST(key, d) ) {
                    /* Right */
                    n->left = *cur;
                    n->right = c;
                } else {
                    /* Left */
                    n->left = c;
                    n->right = *cur;
                }
                *cur = n;
            }
        } else {
            /* Traverse to a descendant node */
            if ( BIT_TEST(key, (*cur)->bit) ) {
                /* Right */
                return _add(&(*cur)->right, key, prefixlen, data);
            } else {
                /* Left */
                return _add(&(*cur)->left, key, prefixlen, data);
            }
        }
    } else {
        /* *cur is a leaf. */
        if ( d == prefixlen ) {
            /* *cur is a descendant node of the new node */
            n = _new_node(key, prefixlen, data);
            if ( NULL == n ) {
                return -1;
            }
            n->bit = d;
            if ( BIT_TEST((*cur)->key, d) ) {
                /* Right */
                n->right = *cur;
            } else {
                /* Left */
                n->left = *cur;
            }
            *cur = n;
        } else if ( d == (*cur)->prefixlen )  {
            /* The new node is a descendant node of *cur */
            n = _new_node(key, prefixlen, data);
            if ( NULL == n ) {
                return -1;
            }
            /* Assert */
            if ( d != (*cur)->prefixlen ) {
                fprintf(stderr, "Fatal error %s %d\n", __FILE__, __LINE__);
                return -1;
            }
            (*cur)->bit = d;
            if ( BIT_TEST(key, d) ) {
                /* Right */
                (*cur)->right = n;
            } else {
                /* Left */
                (*cur)->left = n;
            }
        } else {
            /* *cur and the new node are descendant nodes of another node. */
            n = _new_node(BIT_PREFIX(key, d), d, NULL);
            if ( NULL == n ) {
                return -1;
            }
            c = _new_node(key, prefixlen, data);
            if ( NULL == n ) {
                free(n);
                return -1;
            }
            n->bit = d;
            if ( BIT_TEST(key, d) ) {
                /* Right */
                n->left = *cur;
                n->right = c;
            } else {
                /* Left */
                n->left = c;
                n->right = *cur;
            }
            *cur = n;
        }
    }

    return 0;
}

/*
 * Add a data value to the key
 */
int
path_compressed_trie_add(struct path_compressed_trie *trie, uint32_t key,
                         int prefixlen, void *data)
{
    return _add(&trie->root, key, prefixlen, data);
}

/*
 * Delete the data value corresponding to the key and return the value
 */
static void *
_delete(struct path_compressed_trie_node **n,
        struct path_compressed_trie_node *p, uint32_t key, int prefixlen)
{
    void *data;
    struct path_compressed_trie_node **c;

    if ( NULL == *n ) {
        return NULL;
    }

    if ( BIT_PREFIX(key, prefixlen) == BIT_PREFIX((*n)->key, (*n)->prefixlen)
         && prefixlen == (*n)->prefixlen ) {
        /* n is the node corresponding to the set of key and prefix length */
        data = (*n)->data;
        if ( (*n)->bit < 0 ) {
            /* n is a leaf. */
            free(*n);
            *n = NULL;
            if ( NULL != p && NULL == p->left && NULL == p->right ) {
                p->bit = -1;
            }
        }

        return data;
    }
    if ( (*n)->bit < 0 ) {
        /* Reach at a leaf */
        return NULL;
    }

    if ( BIT_TEST(key, (*n)->bit) ) {
        /* Right */
        c = &(*n)->right;
    } else {
        /* Left */
        c = &(*n)->left;
    }

    data = _delete(c, *n, key, prefixlen);
    if ( NULL == data ) {
        return NULL;
    }

    if ( (*n)->bit < 0 && NULL == (*n)->data ) {
        /* n is (becomes) a leaf without data. */
        *n = NULL;
        free(*n);
        if ( NULL != p && NULL == p->left && NULL == p->right ) {
            /* p becomes a leaf */
            p->bit = -1;
        }
    }

    return data;
}

/*
 * Delete the data value corresponding to the key and return it
 */
void *
path_compressed_trie_delete(struct path_compressed_trie *trie, uint32_t key,
                            int prefixlen)
{
    return _delete(&trie->root, NULL, key, prefixlen);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
