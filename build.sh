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

ZIP_FILE=source-info-import_macos-$(arch).zip
zip $ZIP_FILE source-info-import

if command -v sha256sum >/dev/null 2>&1; then
    sha256sum $ZIP_FILE
fi
