#include "merge.h"

#include <algorithm>
#include <unordered_map>

namespace retrieval {

namespace {
// Standard RRF damping constant from the original paper — large
// enough that rank differences among top results don't dominate.
constexpr double kRrfK = 60.0;
}  // namespace

std::vector<common::ScoredDoc> MergeRRF(const std::vector<common::ScoredDoc>& bm25_results,
                                         const std::vector<AnnResult>& vector_results,
                                         int top_k) {
  std::unordered_map<int, double> fused_scores;

  for (size_t rank = 0; rank < bm25_results.size(); ++rank) {
    int doc_id = bm25_results[rank].doc_id;
    fused_scores[doc_id] += 1.0 / (kRrfK + static_cast<double>(rank + 1));
  }

  for (size_t rank = 0; rank < vector_results.size(); ++rank) {
    int doc_id = vector_results[rank].doc_id;
    fused_scores[doc_id] += 1.0 / (kRrfK + static_cast<double>(rank + 1));
  }

  std::vector<common::ScoredDoc> results;
  results.reserve(fused_scores.size());
  for (const auto& [doc_id, score] : fused_scores) {
    results.push_back(common::ScoredDoc{doc_id, score});
  }

  std::sort(results.begin(), results.end(),
            [](const common::ScoredDoc& a, const common::ScoredDoc& b) {
              return a.score > b.score;
            });

  if (static_cast<int>(results.size()) > top_k) {
    results.resize(top_k);
  }

  return results;
}

}  // namespace retrieval
