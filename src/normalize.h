/* Turning a raw log line into a level and a message template. */
#ifndef LOGPARSE_NORMALIZE_H
#define LOGPARSE_NORMALIZE_H

#include <stddef.h>

typedef enum { LVL_DEBUG, LVL_INFO, LVL_WARN, LVL_ERROR, LVL_FATAL, LVL_UNKNOWN, LVL_COUNT } level;

extern const char *const level_names[LVL_COUNT];

/* Detect the first level token in the line (INFO, WARN, WARNING, ERROR, ERR, FATAL, DEBUG, case insensitive).
 * Returns the level and sets *after to the text following the token, or LVL_UNKNOWN with *after = line. */
level detect_level(const char *line, const char **after);

/* Copy `src` into `dst` (size cap) replacing runs of digits with '#' and collapsing whitespace,
 * so "user 1042 took 37ms" and "user 7 took 120ms" become the same template. Always NUL terminates. */
void normalize_message(const char *src, char *dst, size_t cap);

#endif
