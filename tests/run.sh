#!/bin/sh
# Minimal shell test harness: each case runs the binary and compares a grep of its output.
set -u
BIN=build/logparse
FIX=tests/fixtures
pass=0; fail=0

check() {  # check NAME EXPECTED_SUBSTRING COMMAND...
    name=$1; want=$2; shift 2
    out=$("$@" 2>&1)
    if printf '%s' "$out" | grep -qF -- "$want"; then
        pass=$((pass+1)); echo "ok   $name"
    else
        fail=$((fail+1)); echo "FAIL $name"; echo "  wanted: $want"; echo "  got:"; printf '%s\n' "$out" | sed 's/^/    /'
    fi
}

check "counts lines"            "lines      12"                       $BIN $FIX/app.log
check "detects levels"          "DEBUG=1 INFO=4 WARN=1 ERROR=4 FATAL=1 UNKNOWN=1" $BIN $FIX/app.log
check "error rate"              "error rate 45.45%"                   $BIN $FIX/app.log
check "templates collapse digits" "       3  db connection refused (attempt #)" $BIN $FIX/app.log
check "login lines merge"       "       3  user # logged in from #.#.#.#" $BIN $FIX/app.log
check "bracket level"           "       1  worker # crashed: signal #"  $BIN -n 20 $FIX/app.log
check "min level filter"        "distinct   4 templates"               $BIN -l error $FIX/app.log
check "grep filter"             "matched    3"                         $BIN -g "logged in" $FIX/app.log
check "top n"                   "top 2 templates:"                     $BIN -n 2 $FIX/app.log
check "stdin"                   "lines      12"                        sh -c "cat $FIX/app.log | $BIN"
check "json shape"              '"levels":{"DEBUG":1,"INFO":4'         $BIN -j $FIX/app.log
check "json escapes quotes"     '\"ok\"'                               sh -c "printf 'INFO said \"ok\"\n' | $BIN -j"
check "missing file"            "No such file"                         $BIN /nonexistent.log
check "bad level"               "unknown level"                        $BIN -l loud $FIX/app.log
check "empty input"             "lines      0"                         sh -c "printf '' | $BIN"
check "unicode and long line"   "lines      1"                         sh -c "python3 -c \"print('INFO ' + 'ação ' * 400)\" | $BIN"

echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
