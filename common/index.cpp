#include "index.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace common {

namespace {

// Every value below is written/read as raw bytes, with no text
// formatting. Strings are prefixed with their length (as an int) so
// the reader knows exactly how many bytes to consume for each one.

void WriteInt(std::ofstream& out, int value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteDouble(std::ofstream& out, double value) {
  out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteString(std::ofstream& out, const std::string& value) {
  WriteInt(out, static_cast<int>(value.size()));
  out.write(value.data(), static_cast<std::streamsize>(value.size()));
}

int ReadInt(std::ifstream& in) {
  int value = 0;
  in.read(reinterpret_cast<char*>(&value), sizeof(value));
  return value;
}

double ReadDouble(std::ifstream& in) {
  double value = 0.0;
  in.read(reinterpret_cast<char*>(&value), sizeof(value));
  return value;
}

std::string ReadString(std::ifstream& in) {
  int len = ReadInt(in);
  std::string value(len, '\0');
  in.read(value.data(), len);
  return value;
}

}  // namespace

void InvertedIndex::AddDocument(const std::string& external_id, const std::vector<std::string>& tokens) {
  int doc_id = static_cast<int>(doc_ids_.size());
  doc_ids_.push_back(external_id);
  doc_lengths_.push_back(static_cast<int>(tokens.size()));

  std::unordered_map<std::string, int> term_freq;
  for (const std::string& term : tokens) {
    term_freq[term]++;
  }

  for (const auto& [term, freq] : term_freq) {
    postings_[term].push_back(PostingEntry{doc_id, freq});
  }
}

int InvertedIndex::NumDocs() const {
  return static_cast<int>(doc_ids_.size());
}

double InvertedIndex::AvgDocLength() const {
  if (doc_lengths_.empty()) return 0.0;

  long long total = 0;
  for (int len : doc_lengths_) {
    total += len;
  }
  return static_cast<double>(total) / static_cast<double>(doc_lengths_.size());
}

const std::string& InvertedIndex::ExternalId(int doc_id) const {
  return doc_ids_[doc_id];
}

std::vector<ScoredDoc> InvertedIndex::Search(const std::vector<std::string>& query_tokens, int top_k) const {
  constexpr double k1 = 1.5;
  constexpr double b = 0.75;

  const double avgdl = AvgDocLength();
  const double n = static_cast<double>(NumDocs());

  std::unordered_map<int, double> scores;

  for (const std::string& term : query_tokens) {
    auto it = postings_.find(term);
    if (it == postings_.end()) continue;

    const std::vector<PostingEntry>& entries = it->second;
    const double df = static_cast<double>(entries.size());
    const double idf = std::log((n - df + 0.5) / (df + 0.5) + 1.0);

    for (const PostingEntry& entry : entries) {
      const double tf = static_cast<double>(entry.term_freq);
      const double doc_len = static_cast<double>(doc_lengths_[entry.doc_id]);
      const double denom = tf + k1 * (1.0 - b + b * doc_len / avgdl);
      scores[entry.doc_id] += idf * (tf * (k1 + 1.0)) / denom;
    }
  }

  std::vector<ScoredDoc> results;
  results.reserve(scores.size());
  for (const auto& [doc_id, score] : scores) {
    results.push_back(ScoredDoc{doc_id, score});
  }

  std::sort(results.begin(), results.end(), [](const ScoredDoc& a, const ScoredDoc& b) { return a.score > b.score; });

  if (static_cast<int>(results.size()) > top_k) {
    results.resize(top_k);
  }

  return results;
}

void InvertedIndex::Save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    throw std::runtime_error("failed to open index file for writing: " + path);
  }

  WriteInt(out, NumDocs());
  WriteDouble(out, AvgDocLength());

  for (int i = 0; i < NumDocs(); ++i) {
    WriteString(out, doc_ids_[i]);
    WriteInt(out, doc_lengths_[i]);
  }

  WriteInt(out, static_cast<int>(postings_.size()));
  for (const auto& [term, entries] : postings_) {
    WriteString(out, term);
    WriteInt(out, static_cast<int>(entries.size()));
    for (const PostingEntry& entry : entries) {
      WriteInt(out, entry.doc_id);
      WriteInt(out, entry.term_freq);
    }
  }
}

InvertedIndex InvertedIndex::Load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open index file for reading: " + path);
  }

  InvertedIndex index;

  const int num_docs = ReadInt(in);
  ReadDouble(in);  // avgdl — recomputed from doc_lengths_, not stored directly

  index.doc_ids_.resize(num_docs);
  index.doc_lengths_.resize(num_docs);
  for (int i = 0; i < num_docs; ++i) {
    index.doc_ids_[i] = ReadString(in);
    index.doc_lengths_[i] = ReadInt(in);
  }

  const int vocab_size = ReadInt(in);
  for (int i = 0; i < vocab_size; ++i) {
    std::string term = ReadString(in);
    const int postings_count = ReadInt(in);

    std::vector<PostingEntry> entries(postings_count);
    for (int j = 0; j < postings_count; ++j) {
      entries[j].doc_id = ReadInt(in);
      entries[j].term_freq = ReadInt(in);
    }
    index.postings_[term] = std::move(entries);
  }

  return index;
}

}  // namespace common
