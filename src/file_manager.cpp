#include "../include/file_manager.hpp"
#include <algorithm>
#include <cassert>
#include <utility>

namespace ff {
namespace drv {

void SourceFile::write(const char *buffer, std::size_t length) {
  stringStream.write(buffer, static_cast<std::streamsize>(length));
  offset += length;
}

void SourceFile::markLine() { lineOffsets.push_back(offset); }

void SourceFile::finalize() { contents = stringStream.str(); }

std::size_t SourceFile::getIndex(int line, int column) const {
  assert(line > 0 && std::cmp_less_equal(line, lineOffsets.size()));
  assert(column > 0);
  return lineOffsets.at(static_cast<std::size_t>(line) - 1) +
         static_cast<std::size_t>(column) - 1;
}

std::size_t SourceFile::getLineEnd(int line) const {
  if (std::cmp_equal(line, lineOffsets.size()))
    return contents.size();
  return getIndex(line + 1, 1);
}

void SourceFile::print(std::ostream &to, const yy::location &loc,
                       bool highlight) const {
  /* The scanner stops reading at the first error, so a location may reach
   * past the text we have recorded. */
  const std::size_t limit = contents.size();
  std::size_t printStart = std::min(getIndex(loc.begin.line, 1), limit);
  std::size_t highlightStart =
      std::min(getIndex(loc.begin.line, loc.begin.column), limit);
  std::size_t highlightEnd =
      std::min(getIndex(loc.end.line, loc.end.column), limit);
  std::size_t printEnd = std::min(getLineEnd(loc.end.line), limit);

  const char *text = contents.c_str();
  to.write(text + printStart,
           static_cast<std::streamsize>(highlightStart - printStart));
  if (highlight)
    to << "\033[4;31m";
  to.write(text + highlightStart,
           static_cast<std::streamsize>(highlightEnd - highlightStart));
  if (highlight)
    to << "\033[0m";
  to.write(text + highlightEnd,
           static_cast<std::streamsize>(printEnd - highlightEnd));

  /* The last line of a file need not end in a newline. */
  if (printEnd == 0 || contents[printEnd - 1] != '\n')
    to << std::endl;
}

SourceFile &FileManager::open(std::string name) {
  files.push_back(std::unique_ptr<SourceFile>(new SourceFile(std::move(name))));
  return *files.back();
}

void FileManager::printLocation(std::ostream &to, const yy::location &loc,
                                bool highlight) const {
  if (!loc.begin.filename)
    return;

  for (auto &file : files) {
    if (file->getName() == *loc.begin.filename) {
      file->print(to, loc, highlight);
      return;
    }
  }
}

} // namespace drv
} // namespace ff
