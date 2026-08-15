#pragma once

#include <onnxruntime_cxx_api.h>

#include <string>
#include <vector>

#include "wordpiece_tokenizer.h"

namespace common {

// Computes 384-dim sentence embeddings using a pretrained ONNX
// model (all-MiniLM-L6-v2): tokenize -> run the model -> mean-pool
// the per-token outputs -> L2-normalize. Model inference happens
// entirely offline (indexer) or per-query (retrieval) — never
// trained here, only run.
class Embedder {
 public:
  Embedder(const std::string& model_path, const std::string& vocab_path);

  // Computes a normalized 384-dim embedding for the given text.
  std::vector<float> Embed(const std::string& text);

 private:
  WordPieceTokenizer tokenizer_;
  Ort::Env env_;
  Ort::Session session_;
};

}  // namespace common
