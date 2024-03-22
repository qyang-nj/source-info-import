# source-info-import
A tool to **inspect** `.swiftsourceinfo` file and **remap** the absolute paths in it.

## Overview
The `.swiftsourceinfo` file is generated by the Swift compiler during compilation, along with `.swiftmodule` and `.swiftdoc`. It is used for diagnosis and code navigation. We have identified [a case](https://github.com/qyang-nj/llios/blob/main/articles/SwiftSourceInfo.md#the-usage) where jump-to-definition requires the `.swiftsource` file, and IndexStore alone does not suffice. Additionally, the file is potentially used by the debugger.

However, the `.swiftsourceinfo` file [always embeds absolute paths](https://github.com/apple/swift/blob/c2ca810126074406f03dc29a44f4ad4b12f04c79/lib/Serialization/SerializeDoc.cpp#L765-L767), which can hinder local indexing and debugging if the file is downloaded from a remote cache. In such cases, it is necessary to remap the server paths to the local paths.

## Usage
* The tool can be used to remap absolute source paths. Inspired by [`index-import`](https://github.com/MobileNativeFoundation/index-import?tab=readme-ov-file), it accepts an `--remap` option in the format of `regex=replacement`. Multiple `--remap` options can be accumulated.
```
$ ./source-info-import --remap="/Users/.*/MyProject=/new/path/MyProject" FooLibrary.swiftmodule/Project/arm64-apple-ios-simulator.swiftsourceinfo new.swiftsourceinfo
/Users/xyz/MyProject/FooLibrary/Foo.swift -> /new/path/MyProject/FooLibrary/Foo.swift
/Users/xyz/MyProject/FooLibrary/Bar.swift -> /new/path/MyProject/FooLibrary/Bar.swift
```

* This tool can also be used to inspect the content of a `.swiftsourceinfo` file. Simply provide the file path without the `--remap` option.
```
$ ./source-info-import FooLibrary.swiftmodule/Project/arm64-apple-ios-simulator.swiftsourceinfo
Source Files:
  /Users/xyz/MyProject/FooLibrary/Foo.swift (2024-03-14 23:54:31.384273393, 417 bytes)
  /Users/xyz/MyProject/FooLibrary/Bar.swift (2024-03-18 10:00:23.816131108, 122 bytes)
USRs:
  s:10FooLibrary0A8ProtocolPA2A0A6StructVRszrlE3fooSSvpZ (Foo.swift:10:21)
  s:10FooLibrary0A6StructV (Foo.swift:7:15)
  s:10FooLibrary3BarC (Bar.swift:6:7)
  s:e:s:10FooLibrary0A8ProtocolPA2A0A6StructVRszrlE3fooSSvpZ (Foo.swift:9:1)
```

## Build Instructions
`source-info-import` depends on the libraries from [Apple's LLVM fork](https://github.com/apple/llvm-project).
1. Follow the steps in [Swift instructions](https://github.com/apple/swift/blob/main/docs/HowToGuides/GettingStarted.md) to setup the environment and dependencies.
1.1 [check the system requirements](https://github.com/apple/swift/blob/main/docs/HowToGuides/GettingStarted.md#system-requirements)
1.2 [clone the source code](https://github.com/apple/swift/blob/main/docs/HowToGuides/GettingStarted.md#cloning-the-project)
1.3 [install dependencies](https://github.com/apple/swift/blob/main/docs/HowToGuides/GettingStarted.md#installing-dependencies)

2. Build the LLVM libraries
```bash
# in `swift` directory
utils/build-script --release --skip-build
ninja -C ../build/Ninja-ReleaseAssert/llvm-macosx-arm64 llvm-libraries llvm-config
```
3. Change the `LLVM_BUILD_DIR` in [`build.sh`](./build.sh) to the correct path, which should be like `swift-project/build/Ninja-ReleaseAssert/llvm-macosx-arm64"`
4. Run `./build.sh`

## File Format
The `.swiftsourceinfo` file, like `.swiftmodule` and `.swiftdoc`, is also in [LLVM Bitstream](https://www.llvm.org/docs/BitCodeFormat.html#bitstream-format) format. [This article](https://github.com/qyang-nj/llios/blob/main/articles/SwiftSourceInfo.md#the-file-format) provides a detailed description of the format.
