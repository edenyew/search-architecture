#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>

#include <cstdio>
#include <memory>
#include <string>

#include "common.pb.h"
#include "controller.grpc.pb.h"
#include "retrieval.grpc.pb.h"

namespace {

class ControllerServiceImpl final : public search::controller::Controller::Service {
 public:
  explicit ControllerServiceImpl(const std::shared_ptr<grpc::Channel>& retrieval_channel) : retrieval_stub_(search::retrieval::Retrieval::NewStub(retrieval_channel)) {}

  grpc::Status Search(grpc::ServerContext*, const search::common::SearchQuery* request, search::controller::SearchResponse* response) override {
    std::string route = Classify(*request);
    return Orchestrate(*request, route, response);
  }

 private:
  // Phase 1 stub: every query takes the same route. Real routing
  // logic lands here in a later phase, once there's something to
  // route between.
  std::string Classify(const search::common::SearchQuery& /*query*/) {
    return "default";
  }

  // Phase 1: a single passthrough call to Retrieval. Later phases
  // fan out to Retrieval + Cache + Sorter in parallel here instead.
  grpc::Status Orchestrate(const search::common::SearchQuery& query, const std::string&, search::controller::SearchResponse* response) {
    grpc::ClientContext context;
    search::retrieval::SearchResponse retrieval_response;

    grpc::Status status = retrieval_stub_->Search(&context, query, &retrieval_response);
    if (!status.ok()) {
      return status;
    }

    response->mutable_documents()->CopyFrom(retrieval_response.documents());
    return grpc::Status::OK;
  }

  std::unique_ptr<search::retrieval::Retrieval::Stub> retrieval_stub_;
};

}  // namespace

int main() {
  const std::string retrieval_address = "localhost:50051";
  const std::string server_address = "0.0.0.0:50050";

  std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel(retrieval_address, grpc::InsecureChannelCredentials());
  ControllerServiceImpl service(channel);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();

  grpc::ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::printf("controller service listening on %s (retrieval at %s)\n", server_address.c_str(), retrieval_address.c_str());
  server->Wait();

  return 0;
}
