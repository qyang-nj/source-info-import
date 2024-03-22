#!/bin/zsh
set -e

# The path of LLVM build directory
LLVM_BUILD_DIR="$HOME/OpenSource/swift-project/build/Ninja-ReleaseAssert/llvm-macosx-arm64"

DEBUG_FLAGS=(-g -O0)
RELEASE_FLAGS=(-O3 -DNDEBUG)

xcrun clang++ ${RELEASE_FLAGS[@]} \
    $($LLVM_BUILD_DIR/bin/llvm-config --cxxflags --ldflags --libs) \
    -o source-info-import \
    -lcurses \
    srcs/source-info-import.cpp \
    srcs/SwiftSourceInfo.cpp

zip -r source-info-import_macos-$(arch).zip source-info-import
