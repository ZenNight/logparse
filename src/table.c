#define _POSIX_C_SOURCE 200809L
#include "table.h"

#include <stdlib.h>
#include <string.h>

/* FNV 1a, 64 bit. Good enough distribution for log lines and trivial to verify. */
static size_t hash_str(const char *s) {
    unsigned long long h = 1469598103934665603ULL;
    for (; *s; s++) {
        h ^= (unsigned char)*s;
        h *= 1099511628211ULL;
    }
    return (size_t)h;
}

int table_init(table *t, size_t initial_cap) {
    size_t cap = 16;
    while (cap < initial_cap) cap <<= 1;
    t->slots = calloc(cap, sizeof *t->slots);
    if (!t->slots) return -1;
    t->cap = cap;
    t->len = 0;
    return 0;
}

void table_free(table *t) {
    for (size_t i = 0; i < t->cap; i++) free(t->slots[i].key);
    free(t->slots);
    t->slots = NULL;
    t->cap = t->len = 0;
}

static table_entry *find_slot(table_entry *slots, size_t cap, const char *key) {
    size_t i = hash_str(key) & (cap - 1);
    for (;;) {
        if (!slots[i].key || strcmp(slots[i].key, key) == 0) return &slots[i];
        i = (i + 1) & (cap - 1);  /* linear probing */
    }
}

static int grow(table *t) {
    size_t ncap = t->cap * 2;
    table_entry *nslots = calloc(ncap, sizeof *nslots);
    if (!nslots) return -1;
    for (size_t i = 0; i < t->cap; i++) {
        if (!t->slots[i].key) continue;
        table_entry *dst = find_slot(nslots, ncap, t->slots[i].key);
        *dst = t->slots[i];  /* move ownership of key */
    }
    free(t->slots);
    t->slots = nslots;
    t->cap = ncap;
    return 0;
}

int table_bump(table *t, const char *key) {
    if (t->len * 10 >= t->cap * 7 && grow(t) != 0) return -1;  /* keep load factor under 0.7 */
    table_entry *e = find_slot(t->slots, t->cap, key);
    if (!e->key) {
        e->key = strdup(key);
        if (!e->key) return -1;
        e->count = 0;
        t->len++;
    }
    e->count++;
    return 0;
}

static int cmp_desc(const void *a, const void *b) {
    const table_entry *x = a, *y = b;
    if (x->count != y->count) return x->count < y->count ? 1 : -1;
    return strcmp(x->key, y->key);  /* deterministic order for equal counts */
}

size_t table_top(const table *t, table_entry *out, size_t n) {
    if (n == 0 || t->len == 0) return 0;
    table_entry *all = malloc(t->len * sizeof *all);
    if (!all) return 0;
    size_t k = 0;
    for (size_t i = 0; i < t->cap; i++)
        if (t->slots[i].key) all[k++] = t->slots[i];
    qsort(all, k, sizeof *all, cmp_desc);
    size_t m = k < n ? k : n;
    memcpy(out, all, m * sizeof *out);
    free(all);
    return m;
}
