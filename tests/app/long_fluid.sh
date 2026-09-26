#!/bin/sh
set -eu

timeout 120 ./build/dungeoncraft --smoke-motion-long
test -s build/screenshots/after_5s.bmp
test -s build/screenshots/after_10s.bmp
cmp -s build/screenshots/after_5s.bmp build/screenshots/after_10s.bmp && {
    echo 'Fluid scene stopped changing between 5s and 10s' >&2
    exit 1
}
