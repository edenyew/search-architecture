#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "common.pb.h"
#include "corpus.h"
#include "index.h"
#include "retrieval.grpc.pb.h"
#include "tokenizer.h"

namespace {

class RetrievalServiceImpl final : public search::retrieval::Retrieval::Service {
 public:
  RetrievalServiceImpl(common::InvertedIndex index,
                       std::unordered_map<std::string, std::string> id_to_text)
      : index_(std::move(index)), id_to_text_(std::move(id_to_text)) {}

  grpc::Status Search(grpc::ServerContext* /*context*/, const search::common::SearchQuery* request,
                      search::retrieval::SearchResponse* response) override {
    std::vector<std::string> query_tokens = common::Tokenize(request->query_text());
    std::vector<common::ScoredDoc> scored = index_.Search(query_tokens, request->top_k());

    for (const common::ScoredDoc& sd : scored) {
      const std::string& id = index_.ExternalId(sd.doc_id);

      search::common::Document* doc = response->add_documents();
      doc->set_id(id);
      doc->set_score(sd.score);

      auto it = id_to_text_.find(id);
      if (it != id_to_text_.end()) {
        doc->set_text(it->second);
      }
    }

    return grpc::Status::OK;
  }

 private:
  common::InvertedIndex index_;
  std::unordered_map<std::string, std::string> id_to_text_;
};

}  // namespace

int main() {
  const std::string index_path = "data/bm25.idx";
  const std::string corpus_path = "data/corpus.jsonl";
  const std::string server_address = "0.0.0.0:50051";

  std::printf("loading index from %s...\n", index_path.c_str());
  common::InvertedIndex index = common::InvertedIndex::Load(index_path);

  std::printf("loading corpus text from %s...\n", corpus_path.c_str());
  std::vector<common::CorpusDoc> docs = common::LoadCorpus(corpus_path);
  std::unordered_map<std::string, std::string> id_to_text;
  for (common::CorpusDoc& doc : docs) {
    id_to_text.emplace(std::move(doc.id), std::move(doc.text));
  }

  RetrievalServiceImpl service(std::move(index), std::move(id_to_text));

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
