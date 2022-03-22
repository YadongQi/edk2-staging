#!/bin/bash

# https://github.com/tianocore/tianocore.github.io/wiki/Common-instructions
#git submodule update --init
#make -C BaseTools

source edksetup.sh
build -p OvmfPkg/OvmfPkgX64.dsc -t GCC5 -a X64  -D DEBUG_ON_SERIAL_PORT=TRUE
#-b RELEASE
