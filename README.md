# logparse

A single pass log analyzer in C11. Streams a log file of any size, counts lines per level, and reports the most frequent **message templates** (lines with numbers collapsed), so ten thousand `user 1042 logged in` lines show up as one row with a count instead of ten thousand rows.

```
$ ./build/logparse -n 4 tests/fixtures/app.log
lines      12
levels     DEBUG=1 INFO=4 WARN=1 ERROR=4 FATAL=1 UNKNOWN=1
error rate 45.45%  (5 of 11 levelled lines)
distinct   8 templates

top 4 templates:
       3  db connection refused (attempt #)
       3  user # logged in from #.#.#.#
       1  cache hit ratio #.#
       1  out of memory, exiting
```

## Why C

I learned C in CS50 and use it in the Computational Reasoning course at PUCPR. This is the first thing I wrote in C that I actually use: when a client's chatbot logs grow past what a text editor opens, `logparse` tells me in under a second what is going wrong and how often, with memory that stays flat because only distinct templates are stored.

## Build and test

```bash
make            # build/logparse, with -Wall -Wextra -Wpedantic -Wshadow -Wconversion and zero warnings
make test       # 16 shell level cases
make debug      # build/logparse-debug with AddressSanitizer and UBSan
```

No dependencies beyond a C11 compiler and POSIX `getline`. Tested with gcc on Linux.

## Usage

```
logparse [options] [FILE ...]
  reads stdin when no FILE is given
  -n N        show the N most frequent templates (default 10)
  -l LEVEL    only template lines at LEVEL or above (debug, info, warn, error, fatal)
  -g TEXT     only lines containing TEXT
  -j          print JSON instead of text
```

```bash
logparse -l error app.log                 # what is failing, grouped
logparse -g "user 1042" app.log           # one user's activity
zcat old.log.gz | logparse -j | jq .      # pipes and JSON
logparse -n 20 web1.log web2.log web3.log # several files, one report
```

## How it works

1. `main.c` reads with `getline`, so a line of any length works and memory does not scale with file size.
2. `normalize.c` finds the first level token (`INFO`, `WARN`, `WARNING`, `ERROR`, `ERR`, `FATAL`, `CRITICAL`, `DEBUG`, `TRACE`, case insensitive, word bounded so `ERRORS_TOTAL` does not match) and takes the text after it as the message. Runs of digits become `#` and whitespace is collapsed, which turns `request 3391 took 1207ms` into `request # took #ms`.
3. `table.c` is an open addressing hash table (FNV 1a, linear probing, load factor under 0.7, doubling on growth) from template to count. Keys are copied on insert. `table_top` sorts a snapshot to return the N largest, with ties broken alphabetically so output is deterministic.

## Design decisions

**Templates, not raw lines.** Counting raw lines is useless for logs because almost every line is unique. Collapsing digits is a crude but effective template extractor: it covers ids, durations, ports, sizes and timestamps without any configuration.

**Own hash table instead of a library.** The point of the project is to show I can write and test the data structure. It is 90 lines and the test suite exercises growth (the fixture is small, so growth is also checked by the 3000 character line case and in the sanitizer build).

**Errors go to stderr and the exit code, never to stdout.** A missing file is reported and processing continues with the next file. Exit code 2 for usage errors, 1 for I/O errors, 0 otherwise, so the tool composes in scripts.

**Shell tests over a C test framework.** The binary is the interface. Sixteen cases cover level detection, digit collapsing, filters, stdin, JSON escaping, missing files, bad arguments, empty input and a 2 KB line, which is what a user would actually hit.

## Things I got wrong on the way

The first build segfaulted on every input. The cause was two missing `#define _POSIX_C_SOURCE 200809L` lines: without them `strdup` and `strncasecmp` are implicitly declared as returning `int`, so a 64 bit pointer was truncated to 32 bits. `-Wimplicit-function-declaration` flagged it and AddressSanitizer pinned the crash to `strcmp` on the truncated pointer. The lesson is to read the warnings before the output.

Two test expectations were also wrong on the first run (I miscounted fixture lines). The program was right; the tests were fixed.

## Known limitations

- Level detection takes the **first** token that looks like a level. A line like `INFO user said "error"` is INFO, which is right, but `message: ERROR in module INFO` is ERROR, which may not be.
- Digit collapsing also hits version strings and hex ids partially (`v2.3.1` becomes `v#.#.#`, `0xdeadbeef` stays mixed). Good enough for grouping, not for parsing.
- Templates are capped at 512 bytes; longer messages are truncated for grouping purposes only.
- No timestamp parsing, so there is no rate over time. That is the obvious next feature.
- Multi line entries (stack traces) are counted as separate lines.

## Roadmap

- `-t` time bucketing (lines per minute) once timestamp detection is in
- `-i` ignore list of templates
- a `--since`/`--until` filter

## License

MIT
