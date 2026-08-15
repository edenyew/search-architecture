#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace common {

// A WordPiece tokenizer matching BERT-family models (e.g.
// all-MiniLM-L6-v2): greedy longest-match against a fixed
// vocabulary, wrapping the result with [CLS]/[SEP]. This is a
// separate algorithm from Tokenize() in tokenizer.h — BM25 uses
// whole lowercase words, embedding models use these sub-word pieces.
class WordPieceTokenizer {
 public:
  explicit WordPieceTokenizer(const std::string& vocab_path);

  // Tokenizes text into vocabulary ids, with [CLS] prepended and
  // [SEP] appended, matching what the embedding model expects.
  // Truncated to max_length total tokens (including [CLS]/[SEP]) —
  // all-MiniLM-L6-v2's max_position_embeddings is 512.
  std::vector<int64_t> Encode(const std::string& text, size_t max_length = 512) const;

 private:
  // Basic pre-tokenization: lowercase, split on whitespace, split
  // punctuation into its own words.
  std::vector<std::string> SplitWords(const std::string& text) const;

  // Greedy longest-match: decomposes one word into vocabulary
  // pieces, prefixing continuation pieces with "##".
  std::vector<std::string> WordPieceSplit(const std::string& word) const;

  std::unordered_map<std::string, int64_t> vocab_;
  int64_t cls_id_;
  int64_t sep_id_;
  int64_t unk_id_;
};

}  // namespace common
