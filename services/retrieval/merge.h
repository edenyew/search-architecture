#pragma once

#include <vector>

#include "ann_index.h"
#include "index.h"

namespace retrieval {

// Combines a BM25 ranking and a vector-similarity ranking into one
// fused ranking using Reciprocal Rank Fusion (RRF): a document's
// fused score is the sum of 1/(k + rank) across every list it
// appears in, using each list's RANK POSITION rather than its raw
// score — BM25 scores and cosine similarities live on completely
// different scales, so rank position is what makes them combinable
// at all. Returns the top_k highest fused-score documents, best
// first.
std::vector<common::ScoredDoc> MergeRRF(const std::vector<common::ScoredDoc>& bm25_results,
                                         const std::vector<AnnResult>& vector_results, int top_k);

}  // namespace retrieval
