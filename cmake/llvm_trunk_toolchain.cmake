set(LLVM_ROOT "$ENV{HOME}/opt/clang-p2996")

set(CMAKE_C_COMPILER "${LLVM_ROOT}/bin/clang")
set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang++")

set(CMAKE_CXX_FLAGS_INIT "-nostdinc++ -fexperimental-library -freflection-latest -isystem ${LLVM_ROOT}/include/c++/v1")

set(LINKER_FLAGS "-nostdlib++ -fexperimental-library -L${LLVM_ROOT}/lib -Wl,-rpath,${LLVM_ROOT}/lib -lc++ -lc++abi")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${LINKER_FLAGS}")
