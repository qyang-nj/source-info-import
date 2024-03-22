#ifndef REMAPPER_H
#define REMAPPER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/raw_ostream.h"
#include <regex>

using namespace llvm;

// Remaps file paths with provided regexes and replacements
class FilePathRemapper {
  std::vector<std::pair<std::regex, std::string>> Remaps;

public:
  std::string remap(StringRef input) const {
    std::string result = input.str();
    for (const auto &remap : Remaps) {
      const auto &pattern = remap.first;
      const auto &replacement = remap.second;
      result = std::regex_replace(input.str(), pattern, replacement);
    }

    return result;
  }

  void addRemap(std::regex &pattern, const std::string &replacement) {
    Remaps.emplace_back(pattern, replacement);
  }
};

// Remaps the FileID, which is the offset into the TextData blob.
// This class also provides the remapped TextData blob.
class FileIDRemapper {
  FilePathRemapper &PathRemapper;
  DenseMap<uint32_t, uint32_t> IndexMap;
  SmallString<1024> Buffer;

public:
  FileIDRemapper(FilePathRemapper &PathRemapper) : PathRemapper(PathRemapper) {}

  uint32_t mapFileID(uint32_t FileID, StringRef TextDataData, bool Quiet) {
    if (IndexMap.count(FileID) == 0) {
      IndexMap[FileID] = Buffer.size();

      auto OldPath = TextDataData.substr(FileID);
      size_t terminatorOffset = OldPath.find('\0');
      OldPath = OldPath.slice(0, terminatorOffset);

      auto NewPath = PathRemapper.remap(OldPath.str());
      Buffer.append(NewPath);
      Buffer.push_back('\0');

      if (!Quiet)
        llvm::outs() << llvm::formatv("{0} -> {1}\n", OldPath.size() > 0 ? OldPath : "(Empty)",
                                      NewPath.size() > 0 ? NewPath : "(Empty)");
    }

    return IndexMap[FileID];
  }

  StringRef getNewTextDataData() { return StringRef(Buffer); }
};

#endif // REMAPPER_H
