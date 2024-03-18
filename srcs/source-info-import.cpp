#include "Remapper.h"
#include "SwiftInternals.h"
#include "SwiftSourceInfo.h"
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Bitstream/BitstreamWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <iomanip>
#include <iostream>
#include <regex>

using namespace llvm;
using namespace llvm::support;

#define DEBUG_TYPE "debug"

#define RETURN_IF_ERROR(Err)                                                                       \
  if (auto _err = (Err)) {                                                                         \
    return std::move(_err);                                                                        \
  }

#define UNEXPECTED_BIT_ERROR                                                                       \
  llvm::createStringError(std::errc::illegal_byte_sequence, "Unexpected bit in bitstream. (%d)",   \
                          __LINE__)

static cl::OptionCategory DefaultCategory("Basic Options");
static cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<src>"), cl::init("-"),
                                          cl::cat(DefaultCategory));
static cl::opt<std::string> OutputFilename(cl::Positional, cl::desc("<dst>"), cl::init("-"),
                                           cl::cat(DefaultCategory));
static cl::list<std::string> PathRemaps("remap", cl::desc("Path remapping substitution."),
                                        cl::value_desc("regex=replacement"),
                                        cl::cat(DefaultCategory));
static cl::opt<bool> Quiet("quiet", cl::desc("Suppress any output"), cl::init(false),
                           cl::cat(DefaultCategory));
static cl::opt<bool, true> Debug("debug", cl::desc("Enable debug output"), cl::Hidden,
                                 cl::location(DebugFlag));

static bool checkMagicNumber(BitstreamCursor &cursor) {
  for (unsigned char byte : SWIFTSOURCEINFO_SIGNATURE) {
    Expected<SimpleBitstreamCursor::word_t> maybeRead = cursor.Read(8);
    if (maybeRead.get() != byte) {
      return false;
    }
  }
  return true;
}

static Expected<std::unique_ptr<SwiftSourceInfo>> parseSwiftSourceInfo(BitstreamCursor &Cursor) {
  StringRef SourceFileListData;
  StringRef BasicDeclLocsData;
  std::pair<SmallVector<uint64_t>, StringRef> DeclUSRsData;
  StringRef TextDataData;
  StringRef DocRangesData;

  unsigned BlockID = 0;

  while (!Cursor.AtEndOfStream()) {
    BitstreamEntry Entry;
    RETURN_IF_ERROR(Cursor.advance().moveInto(Entry));

    switch (Entry.Kind) {
    case BitstreamEntry::SubBlock:
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::SubBlock]\tID: " << Entry.ID << "\n");
      if (Entry.ID != MODULE_SOURCEINFO_BLOCK_ID && Entry.ID != DECL_LOCS_BLOCK_ID) {
        RETURN_IF_ERROR(Cursor.SkipBlock());
      } else {
        RETURN_IF_ERROR(Cursor.EnterSubBlock(Entry.ID));
      }
      BlockID = Entry.ID;
      break;
    case BitstreamEntry::EndBlock:
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::EndBlock]\tID: \n");
      break;

    case BitstreamEntry::Record: {
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::Record]\tID: " << Entry.ID << "\n");
      if (BlockID != DECL_LOCS_BLOCK_ID) {
        auto V = Cursor.skipRecord(Entry.ID);
        if (!V)
          return V.takeError();
        break;
      }
      SmallVector<uint64_t> Record;
      StringRef Blob;
      unsigned Code;
      RETURN_IF_ERROR(Cursor.readRecord(Entry.ID, Record, &Blob).moveInto(Code))

      switch (Entry.ID) {
      case SOURCE_FILE_LIST_ABBREV_ID:
        SourceFileListData = Blob;
        break;
      case BASIC_DECL_LOCS_ABBREV_ID:
        BasicDeclLocsData = Blob;
        break;
      case DECL_USRS_ABBREV_ID:
        DeclUSRsData = {Record, Blob};
        break;
      case TEXT_DATA_ABBREV_ID:
        TextDataData = Blob;
        break;
      case DOC_RANGES_ABBREV_ID:
        DocRangesData = Blob;
        break;
      default:
        return UNEXPECTED_BIT_ERROR;
      }
      break;
    }
    default:
      return UNEXPECTED_BIT_ERROR;
    }
  }

  return std::make_unique<SwiftSourceInfo>(TextDataData, SourceFileListData, BasicDeclLocsData,
                                           DocRangesData, std::move(DeclUSRsData));
}

static Error rewriteSwiftSourceInfo(SwiftSourceInfo &SSI, BitstreamCursor &Cursor,
                                    BitstreamWriter &Writer) {
  RETURN_IF_ERROR(Cursor.JumpToBit(sizeof(SWIFTSOURCEINFO_SIGNATURE) * 8));
  for (unsigned char byte : SWIFTSOURCEINFO_SIGNATURE)
    Writer.Emit(byte, 8);

  int NumAbbrevs = 0;
  unsigned BlockID = 0;

  while (!Cursor.AtEndOfStream()) {
    BitstreamEntry Entry;
    RETURN_IF_ERROR(Cursor.advance(BitstreamCursor::AF_DontAutoprocessAbbrevs).moveInto(Entry));

    switch (Entry.Kind) {
    case BitstreamEntry::SubBlock: {
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::SubBlock]\tID: " << Entry.ID << "\n");
      BlockID = Entry.ID;
      RETURN_IF_ERROR(Cursor.EnterSubBlock(Entry.ID));
      Writer.EnterSubblock(Entry.ID, Cursor.getAbbrevIDWidth());
      break;
    }
    case BitstreamEntry::EndBlock: {
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::EndBlock]\n");
      Writer.ExitBlock();
      NumAbbrevs = 0;
      break;
    }
    case BitstreamEntry::Record: {
      LLVM_DEBUG(dbgs() << "[BitstreamEntry::Record]\t");
      if (Entry.ID == bitc::DEFINE_ABBREV) {
        LLVM_DEBUG(dbgs() << "bitc::DEFINE_ABBREV\t");
        RETURN_IF_ERROR(Cursor.ReadAbbrevRecord());

        NumAbbrevs++;

        auto AbbvID = NumAbbrevs + bitc::FIRST_APPLICATION_ABBREV - 1;
        Expected<const BitCodeAbbrev *> MaybeAbbv = Cursor.getAbbrev(AbbvID);
        if (!MaybeAbbv)
          return MaybeAbbv.takeError();
        auto Abbv = MaybeAbbv.get();
        LLVM_DEBUG(dbgs() << "NumOperand: " << Abbv->getNumOperandInfos() << "\n");

        Writer.EmitAbbrev(std::make_shared<BitCodeAbbrev>(*Abbv));

      } else {
        SmallVector<uint64_t, 64> Record;
        StringRef Blob;
        unsigned Code;

        RETURN_IF_ERROR(Cursor.readRecord(Entry.ID, Record, &Blob).moveInto(Code))

        if (Entry.ID == bitc::UNABBREV_RECORD) {
          LLVM_DEBUG(dbgs() << "bitc::UNABBREV_RECORD\n");
          Writer.EmitRecord(Code, Record);
        } else {
          LLVM_DEBUG(dbgs() << "RecordID: " << Entry.ID << "\n");
          Record.insert(Record.begin(), Code);
          if (BlockID == DECL_LOCS_BLOCK_ID) {
            if (Entry.ID == SOURCE_FILE_LIST_ABBREV_ID) {
              Blob = SSI.SourceFileListData;
            } else if (Entry.ID == BASIC_DECL_LOCS_ABBREV_ID) {
              Blob = SSI.BasicDeclLocsData;
            } else if (Entry.ID == TEXT_DATA_ABBREV_ID) {
              Blob = SSI.TextDataData;
            } else if (Entry.ID == DOC_RANGES_ABBREV_ID) {
              Blob = SSI.DocRangesData;
            } else if (Entry.ID == DECL_USRS_ABBREV_ID) {
              // We don't need to rewrite DeclUSRsData, because it doesn't reference any file paths.
            }
          }
          Writer.EmitRecordWithBlob(Entry.ID, Record, Blob);
        }
      }
      break;
    }
    default:
      return UNEXPECTED_BIT_ERROR;
    }
  }
  return Error::success();
}

static Expected<std::unique_ptr<MemoryBuffer>> openBitcodeFile(StringRef Path) {
  Expected<std::unique_ptr<MemoryBuffer>> MemBufOrErr =
      errorOrToExpected(MemoryBuffer::getFileOrSTDIN(Path));
  if (Error E = MemBufOrErr.takeError())
    return std::move(E);

  std::unique_ptr<MemoryBuffer> MemBuf = std::move(*MemBufOrErr);
  return std::move(MemBuf);
}

int main(int argc, char **argv) {
  InitLLVM X(argc, argv);
  cl::ParseCommandLineOptions(argc, argv,
                              "The utility to inspect and remap .swiftsourceinfo file.\n");

  ExitOnError ExitOnErr("source-info-import: ");

  std::unique_ptr<MemoryBuffer> MB = ExitOnErr(openBitcodeFile(InputFilename));
  llvm::BitstreamCursor Cursor{MB.get()->getMemBufferRef()};

  if (!checkMagicNumber(Cursor)) {
    ExitOnErr(createStringError(std::errc::invalid_argument,
                                "The input is not a .swiftsourceinfo file."));
  }

  Expected<std::unique_ptr<SwiftSourceInfo>> MaybeSourceInfo = parseSwiftSourceInfo(Cursor);
  if (!MaybeSourceInfo)
    ExitOnErr(MaybeSourceInfo.takeError());

  std::unique_ptr<SwiftSourceInfo> SSI = std::move(MaybeSourceInfo.get());
  if (PathRemaps.size() > 0) {
    FilePathRemapper FPathRemapper;
    for (const auto &remap : PathRemaps) {
      auto divider = remap.find('=');
      auto pattern = remap.substr(0, divider);
      std::regex re(pattern);
      auto replacement = remap.substr(divider + 1);
      FPathRemapper.addRemap(re, replacement);
    }

    if (OutputFilename == "-") {
      ExitOnErr(createStringError(std::errc::invalid_argument,
                                  "The destination file is required when --remap is specified."));
    }

    FileIDRemapper FIDRemapper(FPathRemapper);
    SSI.get()->remapFilePath(FIDRemapper, Quiet);

    // Write the remapped source info to the output file
    SmallVector<char, 0> Buffer;
    BitstreamWriter Writer{Buffer};
    if (Error Err = rewriteSwiftSourceInfo(*SSI.get(), Cursor, Writer)) {
      ExitOnErr(std::move(Err));
    }

    // Write the buffer the output file
    std::error_code EC;
    raw_fd_ostream OutFile(OutputFilename, EC);
    if (EC)
      ExitOnErr(createStringError(EC, EC.message()));
    OutFile.write(reinterpret_cast<const char *>(Buffer.data()), Buffer.size());
  } else {
    // If no --remap is specified, it dumps the original file content.
    SSI.get()->printContent();
  }

  return 0;
}
