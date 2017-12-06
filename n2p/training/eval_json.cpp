/*
   Copyright 2017 Software Reliability Lab, ETH Zurich

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

#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "json/json.h"

#include "n2p/json_server/json_adapter.h"

#include "eval_internal.h"

using nice2protos::Query;

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  return EvalMain<std::string>([](const std::string &line) {
    JsonAdapter adapter;
    Json::Reader json_reader;
    Json::Value v;
    if (!json_reader.parse(line, v, false)) {
      LOG(ERROR) << "Could not parse input: " << json_reader.getFormattedErrorMessages();
    }

    return adapter.JsonToQuery(v);
  });
}
