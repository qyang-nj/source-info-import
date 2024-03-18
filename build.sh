#!/bin/zsh

# The path where https://github.com/apple/swift is checked out
SWIFT_CHECKOUT=$HOME/OpenSource/swift-project/swift

DEBUG_FLAGS=(-g -O0)
RELEASE_FLAGS=(-O3 -DNDEBUG)

xcrun clang++ ${RELEASE_FLAGS[@]} \
    $($SWIFT_CHECKOUT/../llvm-project/bin/llvm-config --cxxflags --ldflags --libs) \
    -o source-info-import \
    -lcurses \
    srcs/source-info-import.cpp \
    srcs/SwiftSourceInfo.cpp
