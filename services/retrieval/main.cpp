#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <string>
#include <unordered_map>
#include <vector>

#include "ann_index.h"
#include "common.pb.h"
#include "corpus.h"
#include "embedder.h"
#include "index.h"
#include "merge.h"
#include "retrieval.grpc.pb.h"
#include "tokenizer.h"
#include "trace.h"

namespace {

class RetrievalServiceImpl final : public search::retrieval::Retrieval::Service {
 public:
  RetrievalServiceImpl(common::InvertedIndex index, std::unordered_map<std::string, std::string> id_to_text, common::Embedder embedder, retrieval::AnnIndex ann_index)
      : index_(std::move(index)), id_to_text_(std::move(id_to_text)), embedder_(std::move(embedder)), ann_index_(std::move(ann_index)) {}

  grpc::Status Search(grpc::ServerContext* context, const search::common::SearchQuery* request, search::retrieval::SearchResponse* response) override {
    const std::string trace_id = common::ExtractTraceId(*context);
    const auto total_start = std::chrono::steady_clock::now();

    const std::string query_text = request->query_text();
    const int top_k = request->top_k();

    // Run BM25 scoring and vector similarity search in parallel —
    // they touch entirely separate data (index_ vs embedder_ +
    // ann_index_), so there's no shared state between the two tasks.
    // Each task times itself independently; since they run
    // concurrently, "total" below should land close to whichever of
    // the two is slower, not their sum — that's the actual evidence
    // the parallelism is real, visible directly in the logs.
    std::future<std::vector<common::ScoredDoc>> bm25_future = std::async(std::launch::async, [this, &query_text, top_k, &trace_id]() {
      const auto start = std::chrono::steady_clock::now();
      std::vector<std::string> tokens = common::Tokenize(query_text);
      std::vector<common::ScoredDoc> results = index_.Search(tokens, top_k);
      common::LogStage(trace_id, "retrieval", "bm25_search", std::chrono::steady_clock::now() - start);
      return results;
    });

    std::future<std::vector<retrieval::AnnResult>> vector_future = std::async(std::launch::async, [this, &query_text, top_k, &trace_id]() {
      const auto start = std::chrono::steady_clock::now();
      std::vector<float> query_embedding = embedder_.Embed(query_text);
      std::vector<retrieval::AnnResult> results = ann_index_.Search(query_embedding, top_k);
      common::LogStage(trace_id, "retrieval", "vector_search", std::chrono::steady_clock::now() - start);
      return results;
    });

    std::vector<common::ScoredDoc> bm25_results = bm25_future.get();
    std::vector<retrieval::AnnResult> vector_results = vector_future.get();

    const auto merge_start = std::chrono::steady_clock::now();
    std::vector<common::ScoredDoc> fused = retrieval::MergeRRF(bm25_results, vector_results, top_k);
    common::LogStage(trace_id, "retrieval", "merge", std::chrono::steady_clock::now() - merge_start);

    for (const common::ScoredDoc& sd : fused) {
      const std::string& id = index_.ExternalId(sd.doc_id);

      search::common::Document* doc = response->add_documents();
      doc->set_id(id);
      doc->set_score(sd.score);

      auto it = id_to_text_.find(id);
      if (it != id_to_text_.end()) {
        doc->set_text(it->second);
      }
    }

    common::LogStage(trace_id, "retrieval", "total", std::chrono::steady_clock::now() - total_start);
    return grpc::Status::OK;
  }

 private:
  common::InvertedIndex index_;
  std::unordered_map<std::string, std::string> id_to_text_;
  common::Embedder embedder_;
  retrieval::AnnIndex ann_index_;
};

}  // namespace

int main() {
  const std::string index_path = "data/bm25.idx";
  const std::string corpus_path = "data/corpus.jsonl";
  const std::string embeddings_path = "data/embeddings.bin";
  const std::string model_path = "models/all-MiniLM-L6-v2.onnx";
  const std::string vocab_path = "models/vocab.txt";
  const std::string server_address = "0.0.0.0:50051";

  std::printf("loading index from %s...\n", index_path.c_str());
  common::InvertedIndex index = common::InvertedIndex::Load(index_path);

  std::printf("loading corpus text from %s...\n", corpus_path.c_str());
  std::vector<common::CorpusDoc> docs = common::LoadCorpus(corpus_path);
  std::unordered_map<std::string, std::string> id_to_text;
  for (common::CorpusDoc& doc : docs) {
    id_to_text.emplace(std::move(doc.id), std::move(doc.text));
  }

  std::printf("loading embedding model from %s...\n", model_path.c_str());
  common::Embedder embedder(model_path, vocab_path);

  std::printf("loading vector index from %s...\n", embeddings_path.c_str());
  retrieval::AnnIndex ann_index(embeddings_path);

  RetrievalServiceImpl service(std::move(index), std::move(id_to_text), std::move(embedder), std::move(ann_index));

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::printf("retrieval service listening on %s\n", server_address.c_str());
  server->Wait();

  return 0;
}
