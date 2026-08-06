#include <grpcpp/grpcpp.h>

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "common.pb.h"
#include "controller.grpc.pb.h"
#include "crow.h"

int main()
{
  const std::string controller_address = "localhost:50050";

  std::shared_ptr<grpc::Channel> channel =
      grpc::CreateChannel(controller_address, grpc::InsecureChannelCredentials());
  std::unique_ptr<search::controller::Controller::Stub> stub =
      search::controller::Controller::NewStub(channel);

  crow::SimpleApp app;

  CROW_ROUTE(app, "/search")
  ([&stub](const crow::request &req)
   {
    const char* q = req.url_params.get("q");
    if (q == nullptr || std::string(q).empty()) {
      return crow::response(400, "missing required query parameter: q");
    }

    int top_k = 10;
    if (const char* top_k_param = req.url_params.get("top_k")) {
      top_k = std::atoi(top_k_param);
    }

    search::common::SearchQuery request;
    request.set_query_text(q);
    request.set_top_k(top_k);

    search::controller::SearchResponse controller_response;
    grpc::ClientContext context;
    grpc::Status status = stub->Search(&context, request, &controller_response);

    if (!status.ok()) {
      return crow::response(502, "controller call failed: " + status.error_message());
    }

    crow::json::wvalue result;
    std::vector<crow::json::wvalue> docs;
    for (const search::common::Document& doc : controller_response.documents()) {
      crow::json::wvalue d;
      d["id"] = doc.id();
      d["text"] = doc.text();
      d["score"] = doc.score();
      docs.push_back(std::move(d));
    }
    result["documents"] = std::move(docs);

    return crow::response{result}; });

  std::printf("gateway listening on 0.0.0.0:8080 (controller at %s)\n", controller_address.c_str());
  app.port(8080).multithreaded().run();

  return 0;
}
