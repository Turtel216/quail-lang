#pragma once

#include <cstddef>
#include <location.hh>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

namespace ff {
namespace drv {

/* The text of one file, recorded as the scanner consumes it, along with the
 * offset at which each line begins. That index is what lets an error quote
 * the code that caused it instead of just naming a line number. */
class SourceFile {
private:
  std::string name;
  std::ostringstream stringStream;
  std::string contents;
  std::size_t offset = 0;
  std::vector<std::size_t> lineOffsets;

  std::size_t getIndex(int line, int column) const;
  std::size_t getLineEnd(int line) const;

public:
  explicit SourceFile(std::string _name)
      : name(std::move(_name)), lineOffsets{0} {}

  void write(const char *buffer, std::size_t length);
  void markLine();
  /* Move the recorded text into place once the scanner is done with it. */
  void finalize();

  void print(std::ostream &to, const yy::location &loc, bool highlight) const;

  inline const std::string &getName() const noexcept { return this->name; }
  /* Locations refer back to their file through this pointer, so it must
   * outlive every location the parser produces. */
  inline std::string *getNamePointer() noexcept { return &this->name; }
};

/* Every file the compiler has read. A location names its file, which is how
 * an error finds its way back to the right text. */
class FileManager {
private:
  std::vector<std::unique_ptr<SourceFile>> files;

public:
  SourceFile &open(std::string name);
  void printLocation(std::ostream &to, const yy::location &loc,
                     bool highlight = true) const;
};

} // namespace drv
} // namespace ff
