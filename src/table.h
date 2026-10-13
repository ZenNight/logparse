/* Open addressing hash table from string key to count.
 * Keys are copied on insert, so callers may reuse their buffers. */
#ifndef LOGPARSE_TABLE_H
#define LOGPARSE_TABLE_H

#include <stddef.h>

typedef struct {
    char *key;
    size_t count;
} table_entry;

typedef struct {
    table_entry *slots;
    size_t cap;   /* always a power of two */
    size_t len;   /* occupied slots */
} table;

int    table_init(table *t, size_t initial_cap);
void   table_free(table *t);
/* Increment the count for key, inserting it on first sight. Returns 0 on success, -1 on allocation failure. */
int    table_bump(table *t, const char *key);
/* Fill `out` (size n) with the entries having the highest counts, in descending order. Returns how many were written. */
size_t table_top(const table *t, table_entry *out, size_t n);

#endif
