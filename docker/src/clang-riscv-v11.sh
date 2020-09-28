#!/bin/sh

set -ex

if [ -z "${xlen}" ]; then
    echo "xlen not defined" >&2
    exit 1
elif [ ${xlen} -eq 64 ]; then
    xabi="lp"
    xmodel="-mcmodel=medany"
elif [ ${xlen} -eq 32 ]; then
    xabi="ilp"
    xmodel="-mcmodel=medlow"
else
    echo "xlen invalid" >&2
    exit 1
fi
if [ -z "${xtarget}" ]; then
    echo "xtarget not defined" >&2
    exit 1
fi
if [ -z "${prefix}" ]; then
    echo "prefix not defined" >&2
    exit 1
fi

host=$(cc -dumpmachine)
xopts="-g -Os"
xcfeatures="-ffunction-sections -fdata-sections -fno-stack-protector -fvisibility=hidden"
xcxxfeatures="${xcfeatures} -fno-use-cxa-atexit"

xcxxdefs="-D_LIBUNWIND_IS_BAREMETAL=1 -D_GNU_SOURCE=1 -D_POSIX_TIMERS=1"
xcxxdefs="${xcxxdefs} -D_LIBCPP_HAS_NO_LIBRARY_ALIGNED_ALLOCATION"
xcxxnothread="-D_LIBCPP_HAS_NO_THREADS=1"

export CC_FOR_TARGET="${CLANG11PATH}/bin/clang"
export AR_FOR_TARGET="${CLANG11PATH}/bin/llvm-ar"
export NM_FOR_TARGET="${CLANG11PATH}/bin/llvm-nm"
export RANLIB_FOR_TARGET="${CLANG11PATH}/bin/llvm-ranlib"
export READELF_FOR_TARGET="${CLANG11PATH}/bin/llvm-readelf"
export AS_FOR_TARGET="${CLANG11PATH}/bin/clang"

jobs=$(nproc)

for abi in i ia iac im imac iaf iafd imf imfd imafc imafdc; do
    if echo "${abi}" | grep -q "d"; then
        fp="d"
    elif echo "${abi}" | grep -q "f"; then
        fp="f"
    else
        fp=""
    fi

    xarch="rv${xlen}${abi}"
    xctarget="-march=${xarch} -mabi=${xabi}${xlen}${fp} ${xmodel}"
    xarchdir="${xarch}"
    xsysroot="${prefix}/${xtarget}/${xarchdir}"
    xcxx_inc="-I${xsysroot}/include"
    xcxx_lib="-L${xsysroot}/lib"
    xcflags="${xctarget} ${xopts} ${xcfeatures}"
    xcxxflags="${xctarget} ${xopts} ${xcxxfeatures} ${xcxxdefs} ${xcxx_inc}"

    export "CFLAGS_FOR_TARGET"="-target ${xtarget} ${xcflags} -Wno-unused-command-line-argument"

    echo "--- cleanup ---"
    rm -rf /toolchain/build

    echo "--- newlib ${xarch} ---"
    mkdir -p /toolchain/build/newlib
    cd /toolchain/build/newlib
    /toolchain/newlib/configure              \
        --host=${host}                       \
        --build=${host}                      \
        --target=${xtarget}                  \
        --prefix=${xsysroot}                 \
        --disable-newlib-supplied-syscalls   \
        --enable-newlib-reent-small          \
        --disable-newlib-fvwrite-in-streamio \
        --disable-newlib-fseek-optimization  \
        --disable-newlib-wide-orient         \
        --enable-newlib-nano-malloc          \
        --disable-newlib-unbuf-stream-opt    \
        --enable-lite-exit                   \
        --enable-newlib-global-atexit        \
        --disable-newlib-nano-formatted-io   \
        --disable-newlib-fvwrite-in-streamio \
        --enable-newlib-io-c99-formats       \
        --enable-newlib-io-float             \
        --disable-newlib-io-long-double      \
        --disable-nls
    make -j${jobs}
    make -j1 install
    mv ${xsysroot}/${xtarget}/* ${xsysroot}/
    rmdir ${xsysroot}/${xtarget}

    echo "--- compiler-rt ${xarch} ---"
    mkdir -p /toolchain/build/compiler-rt
    cd /toolchain/build/compiler-rt
    cmake                                               \
      -G Ninja                                          \
      -DCMAKE_INSTALL_PREFIX=${xsysroot}                \
      -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY    \
      -DCMAKE_SYSTEM_PROCESSOR=riscv                    \
      -DCMAKE_SYSTEM_NAME=Generic                       \
      -DCMAKE_CROSSCOMPILING=ON                         \
      -DCMAKE_CXX_COMPILER_FORCED=TRUE                  \
      -DCMAKE_BUILD_TYPE=Release                        \
      -DCMAKE_C_COMPILER=${CLANG11PATH}/bin/clang       \
      -DCMAKE_CXX_COMPILER=${CLANG11PATH}/bin/clang++   \
      -DCMAKE_LINKER=${CLANG11PATH}/bin/clang           \
      -DCMAKE_AR=${CLANG11PATH}/bin/llvm-ar             \
      -DCMAKE_RANLIB=${CLANG11PATH}/bin/llvm-ranlib     \
      -DCMAKE_C_COMPILER_TARGET=${xtarget}              \
      -DCMAKE_ASM_COMPILER_TARGET=${xtarget}            \
      -DCMAKE_SYSROOT=${xsysroot}                       \
      -DCMAKE_SYSROOT_LINK=${xsysroot}                  \
      -DCMAKE_C_FLAGS=${xcflags}                        \
      -DCMAKE_ASM_FLAGS=${xcflags}                      \
      -DCMAKE_CXX_FLAGS=${xcflags}                      \
      -DCMAKE_EXE_LINKER_FLAGS=-L${xsysroot}/lib        \
      -DLLVM_CONFIG_PATH=${CLANG11PATH}/bin/llvm-config \
      -DLLVM_DEFAULT_TARGET_TRIPLE=${xtarget}           \
      -DLLVM_TARGETS_TO_BUILD=RISCV                     \
      -DLLVM_ENABLE_PIC=OFF                             \
      -DCOMPILER_RT_OS_DIR=baremetal                    \
      -DCOMPILER_RT_BUILD_BUILTINS=ON                   \
      -DCOMPILER_RT_BUILD_SANITIZERS=OFF                \
      -DCOMPILER_RT_BUILD_XRAY=OFF                      \
      -DCOMPILER_RT_BUILD_LIBFUZZER=OFF                 \
      -DCOMPILER_RT_BUILD_PROFILE=OFF                   \
      -DCOMPILER_RT_BAREMETAL_BUILD=ON                  \
      -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON              \
      -DCOMPILER_RT_INCLUDE_TESTS=OFF                   \
      -DCOMPILER_RT_USE_LIBCXX=ON                       \
      -DUNIX=1                                          \
      /toolchain/llvm/compiler-rt
    ninja
    ninja install

    mv ${xsysroot}/lib/baremetal/* ${xsysroot}/lib
    rmdir ${xsysroot}/lib/baremetal
done
