#include <iostream>

#include <grpc++/grpc++.h>

#include "service.grpc.pb.h"

#include "stringprintf.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "graph_inference.h"
#include "server_log.h"
#include "inference.h"


DEFINE_string(model, "model", "Input model files");
DEFINE_string(model_version, "", "Version of the current model");

DEFINE_string(logfile_prefix, "", "File where to log all requests and responses");

DEFINE_int32(port, 5745, "JSON-RPC Server port");
DEFINE_int32(num_threads, 8, "Number of serving threads");

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using nice2protos::InferQuery;
using nice2protos::NBestQuery;
using nice2protos::ShowGraphQuery;
using nice2protos::InferResponse;
using nice2protos::NBestResponse;
using nice2protos::ShowGraphResponse;
using nice2protos::Nice2Service;

// Logic and data behind the server's behavior.
class Nice2ServiceImpl final : public Nice2Service::Service {

 public:
  Nice2ServiceImpl() {
    inference_.LoadModel(FLAGS_model);
    if (!FLAGS_logfile_prefix.empty()) {
      logging_.reset(new Nice2ServerLog(FLAGS_logfile_prefix));
    }
  }

 private:

  Status Infer(ServerContext* context, const InferQuery* request,
               InferResponse* reply) override {

    std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
    query->FromFeatureProto(request->features());
    std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
    assignment->FromPropertyProto(request->assignments());
    inference_.MapInference(query.get(), assignment.get());
    assignment->FillInferResponse(reply);
    return Status::OK;
  }

  Status NBest(ServerContext* context, const NBestQuery* request,
               NBestResponse* reply) override {
    std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
    query->FromFeatureProto(request->query().features());
    std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
    assignment->FromPropertyProto(request->query().assignments());
    if (request->should_infer()) {
      inference_.MapInference(query.get(), assignment.get());
    }
    assignment->GetNBestCandidates(&inference_, request->n(), reply);
    return Status::OK;
  }

  Status ShowGraph(ServerContext* context, const ShowGraphQuery* request,
                   ShowGraphResponse* reply) override {
    std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
    query->FromFeatureProto(request->query().features());
    std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
    assignment->FromPropertyProto(request->query().assignments());
    if (request->should_infer()) {
      inference_.MapInference(query.get(), assignment.get());
    }
    inference_.FillGraphProto(query.get(), assignment.get(), reply);
    return Status::OK;
  }

  GraphInference inference_;
  std::unique_ptr<Nice2ServerLog> logging_;
};

void RunServer() {
  std::string server_address(StringPrintf("0.0.0.0:%d", FLAGS_port));
  Nice2ServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the serer for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  RunServer();

  return 0;
}
