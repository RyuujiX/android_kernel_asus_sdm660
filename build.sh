#!/bin/bash
export KBUILD_BUILD_USER=Yuza
export KBUILD_BUILD_HOST=RMX3171
# Compile plox
function compile() {
    make -j$(6) O=out ARCH=arm64 X00TD_defconfig
    make -j$(6) ARCH=arm64 O=out \
                               CROSS_COMPILE=aarch64-linux-gnu- \
                               CROSS_COMPILE_ARM32=arm-linux-gnueabi-
}
compile
