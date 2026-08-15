#pragma once

#include <hnswlib/hnswlib.h>

#include <memory>
#include <string>
#include <vector>

namespace retrieval {

// One approximate-nearest-neighbor result: an internal doc_id (same
// numbering as InvertedIndex) and its cosine similarity to the query.
struct AnnResult {
  int doc_id;
  float similarity;
};

// Vector similarity search over document embeddings, built in
// memory at startup from data/embeddings.bin. Retrieval-only — no
// other binary needs an ANN search structure, so this lives here
// rather than in common/.
class AnnIndex {
 public:
  explicit AnnIndex(const std::string& embeddings_path);

  // Returns the top_k documents whose embeddings are most similar
  // to query_embedding, best match first.
  std::vector<AnnResult> Search(const std::vector<float>& query_embedding, int top_k) const;

 private:
  std::unique_ptr<hnswlib::InnerProductSpace> space_;
  std::unique_ptr<hnswlib::HierarchicalNSW<float>> index_;
};

}  // namespace retrieval
