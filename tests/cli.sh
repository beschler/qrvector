#!/bin/sh
# Copyright (c) 2026 Zachary Beschler
# SPDX-License-Identifier: MIT
set -eu
tool=./qrvector
fixtures=build/fixtures
"$tool" --help > "$fixtures/help.txt"
"$tool" -h > "$fixtures/short-help.txt"
cmp "$fixtures/help.txt" "$fixtures/short-help.txt"
"$tool" --version > "$fixtures/version.txt"
"$tool" "$fixtures/sample.jpg" -o "$fixtures/cli.svg" --force
cmp "$fixtures/sample.svg" "$fixtures/cli.svg"
"$tool" "$fixtures/sample.png" -o - > "$fixtures/stdout.svg" 2> "$fixtures/stdout.log"
cmp "$fixtures/sample.svg" "$fixtures/stdout.svg"
"$tool" --output "$fixtures/long-output.svg" "$fixtures/sample.png" --force
cmp "$fixtures/sample.svg" "$fixtures/long-output.svg"
cp "$fixtures/sample.png" "$fixtures/-input.png"
(
    cd "$fixtures"
    ../../qrvector -o dashed.svg --force -- -input.png
)
cmp "$fixtures/sample.svg" "$fixtures/dashed.svg"
cp "$fixtures/sample.png" "$fixtures/QR image.png"
"$tool" "$fixtures/QR image.png" --output "$fixtures/QR vector.svg" --force
cmp "$fixtures/sample.svg" "$fixtures/QR vector.svg"
"$tool" "$fixtures/sample.png" --margin 2 -o "$fixtures/explicit-margin.svg" --force
cmp "$fixtures/sample.svg" "$fixtures/explicit-margin.svg"
"$tool" "$fixtures/sample.png" --margin 4 -o "$fixtures/standard-cli.svg" --force
cmp "$fixtures/standard-margin.svg" "$fixtures/standard-cli.svg"
"$tool" "$fixtures/sample.png" -m 4 -o "$fixtures/standard-cli.svg" --force
cmp "$fixtures/standard-margin.svg" "$fixtures/standard-cli.svg"
for margin in -1 65 2.5 2x abc ''; do
    if "$tool" "$fixtures/sample.png" --margin "$margin" -o "$fixtures/bad-margin.svg" > /dev/null 2>&1; then exit 1; fi
done
test ! -e "$fixtures/bad-margin.svg"
if "$tool" "$fixtures/sample.png" --margin > /dev/null 2>&1; then exit 1; fi
if "$tool" "$fixtures/sample.png" -o "$fixtures/cli.svg" > /dev/null 2>&1; then exit 1; fi
cmp "$fixtures/sample.svg" "$fixtures/cli.svg"
if "$tool" "$fixtures/sample.png" -o "$fixtures/sample.png" --force > /dev/null 2>&1; then exit 1; fi
if "$tool" "$fixtures/not-image.png" -o "$fixtures/invalid.svg" > /dev/null 2>&1; then exit 1; fi
test ! -e "$fixtures/invalid.svg"
if "$tool" --unknown > /dev/null 2>&1; then exit 1; fi
if "$tool" > /dev/null 2>&1; then exit 1; fi
# Assert documented exit codes, including unsupported joined flag values and multiple inputs.
expect_exit() {
    expected=$1
    shift
    actual=0
    "$@" > "$fixtures/exit-stdout.txt" 2> "$fixtures/exit-stderr.txt" || actual=$?
    if test "$actual" -ne "$expected"; then
        printf 'Expected exit %s, got %s\n' "$expected" "$actual" >&2
        cat "$fixtures/exit-stderr.txt" >&2
        exit 1
    fi
}
expect_exit 0 "$tool" --version
expect_exit 1 "$tool" "$fixtures/not-image.png"
expect_exit 1 "$tool" "$fixtures/sample.png" -o "$fixtures/cli.svg"
expect_exit 2 "$tool"
expect_exit 2 "$tool" "$fixtures/sample.png" --margin=4
expect_exit 2 "$tool" "$fixtures/sample.png" "$fixtures/sample.jpg"
"$tool" "$fixtures/sample.png" --force
cmp "$fixtures/sample.svg" "$fixtures/stdout.svg"
echo 'Passed CLI conversion, margin options, stdout, default output and failure handling.'
