#!/bin/sh

script_path=$(readlink -f "$0")
script_dir=$(dirname "${script_path}")
"${script_dir}/../imhex" "$@" > /dev/null 2>&1 &