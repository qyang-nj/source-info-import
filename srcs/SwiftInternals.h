#ifndef SWIFT_INTERNALS_H
#define SWIFT_INTERNALS_H

// The file contains the internal data structures and constants for .swiftsourceinfo format.
// Those are not guaranteed to be stable across the Swift versions.

#include "llvm/Support/DJB.h"
#include "llvm/Support/OnDiskHashTable.h"

namespace {
// Magic number for serialized source info files.
// Copied from swift/lib/Serialization/SourceInfoFormat.h
const unsigned char SWIFTSOURCEINFO_SIGNATURE[] = {0xF0, 0x9F, 0x8F, 0x8E};

// Block IDs
// Copied from swift/lib/Serialization/ModuleFormat.h
const unsigned CONTROL_BLOCK_ID = 9;
const unsigned MODULE_SOURCEINFO_BLOCK_ID = 192;
const unsigned DECL_LOCS_BLOCK_ID = 193;

// Abbrev IDs
// Derived from swift/lib/Serialization/SourceInfoFormat.h
const unsigned SOURCE_FILE_LIST_ABBREV_ID = 4;
const unsigned BASIC_DECL_LOCS_ABBREV_ID = 5;
const unsigned DECL_USRS_ABBREV_ID = 6;
const unsigned TEXT_DATA_ABBREV_ID = 7;
const unsigned DOC_RANGES_ABBREV_ID = 8;

// Copied from swift/lib/Serialization/SourceInfoFormat.h
const uint32_t SWIFTSOURCEINFO_HASH_SEED = 5387;

// The memory layout of the items in source info blob
// Derived from swift/lib/Serialization/ModuleFile.cpp
struct __attribute__((packed)) SourceFileRecord {
  uint32_t FileID;
  uint8_t Fingerprint1[32]; // including type members
  uint8_t Fingerprint2[32]; // excluding type members
  uint64_t Timestamp;
  uint64_t FileSize;
};

struct __attribute__((packed)) DeclLocRecord {
  uint32_t FileID;
  uint32_t DocRanges;
  struct __attribute__((packed)) {
    uint32_t Offset;
    uint32_t Line;
    uint32_t Column;
    uint32_t Skipped[3]; // data not interesting for us
    uint32_t FileID;
  } Locs[3];
};

struct __attribute__((packed)) DocRangeRecord {
  struct __attribute__((packed)) {
    uint32_t Skipped[6];
    uint32_t FileID;
  } Loc;
  uint32_t Skipped;
};

// Copied from swift/lib/Serialization/ModuleFileSharedCore.h
class DeclUSRTableInfo {
public:
  using internal_key_type = StringRef;
  using external_key_type = StringRef;
  using data_type = uint32_t;
  using hash_value_type = uint32_t;
  using offset_type = unsigned;

  internal_key_type GetInternalKey(external_key_type key) { return key; }

  external_key_type GetExternalKey(external_key_type key) { return key; }

  hash_value_type ComputeHash(internal_key_type key) {
    assert(!key.empty());
    return llvm::djbHash(key, SWIFTSOURCEINFO_HASH_SEED);
  }

  static bool EqualKey(internal_key_type lhs, internal_key_type rhs) { return lhs == rhs; }

  static std::pair<unsigned, unsigned> ReadKeyDataLength(const uint8_t *&data) {
    using namespace llvm::support;
    unsigned keyLength = endian::readNext<uint32_t, little, unaligned>(data);
    unsigned dataLength = 4;
    return {keyLength, dataLength};
  }

  static internal_key_type ReadKey(const uint8_t *data, unsigned length) {
    return StringRef(reinterpret_cast<const char *>(data), length);
  }

  data_type ReadData(internal_key_type key, const uint8_t *data, unsigned length) {
    using namespace llvm::support;
    assert(length == 4);
    return endian::readNext<uint32_t, little, unaligned>(data);
  }
};

// Copied from swift/lib/Serialization/ModuleFileSharedCore.h
using SerializedDeclUSRTable = llvm::OnDiskIterableChainedHashTable<DeclUSRTableInfo>;

// Copied from swift/lib/Serialization/ModuleFileSharedCore.h
std::unique_ptr<SerializedDeclUSRTable> readDeclUSRsTable(ArrayRef<uint64_t> fields,
                                                          StringRef blobData) {
  if (fields.empty() || blobData.empty())
    return nullptr;
  uint32_t tableOffset = static_cast<uint32_t>(fields.front());
  auto base = reinterpret_cast<const uint8_t *>(blobData.data());
  return std::unique_ptr<SerializedDeclUSRTable>(
      SerializedDeclUSRTable::Create(base + tableOffset, base + sizeof(uint32_t), base));
}
} // namespace

#endif // SWIFT_INTERNALS_H
