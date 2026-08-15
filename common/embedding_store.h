#pragma once

#include <string>
#include <vector>

namespace common {

// A flat store of fixed-size embeddings, indexed by the same
// internal doc_id InvertedIndex uses — document i's embedding lives
// at index i, as long as documents are added to both in the same
// order.
class EmbeddingStore {
 public:
  void AddEmbedding(const std::vector<float>& embedding);

  int NumDocs() const;
  int Dim() const;
  const std::vector<float>& Get(int doc_id) const;

  void Save(const std::string& path) const;
  static EmbeddingStore Load(const std::string& path);

 private:
  int dim_ = 0;
  std::vector<std::vector<float>> embeddings_;
};

}  // namespace common
