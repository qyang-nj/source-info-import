#ifndef SWIFT_SOURCE_INFO_H
#define SWIFT_SOURCE_INFO_H

#include "Remapper.h"
#include "llvm/ADT/StringRef.h"

class SwiftSourceInfo {
public:
  // The binary content of SourceFileList, which is a list of fix-sized records representing the
  // source file information
  StringRef SourceFileListData;

  // The binary content of BasicDeclLocs, which is a list of fix-sized records representing the USR
  // location information
  StringRef BasicDeclLocsData;

  // The binary content of DocRanges, which have records representing the USR's documentation
  // ranges
  StringRef DocRangesData;

  // The binary content of TextData, which is a '\0' terminated strings of actual file paths
  StringRef TextDataData;

  // The binary content DeclUSRs, which is a serialized  `llvm::OnDiskIterableChainedHashTable`.
  // The key is a USR and the value is the index of records in BASIC_DECL_LOCS
  std::pair<SmallVector<uint64_t>, StringRef> DeclUSRsData;

  SwiftSourceInfo(StringRef TextDataData, StringRef SourceFileListData, StringRef BasicDeclLocsData,
                  StringRef DocRangesData, std::pair<SmallVector<uint64_t>, StringRef> DeclUSRsData)
      : TextDataData(TextDataData), SourceFileListData(SourceFileListData),
        BasicDeclLocsData(BasicDeclLocsData), DocRangesData(DocRangesData),
        DeclUSRsData(DeclUSRsData) {}

  // Print the human readable content of the source info
  void printContent();

  // Remap the file paths in the source info
  void remapFilePath(FileIDRemapper &FIDRemapper, bool Quiet);

private:
  // When remapping the file paths, we need allocate new memory spaces for the new data of the above
  // sections. Therefore, we need hold the pointers to prevent the memory from being freed.
  SmallVector<std::unique_ptr<char[]>, 4> PointerHolder;
};

#endif // SWIFT_SOURCE_INFO_H
