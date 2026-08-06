#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace common {

// One entry in a term's postings list: which document contains the
// term, and how many times it appears there.
struct PostingEntry {
  int doc_id;
  int term_freq;
};

// One scored search result: an internal doc_id and its BM25 score,
// highest score first.
struct ScoredDoc {
  int doc_id;
  double score;
};

// A hand-rolled inverted index: term -> list of (doc_id, term_freq).
// Documents are added one at a time; each gets an internal integer
// doc_id (its insertion order) separate from the corpus's own string
// id, since postings lists of ints are smaller and cheaper to
// compare than postings lists of strings.
class InvertedIndex {
 public:
  // Registers one document's tokens under a new internal doc_id.
  void AddDocument(const std::string& external_id, const std::vector<std::string>& tokens);

  // Total number of documents in the index — the N in BM25's IDF term.
  int NumDocs() const;

  // Mean document length across the whole corpus — the avgdl in
  // BM25's length-normalization term.
  double AvgDocLength() const;

  // Translates an internal doc_id back to the corpus's original id.
  const std::string& ExternalId(int doc_id) const;

  // Scores every document containing at least one query token using
  // BM25, and returns the top_k highest-scoring documents, best first.
  std::vector<ScoredDoc> Search(const std::vector<std::string>& query_tokens, int top_k) const;

  // Writes the full index — postings, doc ids, doc lengths — to a
  // binary file at path, so it can be loaded later without rebuilding.
  void Save(const std::string& path) const;

  // Reads an index previously written by Save() back into memory.
  static InvertedIndex Load(const std::string& path);

 private:
  std::vector<std::string> doc_ids_;  // internal doc_id -> external id
  std::vector<int> doc_lengths_;      // internal doc_id -> token count
  std::unordered_map<std::string, std::vector<PostingEntry>> postings_;
};

}  // namespace common
