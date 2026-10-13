/* logparse: stream a log file once, report level counts and the most frequent message templates.
 *
 * Reads line by line with getline so memory stays flat regardless of file size.
 * Only distinct normalized templates are kept in memory, not the lines themselves. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "normalize.h"
#include "table.h"

#define MAX_TEMPLATE 512

typedef struct {
    size_t top;          /* how many templates to show */
    level min_level;     /* only count templates at or above this level */
    const char *grep;    /* substring filter, NULL for none */
    int json;            /* machine readable output */
} options;

static void usage(FILE *out) {
    fputs("usage: logparse [options] [FILE ...]\n"
          "  reads stdin when no FILE is given\n"
          "  -n N        show the N most frequent templates (default 10)\n"
          "  -l LEVEL    only template lines at LEVEL or above (debug, info, warn, error, fatal)\n"
          "  -g TEXT     only lines containing TEXT\n"
          "  -j          print JSON instead of text\n"
          "  -h          this help\n", out);
}

static int parse_level(const char *s, level *out) {
    for (int i = 0; i < LVL_UNKNOWN; i++)
        if (strcasecmp(s, level_names[i]) == 0) { *out = (level)i; return 0; }
    return -1;
}

typedef struct {
    size_t lines, matched, by_level[LVL_COUNT];
    table templates;
} stats;

static int process_stream(FILE *fp, const options *opt, stats *st) {
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    char tmpl[MAX_TEMPLATE];
    while ((n = getline(&line, &cap, fp)) != -1) {
        st->lines++;
        if (n > 0 && line[n - 1] == '\n') line[n - 1] = '\0';
        if (opt->grep && !strstr(line, opt->grep)) continue;
        st->matched++;
        const char *msg;
        level lvl = detect_level(line, &msg);
        st->by_level[lvl]++;
        if (lvl != LVL_UNKNOWN && lvl < opt->min_level) continue;
        normalize_message(msg, tmpl, sizeof tmpl);
        if (tmpl[0] == '\0') continue;
        if (table_bump(&st->templates, tmpl) != 0) { free(line); return -1; }
    }
    free(line);
    return ferror(fp) ? -1 : 0;
}

static void json_escape(const char *s) {
    for (; *s; s++) {
        switch (*s) {
            case '"': fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if ((unsigned char)*s < 0x20) printf("\\u%04x", (unsigned char)*s);
                else putchar(*s);
        }
    }
}

static void report(const stats *st, const options *opt) {
    table_entry *top = calloc(opt->top ? opt->top : 1, sizeof *top);
    size_t k = top ? table_top(&st->templates, top, opt->top) : 0;
    if (opt->json) {
        printf("{\"lines\":%zu,\"matched\":%zu,\"levels\":{", st->lines, st->matched);
        for (int i = 0; i < LVL_COUNT; i++)
            printf("%s\"%s\":%zu", i ? "," : "", level_names[i], st->by_level[i]);
        printf("},\"templates\":[");
        for (size_t i = 0; i < k; i++) {
            printf("%s{\"count\":%zu,\"template\":\"", i ? "," : "", top[i].count);
            json_escape(top[i].key);
            printf("\"}");
        }
        puts("]}");
    } else {
        printf("lines      %zu\n", st->lines);
        if (opt->grep) printf("matched    %zu  (containing \"%s\")\n", st->matched, opt->grep);
        size_t known = st->matched - st->by_level[LVL_UNKNOWN];
        printf("levels    ");
        for (int i = 0; i < LVL_COUNT; i++) printf(" %s=%zu", level_names[i], st->by_level[i]);
        putchar('\n');
        if (known) {
            size_t bad = st->by_level[LVL_ERROR] + st->by_level[LVL_FATAL];
            printf("error rate %.2f%%  (%zu of %zu levelled lines)\n", 100.0 * (double)bad / (double)known, bad, known);
        }
        printf("distinct   %zu templates\n", st->templates.len);
        if (k) {
            printf("\ntop %zu templates:\n", k);
            for (size_t i = 0; i < k; i++) printf("%8zu  %s\n", top[i].count, top[i].key);
        }
    }
    free(top);
}

int main(int argc, char **argv) {
    options opt = {.top = 10, .min_level = LVL_DEBUG, .grep = NULL, .json = 0};
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(a, "-j") == 0) { opt.json = 1; continue; }
        if (strcmp(a, "--") == 0) { i++; break; }
        if (i + 1 >= argc) { usage(stderr); return 2; }
        if (strcmp(a, "-n") == 0) {
            char *end; long v = strtol(argv[++i], &end, 10);
            if (*end || v < 0) { fprintf(stderr, "logparse: bad -n value\n"); return 2; }
            opt.top = (size_t)v;
        } else if (strcmp(a, "-l") == 0) {
            if (parse_level(argv[++i], &opt.min_level) != 0) { fprintf(stderr, "logparse: unknown level '%s'\n", argv[i]); return 2; }
        } else if (strcmp(a, "-g") == 0) {
            opt.grep = argv[++i];
        } else { usage(stderr); return 2; }
    }

    stats st = {0};
    if (table_init(&st.templates, 1024) != 0) { perror("logparse"); return 1; }
    int rc = 0;
    if (i >= argc) {
        if (process_stream(stdin, &opt, &st) != 0) { perror("logparse: stdin"); rc = 1; }
    } else {
        for (; i < argc; i++) {
            FILE *fp = strcmp(argv[i], "-") == 0 ? stdin : fopen(argv[i], "r");
            if (!fp) { fprintf(stderr, "logparse: %s: %s\n", argv[i], strerror(errno)); rc = 1; continue; }
            if (process_stream(fp, &opt, &st) != 0) { fprintf(stderr, "logparse: %s: read error\n", argv[i]); rc = 1; }
            if (fp != stdin) fclose(fp);
        }
    }
    report(&st, &opt);
    table_free(&st.templates);
    return rc;
}
