#include "embedder.h"

#include <array>
#include <cmath>

namespace common {

Embedder::Embedder(const std::string& model_path, const std::string& vocab_path)
    : tokenizer_(vocab_path), env_(ORT_LOGGING_LEVEL_WARNING, "embedder"), session_(env_, model_path.c_str(), Ort::SessionOptions{nullptr}) {}

std::vector<float> Embedder::Embed(const std::string& text) {
  std::vector<int64_t> input_ids = tokenizer_.Encode(text);
  std::vector<int64_t> attention_mask(input_ids.size(), 1);

  std::array<int64_t, 2> shape{1, static_cast<int64_t>(input_ids.size())};

  Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

  std::array<Ort::Value, 2> inputs{
      Ort::Value::CreateTensor<int64_t>(memory_info, input_ids.data(), input_ids.size(), shape.data(), shape.size()),
      Ort::Value::CreateTensor<int64_t>(memory_info, attention_mask.data(), attention_mask.size(), shape.data(), shape.size()),
  };

  const char* input_names[] = {"input_ids", "attention_mask"};
  const char* output_names[] = {"last_hidden_state"};

  std::vector<Ort::Value> outputs = session_.Run(Ort::RunOptions{nullptr}, input_names, inputs.data(), inputs.size(), output_names, 1);

  const float* hidden = outputs[0].GetTensorData<float>();
  std::vector<int64_t> out_shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape();
  int64_t seq_len = out_shape[1];
  int64_t hidden_size = out_shape[2];

  // Mean pooling across the sequence dimension. attention_mask is
  // all 1s here (single document, no padding), so this is a plain
  // average over every token's vector.
  std::vector<float> pooled(hidden_size, 0.0f);
  for (int64_t t = 0; t < seq_len; ++t) {
    for (int64_t d = 0; d < hidden_size; ++d) {
      pooled[d] += hidden[t * hidden_size + d];
    }
  }
  for (float& v : pooled) {
    v /= static_cast<float>(seq_len);
  }

  // L2 normalize, so downstream ANN search can use plain dot
  // product instead of full cosine similarity.
  float norm = 0.0f;
  for (float v : pooled) {
    norm += v * v;
  }
  norm = std::sqrt(norm);
  if (norm > 0.0f) {
    for (float& v : pooled) {
      v /= norm;
    }
  }

  return pooled;
}

}  // namespace common
