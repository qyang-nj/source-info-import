#include "SwiftSourceInfo.h"
#include "SwiftInternals.h"
#include "llvm/Support/Chrono.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/Path.h"

// Allocate new memory for the given data and store the pointer to prevent the memory from being
// freed.
static StringRef duplicateData(StringRef Data,
                               SmallVector<std::unique_ptr<char[]>, 4> &PointerHolder) {
  std::unique_ptr<char[]> NewDataPtr(new char[Data.size()]);
  std::memcpy(NewDataPtr.get(), Data.data(), Data.size());
  auto NewData = StringRef(NewDataPtr.get(), Data.size());
  PointerHolder.push_back(std::move(NewDataPtr));
  return NewData;
}

static StringRef filePathFromID(int ID, StringRef TextDataData) {
  auto filePath = TextDataData.substr(ID);
  size_t terminatorOffset = filePath.find('\0');
  return filePath.slice(0, terminatorOffset);
}

static void printSourceListInfo(StringRef SourceFileListData, StringRef TextDataData) {
  auto *Cursor = SourceFileListData.bytes_begin();
  auto *End = SourceFileListData.bytes_end();
  while (Cursor < End) {
    auto Record = reinterpret_cast<const SourceFileRecord *>(Cursor);

    llvm::outs() << llvm::formatv(
        "  {0} ({1}, {2} bytes)\n", filePathFromID(Record->FileID, TextDataData),
        llvm::sys::TimePoint<>(std::chrono::nanoseconds(Record->Timestamp)), Record->FileSize);

    Cursor += sizeof(SourceFileRecord);
  }
}

static void printUSRInfo(std::pair<SmallVector<uint64_t>, StringRef> DeclUSRsData,
                         StringRef BasicDeclLocsData, StringRef TextDataData) {
  std::unique_ptr<SerializedDeclUSRTable> DeclUSRsTable =
      readDeclUSRsTable(DeclUSRsData.first, DeclUSRsData.second);

  for (const auto &key : DeclUSRsTable->keys()) {
    auto Val = DeclUSRsTable->find(key);

    auto UsrId = *Val;
    auto Record = reinterpret_cast<const DeclLocRecord *>(BasicDeclLocsData.data() +
                                                          sizeof(DeclLocRecord) * UsrId);

    auto filePath = filePathFromID(Record->FileID, TextDataData);

    llvm::outs() << llvm::formatv("  {0} ({1}:{2}:{3})\n", key, llvm::sys::path::filename(filePath),
                                  Record->Locs[0].Line, Record->Locs[0].Column);
  }
}

void SwiftSourceInfo::printContent() {
  llvm::outs() << "Source Files:\n";
  printSourceListInfo(SourceFileListData, TextDataData);

  llvm::outs() << "USRs:\n";
  printUSRInfo(DeclUSRsData, BasicDeclLocsData, TextDataData);
}

void SwiftSourceInfo::remapFilePath(FileIDRemapper &FIDRemapper, bool Quiet) {
  // Remap SourceFileListData
  auto NewSourceFileListData = duplicateData(SourceFileListData, PointerHolder);
  auto *Cursor = NewSourceFileListData.bytes_begin();
  auto *End = NewSourceFileListData.bytes_end();
  while (Cursor < End) {
    auto *Record = reinterpret_cast<SourceFileRecord *>(const_cast<unsigned char *>(Cursor));
    Record->FileID = FIDRemapper.mapFileID(Record->FileID, TextDataData, Quiet);
    Cursor += sizeof(SourceFileRecord);
  }
  SourceFileListData = NewSourceFileListData;

  // Remap BasicDeclLocsData
  auto NewBasicDeclLocsData = duplicateData(BasicDeclLocsData, PointerHolder);
  Cursor = NewBasicDeclLocsData.bytes_begin();
  End = NewBasicDeclLocsData.bytes_end();
  while (Cursor < End) {
    auto *Record = reinterpret_cast<DeclLocRecord *>(const_cast<unsigned char *>(Cursor));
    Record->FileID = FIDRemapper.mapFileID(Record->FileID, TextDataData, Quiet);

    for (int i = 0; i < 3; i++) {
      Record->Locs[i].FileID = FIDRemapper.mapFileID(Record->Locs[i].FileID, TextDataData, Quiet);
    }
    Cursor += sizeof(DeclLocRecord);
  }
  BasicDeclLocsData = NewBasicDeclLocsData;

  // Remap DocRangesData
  auto NewDocRangesData = duplicateData(DocRangesData, PointerHolder);
  Cursor = NewDocRangesData.bytes_begin();
  End = NewDocRangesData.bytes_end();
  Cursor += 1; // Skip the reserved number
  while (Cursor < End) {
    uint32_t Nums = *reinterpret_cast<const uint32_t *>(Cursor);
    Cursor += 4;

    for (int i = 0; i < Nums; i++) {
      auto Record = reinterpret_cast<DocRangeRecord *>(const_cast<unsigned char *>(Cursor));
      Record->Loc.FileID = FIDRemapper.mapFileID(Record->Loc.FileID, TextDataData, Quiet);
      Cursor += sizeof(DocRangeRecord);
    }
  }
  DocRangesData = NewDocRangesData;

  // Remap TextDataData
  TextDataData = duplicateData(FIDRemapper.getNewTextDataData(), PointerHolder);
}
