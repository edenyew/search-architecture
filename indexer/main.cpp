#include <cstdio>
#include <string>
#include <vector>

#include "corpus.h"
#include "index.h"
#include "tokenizer.h"

int main() {
  const std::string corpus_path = "data/corpus.jsonl";
  const std::string index_path = "data/bm25.idx";

  std::vector<common::CorpusDoc> docs = common::LoadCorpus(corpus_path);
  std::printf("loaded %zu documents from %s\n", docs.size(), corpus_path.c_str());

  common::InvertedIndex index;
  for (const common::CorpusDoc& doc : docs) {
    std::vector<std::string> tokens = common::Tokenize(doc.text);
    index.AddDocument(doc.id, tokens);
  }

  index.Save(index_path);
  std::printf("wrote index (%d docs, avgdl=%.2f) to %s\n", index.NumDocs(), index.AvgDocLength(),
              index_path.c_str());

  return 0;
}
