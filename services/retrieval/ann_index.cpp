#include "ann_index.h"

#include <algorithm>

#include "embedding_store.h"

namespace retrieval {

AnnIndex::AnnIndex(const std::string& embeddings_path) {
  common::EmbeddingStore store = common::EmbeddingStore::Load(embeddings_path);

  space_ = std::make_unique<hnswlib::InnerProductSpace>(store.Dim());
  index_ = std::make_unique<hnswlib::HierarchicalNSW<float>>(space_.get(), store.NumDocs());

  for (int doc_id = 0; doc_id < store.NumDocs(); ++doc_id) {
    index_->addPoint(store.Get(doc_id).data(), doc_id);
  }
}

std::vector<AnnResult> AnnIndex::Search(const std::vector<float>& query_embedding, int top_k) const {
  auto result_queue = index_->searchKnn(query_embedding.data(), top_k);

  std::vector<AnnResult> results;
  while (!result_queue.empty()) {
    auto [distance, doc_id] = result_queue.top();
    // InnerProductSpace's "distance" is 1 - inner_product; since our
    // embeddings are L2-normalized, inner_product IS cosine
    // similarity, so similarity = 1 - distance.
    results.push_back(AnnResult{static_cast<int>(doc_id), 1.0f - distance});
    result_queue.pop();
  }

  // The priority queue pops worst-first (max-heap by distance);
  // reverse to get best-match-first.
  std::reverse(results.begin(), results.end());
  return results;
}

}  // namespace retrieval
