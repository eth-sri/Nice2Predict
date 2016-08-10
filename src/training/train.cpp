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

#include <string>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>
#include <math.h>

#include "base.h"
#include "gflags/gflags.h"
#include "glog/logging.h"

#include "jsoncpp/json/json.h"
#include "graph_inference.h"

#include "stringprintf.h"
#include "stringset.h"
#include "readerutil.h"

const std::string SSVM_TRAIN_NAME = "ssvm";
const std::string PL_TRAIN_NAME = "pl";
const std::string PL_SSVM_TRAIN_NAME = "pl_ssvm";

const std::string NO_LEARN_RATE_UPDATE_PL = "fixed";
const std::string PROP_SQRT_PASS_LEARN_RATE_UPDATE_PL = "prop_sqrt_pass";
const std::string PROP_PASS_LEARN_RATE_UPDATE_PL = "prop_pass";
const std::string PROP_INITIAL_LEARN_RATE_AND_PASS_LEARN_RATE_UPDATE_PL = "prop_pass_and_initial_learn_rate";

DEFINE_string(input, "testdata", "Input file with JSON objects regarding training data");
DEFINE_string(out_model, "model", "File prefix for output models");
DEFINE_bool(hogwild, true, "Whether to use Hogwild parallel training.");
DEFINE_int32(num_threads, 8, "Number of threads to use.");
DEFINE_int32(num_training_passes, 24, "Number of passes in training.");

DEFINE_double(start_learning_rate, 0.1, "Initial learning rate");
DEFINE_double(stop_learning_rate, 0.0001, "Stop learning if learning rate falls below the value");
DEFINE_double(regularization_const, 2.0, "Regularization constant. The higher, the more regularization.");
DEFINE_double(svm_margin, 0.1, "SVM Margin = Penalty for keeping equal labels as in the training data during training.");
DEFINE_int32(max_labels_z, 16, "Number of labels considered for the normalization function when training with pseudolikelihood");

DEFINE_int32(cross_validation_folds, 0, "If more than 1, cross-validation is performed with the specified number of folds");
DEFINE_bool(print_confusion, false, "Print confusion statistics instead of training.");

DEFINE_string(training_method, SSVM_TRAIN_NAME, "Training method to be used.");

DEFINE_int32(num_pass_change_training,  10, "When using pseudolikelihood combined with SSVM for the training, this indicates after which pass change the training to SSVM");
DEFINE_double(initial_learning_rate_ssvm, 0.1, "Initial learning rate of SSVM in the combined version.");
DEFINE_string(learning_rate_update_formula_pl, PROP_PASS_LEARN_RATE_UPDATE_PL,"Learning update formula for PL learning. ");
DEFINE_double(pl_lambda, 1.0, "Lambda used in the formula for computing the learning rate proportional to the training pass and the initial learning rate.");

typedef std::function<void(const Json::Value&, const Json::Value&)> InputProcessor;
void ForeachInput(RecordInput* input, InputProcessor proc) {
  std::unique_ptr<InputRecordReader> reader(input->CreateReader());
  Json::Reader jsonreader;
  while (!reader->ReachedEnd()) {
    std::string line;
    reader->Read(&line);
    if (line.empty()) continue;
    Json::Value v;
    if (!jsonreader.parse(line, v, false)) {
      LOG(ERROR) << "Could not parse input: " << jsonreader.getFormattedErrorMessages();
    } else {
      proc(v["query"], v["assign"]);
    }
  }
}

void ProcessLinesParallel(InputRecordReader* reader, InputProcessor proc) {
  std::string line;
  Json::Reader jsonreader;
  while (!reader->ReachedEnd()) {
    std::string line;
    reader->Read(&line);
    if (line.empty()) continue;
    Json::Value v;
    if (!jsonreader.parse(line, v, false)) {
      LOG(ERROR) << "Could not parse input: " << jsonreader.getFormattedErrorMessages() << "\n" << line;
    } else {
      proc(v["query"], v["assign"]);
    }
  }
}

typedef std::function<void(const std::string&)> RawInputProcessor;
void ProcessRawLinesParallel(InputRecordReader* reader, RawInputProcessor proc) {
  std::string line;
  while (!reader->ReachedEnd()) {
    std::string line;
    reader->Read(&line);
    if (line.empty()) continue;
    proc(line);
  }
}

void ParallelForeachRawInput(RecordInput* input, RawInputProcessor proc) {
  // Do parallel ForEach
  std::unique_ptr<InputRecordReader> reader(input->CreateReader());
  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    threads.push_back(std::thread(std::bind(&ProcessRawLinesParallel, reader.get(), proc)));
  }
  for (auto& thread : threads){
    thread.join();
  }
}

void ParallelForeachInput(RecordInput* input, InputProcessor proc) {
  if (!FLAGS_hogwild) {
    ForeachInput(input, proc);
    return;
  }

  // Do parallel ForEach
  std::unique_ptr<InputRecordReader> reader(input->CreateReader());
  std::vector<std::thread> threads;
  for (int i = 0; i < FLAGS_num_threads; ++i) {
    threads.push_back(std::thread(std::bind(&ProcessLinesParallel, reader.get(), proc)));
  }
  for (auto& thread : threads){
    thread.join();
  }
}

void InitTrain(RecordInput* input, GraphInference* inference) {
  int count = 0;
  std::mutex mutex;
  ParallelForeachInput(input, [&inference,&count,&mutex](const Json::Value& query, const Json::Value& assign) {
    std::lock_guard<std::mutex> lock(mutex);
    inference->AddQueryToModel(query, assign);
    ++count;
  });
  LOG(INFO) << "Loaded " << count << " training data samples.";
  inference->PrepareForInference();
}




void TestInference(RecordInput* input, GraphInference* inference) {
  int64 start_time = GetCurrentTimeMicros();
  double score_gain = 0;
  // int count = 0;
  ForeachInput(input, [&](const Json::Value& query, const Json::Value& assign) {
    // if (count >= 10) return;
    // count++;
    Nice2Query* q = inference->CreateQuery();
    q->FromJSON(query);
    Nice2Assignment* a = inference->CreateAssignment(q);
    a->FromJSON(assign);
    double start_score = inference->GetAssignmentScore(a);
    inference->MapInference(q, a);
    score_gain += inference->GetAssignmentScore(a) - start_score;
    delete a;
    delete q;
  });
  int64 end_time = GetCurrentTimeMicros();
  LOG(INFO) << "Inference took " << (end_time - start_time) / 1000 << "ms for gain of " << score_gain << ".";

}

void TrainPL(RecordInput* input, GraphInference* inference, int num_training_passes, double start_learning_rate) {
  inference->InitializeFeatureWeights(FLAGS_regularization_const);
  inference->PLInit(FLAGS_max_labels_z);
  double learning_rate = start_learning_rate;
  LOG(INFO) << "Starting training using pseudolikelihood as objective function with --start_learning_rate=" << std::fixed << start_learning_rate
          << ", --regularization_const=" << std::fixed << FLAGS_regularization_const
          << " and --max_labels_z=" << std::fixed << FLAGS_max_labels_z;

  for (int pass = 0; pass < num_training_passes; ++pass) {

    int64 start_time = GetCurrentTimeMicros();
    if (FLAGS_learning_rate_update_formula_pl.compare(PROP_SQRT_PASS_LEARN_RATE_UPDATE_PL) == 0) {
      learning_rate /= pow(pass + 1, 0.5);
    } else if (FLAGS_learning_rate_update_formula_pl.compare(PROP_PASS_LEARN_RATE_UPDATE_PL) == 0) {
      learning_rate /= (pass + 1);
    } else if (FLAGS_learning_rate_update_formula_pl.compare(PROP_INITIAL_LEARN_RATE_AND_PASS_LEARN_RATE_UPDATE_PL) == 0) {
      learning_rate = start_learning_rate / (1 + FLAGS_pl_lambda * (pass + 1));
    }

    ParallelForeachInput(input, [&inference,&learning_rate,pass](const Json::Value& query, const Json::Value& assign) {
      std::unique_ptr<Nice2Query> q(inference->CreateQuery());
      q->FromJSON(query);
      std::unique_ptr<Nice2Assignment> a(inference->CreateAssignment(q.get()));
      a->FromJSON(assign);
      inference->PLLearn(q.get(), a.get(), learning_rate);
    });

    int64 end_time = GetCurrentTimeMicros();
    LOG(INFO) << "Training pass took " << (end_time - start_time) / 1000 << "ms.";

    LOG(INFO) << "Pass " << pass << " with learning rate " << learning_rate;
    if (learning_rate < FLAGS_stop_learning_rate) break;  // Stop learning in this case.
    inference->PrepareForInference();
  }
}

void TrainSSVM(RecordInput* input, GraphInference* inference, int num_training_passes, double start_learning_rate) {
  if (FLAGS_training_method.compare(PL_SSVM_TRAIN_NAME) != 0) {
    inference->InitializeFeatureWeights(FLAGS_regularization_const);
  }
  inference->SSVMInit(FLAGS_svm_margin);

  double learning_rate = start_learning_rate;

  LOG(INFO) << "Starting SSVM training with --start_learning_rate=" << std::fixed << start_learning_rate
       << ", --regularization_const=" << std::fixed << FLAGS_regularization_const
       << " and --svm_margin=" << std::fixed << FLAGS_svm_margin;

  double last_error_rate = 1.0;
  for (int pass = 0; pass < num_training_passes; ++pass) {
    double error_rate = 0.0;

    GraphInference backup_inference(*inference);

    int64 start_time = GetCurrentTimeMicros();
    PrecisionStats stats;

    ParallelForeachInput(input, [&inference,&stats,&learning_rate,pass](const Json::Value& query, const Json::Value& assign) {
      std::unique_ptr<Nice2Query> q(inference->CreateQuery());
      q->FromJSON(query);
      std::unique_ptr<Nice2Assignment> a(inference->CreateAssignment(q.get()));
      a->FromJSON(assign);
      inference->SSVMLearn(q.get(), a.get(), learning_rate, &stats);
    });

    int64 end_time = GetCurrentTimeMicros();
    LOG(INFO) << "Training pass took " << (end_time - start_time) / 1000 << "ms.";

    LOG(INFO) << "Correct " << stats.correct_labels << " vs " << stats.incorrect_labels << " incorrect labels.";
    error_rate = stats.incorrect_labels / (static_cast<double>(stats.incorrect_labels + stats.correct_labels));
    LOG(INFO) << "Pass " << pass << " with learning rate " << learning_rate << " has error rate of " << std::fixed << error_rate;
    if (error_rate > last_error_rate) {
      LOG(INFO) << "Reverting last pass.";
      learning_rate *= 0.5;  // Halve the learning rate.
      *inference = backup_inference;
      if (learning_rate < FLAGS_stop_learning_rate) break;  // Stop learning in this case.
    } else {
      last_error_rate = error_rate;
    }
    inference->PrepareForInference();
  }
}



void PrintConfusion() {
  std::unique_ptr<RecordInput> input(new FileRecordInput(FLAGS_input));
  NodeConfusionStats confusion_stats;
  ForeachInput(input.get(), [&confusion_stats](const Json::Value& query, const Json::Value& assign) {
    GraphInference inference;
    inference.AddQueryToModel(query, assign);
    std::unique_ptr<Nice2Query> q(inference.CreateQuery());
    q->FromJSON(query);
    std::unique_ptr<Nice2Assignment> a(inference.CreateAssignment(q.get()));
    a->FromJSON(assign);

    inference.PrintConfusionStatistics(q.get(), a.get(), &confusion_stats);
    LOG(INFO) << "Confusion statistics. non-confusable nodes:" << confusion_stats.num_non_confusable_nodes
        << ", confusable nodes:" << confusion_stats.num_confusable_nodes
        << ". Num expected confusion errors:" << confusion_stats.num_expected_confusions;
  });
}

void Evaluate(RecordInput* evaluation_data, GraphInference* inference,
    PrecisionStats* total_stats) {
  int64 start_time = GetCurrentTimeMicros();
  PrecisionStats stats;
  ParallelForeachInput(evaluation_data, [&inference,&stats](const Json::Value& query, const Json::Value& assign) {
    std::unique_ptr<Nice2Query> q(inference->CreateQuery());
    q->FromJSON(query);
    std::unique_ptr<Nice2Assignment> a(inference->CreateAssignment(q.get()));
    a->FromJSON(assign);
    std::unique_ptr<Nice2Assignment> refa(inference->CreateAssignment(q.get()));
    refa->FromJSON(assign);

    a->ClearInferredAssignment();
    inference->MapInference(q.get(), a.get());
    a->CompareAssignments(refa.get(), &stats);
  });
  int64 end_time = GetCurrentTimeMicros();
  LOG(INFO) << "Evaluation pass took " << (end_time - start_time) / 1000 << "ms.";


  LOG(INFO) << "Correct " << stats.correct_labels << " vs " << stats.incorrect_labels << " incorrect labels";
  LOG(INFO) << "Made prediction that were not unknown for " << stats.num_known_predictions << " labels";
  double error_rate = stats.incorrect_labels / (static_cast<double>(stats.incorrect_labels + stats.correct_labels));
  LOG(INFO) << "Error rate of " << std::fixed << error_rate;

  total_stats->AddStats(stats);
}

int main(int argc, char** argv) {
  google::InstallFailureSignalHandler();
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  if (FLAGS_cross_validation_folds > 1) {
    PrecisionStats total_stats;
    for (int fold_id = 0; fold_id < FLAGS_cross_validation_folds; ++fold_id) {
      GraphInference inference;
      std::unique_ptr<RecordInput> training_data(
          new ShuffledCacheInput(new CrossValidationInput(new FileRecordInput(FLAGS_input),
              fold_id, FLAGS_cross_validation_folds, true)));
      std::unique_ptr<RecordInput> validation_data(
          new ShuffledCacheInput(new CrossValidationInput(new FileRecordInput(FLAGS_input),
              fold_id, FLAGS_cross_validation_folds, false)));
      LOG(INFO) << "Training fold " << fold_id;
      InitTrain(training_data.get(), &inference);
      if (FLAGS_training_method.compare(PL_TRAIN_NAME) == 0) {
        TrainPL(training_data.get(), &inference, FLAGS_num_training_passes, FLAGS_start_learning_rate);
      } else if (FLAGS_training_method.compare(SSVM_TRAIN_NAME) == 0) {
        TrainSSVM(training_data.get(), &inference, FLAGS_num_training_passes, FLAGS_start_learning_rate);
      } else if (FLAGS_training_method.compare(PL_SSVM_TRAIN_NAME) == 0) {
        TrainPL(training_data.get(), &inference, FLAGS_num_pass_change_training, FLAGS_start_learning_rate);
        TrainSSVM(training_data.get(), &inference, FLAGS_num_training_passes, FLAGS_initial_learning_rate_ssvm);
      } else {
        LOG(INFO) << "ERROR: training method name not recognized";
        return 1;
      }
      LOG(INFO) << "Evaluating fold " << fold_id;
      Evaluate(validation_data.get(), &inference, &total_stats);
    }
    // Output results of cross-validation (no model is saved in this mode).
    LOG(INFO) << "========================================";
    LOG(INFO) << "Cross-validation done";
    LOG(INFO) << "Correct " << total_stats.correct_labels << " vs " << total_stats.incorrect_labels << " incorrect labels for the whole dataset";
    LOG(INFO) << "Made prediction that were not unknown for " << total_stats.num_known_predictions << " labels";
    double error_rate = total_stats.incorrect_labels / (static_cast<double>(total_stats.incorrect_labels + total_stats.correct_labels));
    LOG(INFO) << "Error rate of " << std::fixed << error_rate;
  } else if (FLAGS_print_confusion) {
    PrintConfusion();
  } else {
    LOG(INFO) << "Running structured training...";
    // Structured training.
    GraphInference inference;
    std::unique_ptr<RecordInput> input(new ShuffledCacheInput(new FileRecordInput(FLAGS_input)));
    InitTrain(input.get(), &inference);
    if (FLAGS_training_method.compare(PL_TRAIN_NAME) == 0) {
      TrainPL(input.get(), &inference, FLAGS_num_training_passes, FLAGS_start_learning_rate);
    } else if (FLAGS_training_method.compare(SSVM_TRAIN_NAME) == 0) {
      TrainSSVM(input.get(), &inference, FLAGS_num_training_passes, FLAGS_start_learning_rate);
    } else if (FLAGS_training_method.compare(PL_SSVM_TRAIN_NAME) == 0) {
      TrainPL(input.get(), &inference, FLAGS_num_pass_change_training, FLAGS_start_learning_rate);
      TrainSSVM(input.get(), &inference, FLAGS_num_training_passes, FLAGS_initial_learning_rate_ssvm);
    } else {
      LOG(INFO) << "ERROR: training method name not recognized";
      return 1;
    }
    // Save the model in the regular training.
    inference.SaveModel(FLAGS_out_model);
  }

  return 0;
}
