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

#ifndef NICE2PREDICT_PROCESS_DATA_H
#define NICE2PREDICT_PROCESS_DATA_H

#include <string>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "json/json.h"

#include "base/base.h"
#include "base/readerutil.h"
#include "n2p/inference/graph_inference.h"

using nice2protos::Query;

DEFINE_bool(hogwild, true, "Whether to use Hogwild parallel training.");
DEFINE_int32(num_threads, 8, "Number of threads to use.");

template <class InputType>
using Adapter = std::function<Query(const InputType&)>;

typedef std::function<void(const Query& query)> InputProcessor;

template <class InputType>
void ForeachInput(InputRecordReader<InputType>* reader, InputProcessor proc, Adapter<InputType>& adapter) {
  while (!reader->ReachedEnd()) {
    InputType record;
    if (!reader->Read(&record)) {
      continue;
    }
    Query query;
    proc(adapter(record));
  }
}

template <class InputType>
void ParallelForeachInput(RecordInput<InputType>* input, InputProcessor proc, Adapter<InputType> &adapter) {
  if (!FLAGS_hogwild) {
    ForeachInput(input->CreateReader(), proc, adapter);
    return;
  }

  // Do parallel ForEach
  std::unique_ptr<InputRecordReader<InputType>> reader(input->CreateReader());
  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    threads.push_back(std::thread(std::bind(&ForeachInput<InputType>, reader.get(), proc, adapter)));
  }
  for (auto& thread : threads){
    thread.join();
  }
}

#endif //NICE2PREDICT_PROCESS_DATA_H
