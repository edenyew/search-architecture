#pragma once

#include <string>
#include <vector>

namespace common {

// One row from data/corpus.jsonl: {"id": "...", "text": "..."}.
struct CorpusDoc {
  std::string id;
  std::string text;
};

// Reads a JSONL file where every line is {"id": ..., "text": ...}
// and returns one CorpusDoc per line, in file order.
std::vector<CorpusDoc> LoadCorpus(const std::string& path);

}  // namespace common
