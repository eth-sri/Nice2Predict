/*
   Copyright 2014 Software Reliability Lab, ETH Zurich

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
 */

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "jsonrpc/server_connectors_httpserver.h"

#include "json_server.h"

DEFINE_int32(port, 5745, "JSON-RPC Server port");
DEFINE_int32(num_threads, 8, "Number of serving threads");

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  LOG(INFO) << "Starting server on port " << FLAGS_port;
  jsonrpc::HttpServer http(FLAGS_port, "", "", FLAGS_num_threads);
  Nice2Server server(&http);
  server.Listen();

  return 0;
}
