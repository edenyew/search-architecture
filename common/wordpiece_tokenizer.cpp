#include "wordpiece_tokenizer.h"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace common {

WordPieceTokenizer::WordPieceTokenizer(const std::string& vocab_path) {
  std::ifstream file(vocab_path);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open vocab file: " + vocab_path);
  }

  std::string line;
  int64_t id = 0;
  while (std::getline(file, line)) {
    vocab_[line] = id;
    ++id;
  }

  cls_id_ = vocab_.at("[CLS]");
  sep_id_ = vocab_.at("[SEP]");
  unk_id_ = vocab_.at("[UNK]");
}

std::vector<std::string> WordPieceTokenizer::SplitWords(const std::string& text) const {
  std::vector<std::string> words;
  std::string current;

  for (unsigned char c : text) {
    if (std::isalnum(c)) {
      current += static_cast<char>(std::tolower(c));
    } else if (std::isspace(c)) {
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
    } else {
      // Punctuation always splits, and — unlike common::Tokenize()
      // for BM25 — is kept as its own token, not discarded.
      if (!current.empty()) {
        words.push_back(current);
        current.clear();
      }
      words.push_back(std::string(1, static_cast<char>(std::tolower(c))));
    }
  }
  if (!current.empty()) {
    words.push_back(current);
  }

  return words;
}

std::vector<std::string> WordPieceTokenizer::WordPieceSplit(const std::string& word) const {
  std::vector<std::string> pieces;
  size_t start = 0;

  while (start < word.size()) {
    size_t end = word.size();
    std::string matched;
    bool found = false;

    while (end > start) {
      std::string candidate = word.substr(start, end - start);
      if (start > 0) {
        candidate = "##" + candidate;
      }
      if (vocab_.count(candidate) > 0) {
        matched = candidate;
        found = true;
        break;
      }
      --end;
    }

    if (!found) {
      return {"[UNK]"};
    }

    pieces.push_back(matched);
    start = end;
  }

  return pieces;
}

std::vector<int64_t> WordPieceTokenizer::Encode(const std::string& text, size_t max_length) const {
  std::vector<int64_t> ids;
  ids.push_back(cls_id_);

  for (const std::string& word : SplitWords(text)) {
    for (const std::string& piece : WordPieceSplit(word)) {
      // Leave room for the trailing [SEP] — stop and close out early
      // rather than exceed the model's max sequence length.
      if (ids.size() + 1 >= max_length) {
        ids.push_back(sep_id_);
        return ids;
      }
      auto it = vocab_.find(piece);
      ids.push_back(it != vocab_.end() ? it->second : unk_id_);
    }
  }

  ids.push_back(sep_id_);
  return ids;
}

}  // namespace common
