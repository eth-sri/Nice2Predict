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


#include "json/json.h"
#include "jsonrpc/server.h"
#include "jsonrpc/server_connectors_httpserver.h"

#include "base/stringprintf.h"
#include "n2p/server/nice2service_internal.h"
#include "n2p/server/server_log.h"

#include "json_adapter.h"
#include "json_server.h"


DEFINE_string(model, "model", "Input model files");
DEFINE_string(model_version, "", "Version of the current model");
DEFINE_string(logfile_prefix, "", "File where to log all requests and responses");


namespace {
void DropTrainingNewLine(std::string* s) {
  if (s->empty()) return;
  if ((*s)[s->size() - 1] == '\n') s->erase(s->begin() + s->size() - 1, s->end());
}
}

class Nice2ServerInternal : public jsonrpc::AbstractServer<Nice2ServerInternal> {
public:
  explicit Nice2ServerInternal(jsonrpc::HttpServer* server) :
      jsonrpc::AbstractServer<Nice2ServerInternal>(*server),
      impl_(FLAGS_model, FLAGS_logfile_prefix) {
    bindAndAddMethod(
        jsonrpc::Procedure("infer", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_ARRAY,
            // Parameters:
            "query", jsonrpc::JSON_ARRAY,
            "assign", jsonrpc::JSON_ARRAY,
            NULL),
        &Nice2ServerInternal::infer);
    bindAndAddMethod(
        jsonrpc::Procedure("nbest", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_ARRAY,
            // Parameters:
            "n", jsonrpc::JSON_INTEGER,
            "query", jsonrpc::JSON_ARRAY,
            "assign", jsonrpc::JSON_ARRAY,
            NULL),
        &Nice2ServerInternal::nbest);

    bindAndAddMethod(
        jsonrpc::Procedure("showgraph", jsonrpc::PARAMS_BY_NAME, jsonrpc::JSON_OBJECT,
            // Parameters:
            "query", jsonrpc::JSON_ARRAY,
            "assign", jsonrpc::JSON_ARRAY,
            NULL),
        &Nice2ServerInternal::showgraph);

    if (!FLAGS_logfile_prefix.empty()) {
      logging_.reset(new Nice2ServerLog(FLAGS_logfile_prefix));
    }
  }

  void verifyVersion(const Json::Value& request){
    VLOG(3) << "Current version: " << FLAGS_model_version << ". Request version: " << request["version"];
    if (FLAGS_model_version.empty()){
      return;
    }

    if (!request.isMember("version") ||
        strcmp(request["version"].asCString(), FLAGS_model_version.c_str()) != 0){
      std::ostringstream stringStream;
      stringStream << "The version of client '" << request["version"].asString() <<
          "' does not match the server version '" << FLAGS_model_version << "'. " <<
          "Please update the client to the latest version by running 'npm update -g unuglify-js'.";
      throw jsonrpc::JsonRpcException(-31001, stringStream.str());
    }
  }


  void MaybeLogQuery(const char* method, const Json::Value& request, const Json::Value& response) {
    if (logging_.get() == NULL) {
      return;
    }

    Json::FastWriter writer;
    std::string rs1 = writer.write(request);
    std::string rs2 = writer.write(response);
    DropTrainingNewLine(&rs1);
    DropTrainingNewLine(&rs2);
    logging_->LogRecord(StringPrintf(
        "\"method\":\"%s\", "
        "\"request\":%s, "
        "\"reply\":%s",
        method, rs1.c_str(), rs2.c_str()));
  }


  void infer(const Json::Value& request, Json::Value& response) {
    VLOG(3) << request.toStyledString();
    verifyVersion(request);
    response = adapter_.InferResponseToJson(impl_.Infer(adapter_.JsonToQuery(request)));
    MaybeLogQuery("infer", request, response);
  }

  void nbest(const Json::Value& request, Json::Value& response) {
    VLOG(3) << request.toStyledString();
    verifyVersion(request);
    response = adapter_.NBestResponseToJson(impl_.NBest(adapter_.JsonToNBestQuery(request)));
    MaybeLogQuery("nbest", request, response);
  }

  void showgraph(const Json::Value& request, Json::Value& response) {
    VLOG(3) << request.toStyledString();
    verifyVersion(request);
    response = adapter_.ShowGraphResponseToJson(impl_.ShowGraph(adapter_.JsonToShowGraphQuery(request)));
    MaybeLogQuery("showgraph", request, response);
  }

private:
  std::unique_ptr<Nice2ServerLog> logging_;
  Nice2ServiceInternal impl_;
  JsonAdapter adapter_;
};

Nice2Server::Nice2Server(jsonrpc::HttpServer* server)
  : internal_(new Nice2ServerInternal(server)) {
}

Nice2Server::~Nice2Server() {
  delete internal_;
}

void Nice2Server::Listen() {
  internal_->StartListening();
  LOG(INFO) << "Nice2Server started.";
  for (;;) {
    sleep(1);
  }
  internal_->StopListening();
}
