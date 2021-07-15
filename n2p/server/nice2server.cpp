#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "grpcpp/grpcpp.h"

#include "base/stringprintf.h"
#include "n2p/inference/graph_inference.h"
#include "n2p/protos/service.grpc.pb.h"

#include "nice2service_internal.h"

DEFINE_int32(port, 5745, "JSON-RPC Server port");
DEFINE_int32(num_threads, 8, "Number of serving threads");

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using nice2protos::Query;
using nice2protos::NBestQuery;
using nice2protos::ShowGraphQuery;
using nice2protos::InferResponse;
using nice2protos::NBestResponse;
using nice2protos::ShowGraphResponse;
using nice2protos::Nice2Service;

DEFINE_string(model, "model", "Input model files");
DEFINE_string(logfile_prefix, "", "File where to log all requests and responses");

// Logic and data behind the server's behavior.
class Nice2ServiceImpl final : public Nice2Service::Service {
 public:
  Nice2ServiceImpl() : impl(FLAGS_model, FLAGS_logfile_prefix) {}

 private:

  Status Infer(ServerContext* context, const Query* request, InferResponse* reply) override {
    *reply = impl.Infer(*request);
    return Status::OK;
  }

  Status NBest(ServerContext* context, const NBestQuery* request, NBestResponse* reply) override {
    *reply = impl.NBest(*request);
    return Status::OK;
  }

  Status ShowGraph(ServerContext* context, const ShowGraphQuery* request, ShowGraphResponse* reply) override {
    *reply = impl.ShowGraph(*request);
    return Status::OK;
  }

  Nice2ServiceInternal impl;
};

void RunServer() {
  std::string server_address(StringPrintf("0.0.0.0:%d", FLAGS_port));
  Nice2ServiceImpl service{};

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
