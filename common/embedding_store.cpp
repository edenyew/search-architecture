#include "embedding_store.h"

#include <fstream>
#include <stdexcept>

namespace common {

void EmbeddingStore::AddEmbedding(const std::vector<float>& embedding) {
  if (embeddings_.empty()) {
    dim_ = static_cast<int>(embedding.size());
  } else if (static_cast<int>(embedding.size()) != dim_) {
    throw std::runtime_error("embedding dimension mismatch");
  }
  embeddings_.push_back(embedding);
}

int EmbeddingStore::NumDocs() const {
  return static_cast<int>(embeddings_.size());
}

int EmbeddingStore::Dim() const {
  return dim_;
}

const std::vector<float>& EmbeddingStore::Get(int doc_id) const {
  return embeddings_[doc_id];
}

void EmbeddingStore::Save(const std::string& path) const {
  std::ofstream out(path, std::ios::binary);
  if (!out.is_open()) {
    throw std::runtime_error("failed to open embeddings file for writing: " + path);
  }

  int num_docs = NumDocs();
  out.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));
  out.write(reinterpret_cast<const char*>(&dim_), sizeof(dim_));

  for (const std::vector<float>& embedding : embeddings_) {
    out.write(reinterpret_cast<const char*>(embedding.data()), dim_ * sizeof(float));
  }
}

EmbeddingStore EmbeddingStore::Load(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    throw std::runtime_error("failed to open embeddings file for reading: " + path);
  }

  EmbeddingStore store;
  int num_docs = 0;
  in.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));
  in.read(reinterpret_cast<char*>(&store.dim_), sizeof(store.dim_));

  store.embeddings_.resize(num_docs);
  for (int i = 0; i < num_docs; ++i) {
    store.embeddings_[i].resize(store.dim_);
    in.read(reinterpret_cast<char*>(store.embeddings_[i].data()), store.dim_ * sizeof(float));
  }

  return store;
}

}  // namespace common
