#!/bin/bash
# Input parameters
APP_NAME=$1
SOURCE_FILE=$2
BUILD_DIR=$3
DEV_USER_INC_DIR=$4
DOCA_DEV_USER_LIB=$5

# Tools location - DPACC, DPA compiler
DOCA_TOOLS="/opt/mellanox/doca/tools"
DPACC="${DOCA_TOOLS}/dpacc"

# CC flags
DEV_CC_FLAGS="-Wall,-Wextra,-Wpedantic,-Werror,-O3,-DE_MODE_LE,-ffreestanding,-mabi=lp64,-mno-relax,-mcmodel=medany,-nostdlib,-Wdouble-promotion"
DEV_INC_DIR="-I/opt/mellanox/flexio/include,-I/opt/mellanox/doca/include"
DEVICE_OPTIONS="${DEV_CC_FLAGS},${DEV_INC_DIR},${DEV_USER_INC_DIR}"

DOCA_DEV_LIB_DIR="/opt/mellanox/doca/lib/$(arch)-linux-gnu/"
# Host flags
HOST_OPTIONS="-Wno-deprecated-declarations"

mkdir -p "${BUILD_DIR}"

# Compile the DPA (kernel) device source code using the DPACC
${DPACC} ${SOURCE_FILE} -o "${BUILD_DIR}/${APP_NAME}.a" \
        -hostcc=gcc \
		-hostcc-options="${HOST_OPTIONS}" \
    --devicecc-options="${DEVICE_OPTIONS}" \
    --device-libs="-L${DOCA_DEV_LIB_DIR} ${DOCA_DEV_USER_LIB}" \
		--app-name="${APP_NAME}"
		# --keep