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

#ifndef NICE2PREDICT_EVAL_INTERNAL_H
#define NICE2PREDICT_EVAL_INTERNAL_H

#include <string>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "base/base.h"
#include "base/readerutil.h"
#include "base/stringprintf.h"
#include "n2p/inference/graph_inference.h"
#include "n2p/training/process_data.h"

using nice2protos::Query;

DEFINE_string(model, "model", "File prefix for model to evaluate.");
DEFINE_int64(input_records, -1, "Number of input records to use.");

DEFINE_string(input, "testdata", "Input file with objects to be used for evaluation.");
DEFINE_bool(debug_stats, false, "If specifies, only outputs debug stats of a trained model.");
DEFINE_string(output_errors, "", "If set, will output the label errors done by the system.");


void PrintLabelErrorStatsSummary(const SingleLabelErrorStats* stats) {
  if (stats == nullptr) return;
  LOG(INFO) << "Counting classification errors...";
  std::vector<std::pair<int, std::string> > best_stats;
  for (auto it = stats->errors_and_counts.begin(); it != stats->errors_and_counts.end(); ++it) {
    best_stats.push_back(std::pair<int, std::string>(it->second, it->first));
  }
  std::sort(best_stats.begin(), best_stats.end(), std::greater<std::pair<int, std::string> >());

  std::string summary = "Top classification errors done by label (expected -> predicted):";
  for (size_t i = 0; i < 32 && i < best_stats.size(); ++i) {
    StringAppendF(&summary, "\n%8d : %s", best_stats[i].first, best_stats[i].second.c_str());
  }
  LOG(INFO) << summary;
}

SingleLabelErrorStats* CreateLabelErrorStats() {
  if (FLAGS_output_errors.empty()) return nullptr;
  if (FLAGS_output_errors == "-")
    LOG(INFO) << "Will perform label error evaluation that will LOG the top errors.";
  else
    LOG(INFO) << "Will perform evaluation that will output to " << FLAGS_output_errors;
  return new SingleLabelErrorStats();
}

void OutputLabelErrorStats(const SingleLabelErrorStats* stats) {
  if (stats == nullptr) return;
  if (FLAGS_output_errors == "-") return;
  LOG(INFO) << "Outputting error stats to " << FLAGS_output_errors << "...";

  std::vector<std::pair<int, std::string> > best_stats;
  for (auto it = stats->errors_and_counts.begin(); it != stats->errors_and_counts.end(); ++it) {
    best_stats.push_back(std::pair<int, std::string>(it->second, it->first));
  }
  std::sort(best_stats.begin(), best_stats.end(), std::greater<std::pair<int, std::string> >());
  FILE* f = fopen(FLAGS_output_errors.c_str(), "wt");
  for (size_t i = 0; i < best_stats.size(); ++i) {
    fprintf(f, "%8d : %s\n", best_stats[i].first, best_stats[i].second.c_str());
  }
  fclose(f);
  LOG(INFO) << "Error stats written.";
}

template <class InputType>
void Evaluate(RecordInput<InputType>* evaluation_data, GraphInference* inference,
              PrecisionStats* total_stats, SingleLabelErrorStats* error_stats, Adapter<InputType> &adapter) {
  LOG(INFO) << "Evaluating...";
  int64 start_time = GetCurrentTimeMicros();
  PrecisionStats stats;
  ParallelForeachInput(evaluation_data, [&inference,&stats,error_stats](const Query& query) {
    std::unique_ptr<Nice2Query> q(inference->CreateQuery());
    q->FromFeaturesQueryProto(query.features());
    std::unique_ptr<Nice2Assignment> a(inference->CreateAssignment(q.get()));
    a->FromNodeAssignmentsProto(query.node_assignments());
    std::unique_ptr<Nice2Assignment> refa(inference->CreateAssignment(q.get()));
    refa->FromNodeAssignmentsProto(query.node_assignments());

    a->ClearInferredAssignment();
    inference->MapInference(q.get(), a.get());
    a->CompareAssignments(refa.get(), &stats);
    if (error_stats != nullptr)
      a->CompareAssignmentErrors(refa.get(), error_stats);
  }, adapter);
  int64 end_time = GetCurrentTimeMicros();
  LOG(INFO) << "Evaluation pass took " << (end_time - start_time) / 1000 << "ms.";


  LOG(INFO) << "Correct " << stats.correct_labels << " vs " << stats.incorrect_labels << " incorrect labels";
  double error_rate = stats.incorrect_labels / (static_cast<double>(stats.incorrect_labels + stats.correct_labels));
  LOG(INFO) << "Error rate of " << std::fixed << error_rate;
  PrintLabelErrorStatsSummary(error_stats);

  total_stats->AddStats(stats);
}

template <class InputType>
int EvalMain(Adapter<InputType> adapter) {
  if (FLAGS_debug_stats) {
    GraphInference inference;
    inference.LoadModel(FLAGS_model);
    inference.PrintDebugInfo();
  } else {
    std::unique_ptr<SingleLabelErrorStats> error_stats(CreateLabelErrorStats());

    GraphInference inference;
    std::unique_ptr<RecordInput<InputType>> input;

    input.reset(new FileRecordInput<InputType>(FLAGS_input, FLAGS_input_records));
    inference.LoadModel(FLAGS_model);
    PrecisionStats total_stats;
    Evaluate(input.get(), &inference, &total_stats, error_stats.get(), adapter);
    OutputLabelErrorStats(error_stats.get());
    // No need to print total_stats. Evaluate() already prints info.
  }
  return 0;
}

#endif //NICE2PREDICT_EVAL_INTERNAL_H
