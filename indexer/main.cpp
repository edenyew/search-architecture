#include <cstdio>
#include <string>
#include <vector>

#include "corpus.h"
#include "embedder.h"
#include "embedding_store.h"
#include "index.h"
#include "tokenizer.h"

int main() {
  const std::string corpus_path = "data/corpus.jsonl";
  const std::string index_path = "data/bm25.idx";
  const std::string embeddings_path = "data/embeddings.bin";
  const std::string model_path = "models/all-MiniLM-L6-v2.onnx";
  const std::string vocab_path = "models/vocab.txt";

  std::vector<common::CorpusDoc> docs = common::LoadCorpus(corpus_path);
  std::printf("loaded %zu documents from %s\n", docs.size(), corpus_path.c_str());

  common::InvertedIndex index;
  common::Embedder embedder(model_path, vocab_path);
  common::EmbeddingStore embeddings;

  for (size_t i = 0; i < docs.size(); ++i) {
    const common::CorpusDoc& doc = docs[i];

    std::vector<std::string> tokens = common::Tokenize(doc.text);
    index.AddDocument(doc.id, tokens);

    std::vector<float> embedding = embedder.Embed(doc.text);
    embeddings.AddEmbedding(embedding);

    if ((i + 1) % 500 == 0 || i + 1 == docs.size()) {
      std::printf("embedded %zu/%zu documents\n", i + 1, docs.size());
    }
  }

  index.Save(index_path);
  std::printf("wrote index (%d docs, avgdl=%.2f) to %s\n", index.NumDocs(), index.AvgDocLength(), index_path.c_str());

  embeddings.Save(embeddings_path);
  std::printf("wrote embeddings (%d docs, dim=%d) to %s\n", embeddings.NumDocs(), embeddings.Dim(), embeddings_path.c_str());

  return 0;
}
