#!/bin/bash

BASE_DIR="$(dirname "$(readlink -f "${0}")")"

source ${BASE_DIR}/AppRun.env

exec ${BASE_DIR}/usr/bin/imhex "$@"