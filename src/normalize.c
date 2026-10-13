#define _POSIX_C_SOURCE 200809L
#include "normalize.h"

#include <ctype.h>
#include <strings.h>
#include <string.h>

const char *const level_names[LVL_COUNT] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL", "UNKNOWN"};

static const struct { const char *tok; level lvl; } tokens[] = {
    {"FATAL", LVL_FATAL}, {"CRITICAL", LVL_FATAL},
    {"ERROR", LVL_ERROR}, {"ERR", LVL_ERROR},
    {"WARNING", LVL_WARN}, {"WARN", LVL_WARN},
    {"INFO", LVL_INFO},
    {"DEBUG", LVL_DEBUG}, {"TRACE", LVL_DEBUG},
};

static int is_word_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

level detect_level(const char *line, const char **after) {
    const char *best = NULL;
    level best_lvl = LVL_UNKNOWN;
    size_t best_len = 0;
    for (size_t i = 0; i < sizeof tokens / sizeof tokens[0]; i++) {
        size_t n = strlen(tokens[i].tok);
        for (const char *p = line; *p; p++) {
            if (strncasecmp(p, tokens[i].tok, n) != 0) continue;
            int left_ok = (p == line) || !is_word_char(p[-1]);
            int right_ok = !is_word_char(p[n]);
            if (!left_ok || !right_ok) continue;
            if (!best || p < best) { best = p; best_lvl = tokens[i].lvl; best_len = n; }
            break;
        }
    }
    if (!best) { *after = line; return LVL_UNKNOWN; }
    const char *a = best + best_len;
    while (*a && (isspace((unsigned char)*a) || *a == ':' || *a == ']' || *a == '-' || *a == '|')) a++;
    *after = a;
    return best_lvl;
}

void normalize_message(const char *src, char *dst, size_t cap) {
    size_t j = 0;
    int in_digits = 0, in_space = 0;
    for (const char *p = src; *p && j + 1 < cap; p++) {
        unsigned char c = (unsigned char)*p;
        if (isdigit(c)) {
            if (!in_digits) dst[j++] = '#';
            in_digits = 1; in_space = 0;
        } else if (isspace(c)) {
            if (!in_space && j > 0) dst[j++] = ' ';
            in_space = 1; in_digits = 0;
        } else {
            dst[j++] = (char)c;
            in_digits = in_space = 0;
        }
    }
    while (j > 0 && dst[j - 1] == ' ') j--;
    dst[j] = '\0';
}
