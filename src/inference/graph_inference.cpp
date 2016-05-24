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

#include "graph_inference.h"

#include <stdio.h>
#include <math.h>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <iterator>

#include "stringprintf.h"
#include "glog/logging.h"

#include "base.h"
#include "maputil.h"
#include "nbest.h"
#include "simple_histogram.h"
#include "updatable_priority_queue.h"

#include "label_set.h"
#include "graph_query.h"
#include "graph_node_assignment.h"
//#define DEBUG

DEFINE_bool(initial_greedy_assignment_pass, true, "Whether to run an initial greedy assignment pass.");
DEFINE_bool(duplicate_name_resolution, true, "Whether to attempt a duplicate name resultion on conflicts.");
DEFINE_int32(graph_per_node_passes, 8, "Number of per-node passes for inference");
DEFINE_int32(graph_per_arc_passes, 5, "Number of per-arc passes for inference");
DEFINE_int32(graph_per_factor_passes, 1, "Number of per-factor passes for inference");
DEFINE_int32(graph_loopy_bp_passes, 0, "Number of loopy belief propagation passes for inference");
DEFINE_int32(graph_loopy_bp_steps_per_pass, 3, "Number of loopy belief propagation steps in each inference pass");

DEFINE_int32(maximum_depth, 2, "Maximum depth when looking for factor candidates");
DEFINE_int32(factors_limit, 64, "Factors limit before which stop to go deeper");

DEFINE_string(valid_labels, "valid_names.txt", "A file describing valid names");

static const size_t kStartPerArcBeamSize = 4;
static const size_t kMaxPerArcBeamSize = 64;

static const size_t kStartPerNodeBeamSize = 4;
static const size_t kMaxPerNodeBeamSize = 64;

static const size_t kLoopyBPBeamSize = 32;


#ifdef GRAPH_INFERENCE_STATS
struct GraphInferenceStats {
  GraphInferenceStats()
  : position_of_best_per_node_label(10),
    position_of_best_per_arc_label(10),
    label_candidates_per_node(10) {
  }

  SimpleHistogram position_of_best_per_node_label;
  SimpleHistogram position_of_best_per_arc_label;
  SimpleHistogram label_candidates_per_node;

  std::string ToString() const {
    std::string result;
    result += "Candidates per node:\n";
    result += label_candidates_per_node.ToString();
    result += "Improving position @ node:\n";
    result += position_of_best_per_node_label.ToString();
    result += "Improving position @ arc:\n";
    result += position_of_best_per_arc_label.ToString();
    return result;
  }
};
#endif

class LoopyBPInference {
public:
  LoopyBPInference(const GraphNodeAssignment& a, const GraphInference& fweights)
: a_(a), fweights_(fweights) {
    node_label_to_score_.set_empty_key(IntPair(-1, -1));
    node_label_to_score_.set_deleted_key(IntPair(-2, -2));
    labels_at_node_.assign(a.assignments_.size(), std::vector<int>());
  }

  void Run(GraphNodeAssignment* a) {
    InitPossibleLabels();
    for (int pass = 0; pass < FLAGS_graph_loopy_bp_steps_per_pass; ++pass) {
      PullMessagesFromAdjacentNodes();
    }
    TraceBack(a);
  }

  void TraceBack(GraphNodeAssignment* a) {
    std::vector< std::pair<double, IntPair> > scores;
    std::vector<bool> node_visited(a_.assignments_.size(), false);
    scores.reserve(node_label_to_score_.size());
    for (auto it = node_label_to_score_.begin(); it != node_label_to_score_.end(); ++it) {
      scores.push_back(std::pair<double, IntPair>(it->second.total_score, it->first));
    }
    std::sort(scores.begin(), scores.end(), std::greater< std::pair<double, IntPair> >());
    std::queue<IntPair> traversal_queue;
    for (size_t si = 0; si < scores.size(); ++si) {
      traversal_queue.push(scores[si].second);
      while (!traversal_queue.empty()) {
        IntPair node_label = traversal_queue.front();
        traversal_queue.pop();
        if (node_visited[node_label.first]) continue;
        node_visited[node_label.first] = true;
        if (a->assignments_[node_label.first].must_infer) {
          a->assignments_[node_label.first].label = node_label.second;
        }
        const BPScore& s = FindWithDefault(node_label_to_score_, node_label, empty_bp_score_);
        for (auto it = s.incoming_node_to_message.begin(); it != s.incoming_node_to_message.end(); ++it) {
          int next_node = it->first;
          int next_label = it->second.label;
          traversal_queue.push(IntPair(next_node, next_label));
        }
      }
    }
  }

  std::string DebugString() const {
    std::string result;
    for (size_t node = 0; node < a_.assignments_.size(); ++node) {
      if (!a_.assignments_[node].must_infer) continue;
      StringAppendF(&result, "\nNode %d:\n", static_cast<int>(node));
      for (int label : labels_at_node_[node]) {
        const BPScore& score = FindWithDefault(node_label_to_score_, IntPair(node, label), empty_bp_score_);
        StringAppendF(&result, "  Label %s  -- %f:\n", a_.label_set_->GetLabelName(label), score.total_score);
        for (auto it = score.incoming_node_to_message.begin(); it != score.incoming_node_to_message.end(); ++it) {
          StringAppendF(&result, "    From %d: %s -- %f [ arc %f ]\n", it->first, a_.label_set_->GetLabelName(it->second.label), it->second.score,
              a_.GetNodePairScore(fweights_, it->first, node, it->second.label, label));
        }
      }
    }
    return result;
  }

private:
  struct IncomingMessage {
    IncomingMessage(int l, double s) : label(l), score(s) {}
    IncomingMessage() : label(-1), score(0.0) {}
    int label;
    double score;
  };

  struct BPScore {
    BPScore() : total_score(0.0) {}
    double total_score;
    std::unordered_map<int, IncomingMessage> incoming_node_to_message;
  };
  BPScore empty_bp_score_;

  const GraphNodeAssignment& a_;
  const GraphInference& fweights_;

  google::dense_hash_map<IntPair, BPScore> node_label_to_score_;
  std::vector<std::vector<int> > labels_at_node_;

  IncomingMessage GetBestMessageFromNode(int from_node, int to_node, int to_label) {
    if (!a_.assignments_[from_node].must_infer) {
      int from_label = a_.assignments_[from_node].label;
      return IncomingMessage(from_label, a_.GetNodePairScore(fweights_, from_node, to_node, from_label, to_label));
    }
    double best_score = 0.0;
    int best_label = -1;
    for (int from_label : labels_at_node_[from_node]) {
      auto it = node_label_to_score_.find(IntPair(from_node, from_label));
      if (it == node_label_to_score_.end()) continue;
      double node_score = it->second.total_score - it->second.incoming_node_to_message[to_node].score;
      double current_score = node_score + a_.GetNodePairScore(fweights_, from_node, to_node, from_label, to_label);
      if (current_score > best_score) {
        best_score = current_score;
        best_label = from_label;
      }
    }
    return IncomingMessage(best_label, best_score);
  }

  void PullMessagesFromAdjacentNodes() {
    for (int node = 0; node < static_cast<int>(a_.assignments_.size()); ++node) {
      for (int label : labels_at_node_[node]) {
        //for (auto it = node_label_to_score_.begin(); it != node_label_to_score_.end(); ++it) {
        auto it = node_label_to_score_.find(IntPair(node, label));
        if (it == node_label_to_score_.end()) continue;
        //int node = it->first.first;
        //int label = it->first.second;
        BPScore& score = it->second;
        for (auto msg_it = score.incoming_node_to_message.begin(); msg_it != score.incoming_node_to_message.end(); ++msg_it) {
          IncomingMessage new_msg = GetBestMessageFromNode(msg_it->first, node, label);
          IncomingMessage& old_msg = msg_it->second;
          double score_improve = new_msg.score - old_msg.score;
          score.total_score += score_improve;
          old_msg = new_msg;
        }
        //}
      }
    }
  }

  void PutPossibleLabelsAtAdjacentNodes(int node, int label, size_t beam_size) {
    std::vector<std::pair<double, int> > empty_vec;
    for (const Arc& arc : a_.query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a == node) {
        if (a_.assignments_[arc.node_b].must_infer) {
          const std::vector<std::pair<double, int> >& v =
              FindWithDefault(fweights_.best_features_for_a_type_, IntPair(label, arc.type), empty_vec);
          for (size_t i = 0; i < v.size() && i < beam_size; ++i) {
            PutPossibleLabelAtNode(arc.node_b, v[i].second);
          }
        }
      }
      if (arc.node_b == node) {
        if (a_.assignments_[arc.node_a].must_infer) {
          const std::vector<std::pair<double, int> >& v =
              FindWithDefault(fweights_.best_features_for_b_type_, IntPair(label, arc.type), empty_vec);
          for (size_t i = 0; i < v.size() && i < beam_size; ++i) {
            PutPossibleLabelAtNode(arc.node_a, v[i].second);
          }
        }
      }
    }
  }

  void PutPossibleLabelAtNode(int node, int label) {
    auto ins = node_label_to_score_.insert(std::pair<IntPair, BPScore>(IntPair(node, label), empty_bp_score_));
    if (ins.second) {
      labels_at_node_[node].push_back(label);
      if (label == a_.penalties_[node].label) {
        ins.first->second.total_score = -a_.penalties_[node].penalty;
      }
      for (const Arc& arc : a_.query_->arcs_adjacent_to_node_[node]) {
        if (arc.node_a == node) {
          ins.first->second.incoming_node_to_message[arc.node_b] = IncomingMessage();
        }
        if (arc.node_b == node) {
          ins.first->second.incoming_node_to_message[arc.node_a] = IncomingMessage();
        }
      }
    }
  }

  void InitPossibleLabels() {
    for (size_t i = 0; i < labels_at_node_.size(); ++i) {
      if (a_.assignments_[i].must_infer) {
        PutPossibleLabelAtNode(i, a_.assignments_[i].label);
        PutPossibleLabelsAtAdjacentNodes(i, a_.assignments_[i].label, kLoopyBPBeamSize);
      }
    }
  }
};

GraphInference::GraphInference() : regularizer_(1.0), svm_margin_(1e-9), num_svm_training_samples_(0) {
  // Initialize dense_hash_map.
  features_.set_empty_key(GraphFeature(-1, -1, -1));
  features_.set_deleted_key(GraphFeature(-2, -2, -2));
  best_features_for_type_.set_empty_key(-1);
  best_features_for_type_.set_deleted_key(-2);
}

GraphInference::~GraphInference() {
}

void GraphInference::LoadModel(const std::string& file_prefix) {
  LOG(INFO) << "Loading model " << file_prefix << "...";
  features_.clear();

  FILE* ffile = fopen(StringPrintf("%s_features", file_prefix.c_str()).c_str(), "rb");
  int num_features = 0;
  int num_factor_features = 0;
  CHECK_EQ(1, fread(&num_features, sizeof(int), 1, ffile));
  CHECK_EQ(1, fread(&num_factor_features, sizeof(int), 1, ffile));
  for (int i = 0; i < num_features; ++i) {
    GraphFeature f(0, 0, 0);
    double score;
    CHECK_EQ(1, fread(&f, sizeof(GraphFeature), 1, ffile));
    CHECK_EQ(1, fread(&score, sizeof(double), 1, ffile));
    features_[f].setValue(score);
  }
  for (int i = 0; i < num_factor_features; i++) {
    Factor f;
    int size_of_factor = 0;
    CHECK_EQ(1, fread(&size_of_factor, sizeof(int), 1, ffile));
    for (int j = 0; j < size_of_factor; j++) {
      int f_var = -1;
      CHECK_EQ(1, fread(&f_var, sizeof(int), 1, ffile));
      f.insert(f_var);
    }
    double score;
    CHECK_EQ(1, fread(&score, sizeof(double), 1, ffile));
    factor_features_[f] = score;
  }
  fclose(ffile);
  CHECK_EQ(features_.size(), num_features);

  FILE* sfile = fopen(StringPrintf("%s_strings", file_prefix.c_str()).c_str(), "rb");
  strings_.loadFromFile(sfile);
  fclose(sfile);
  LOG(INFO) << "Loading model done";

  PrepareForInference();
}

void GraphInference::SaveModel(const std::string& file_prefix) {
  LOG(INFO) << "Saving model " << file_prefix << "...";
  FILE* ffile = fopen(StringPrintf("%s_features", file_prefix.c_str()).c_str(), "wb");
  int num_features = features_.size();
  int num_factor_features = factor_features_.size();
  fwrite(&num_features, sizeof(int), 1, ffile);
  fwrite(&num_factor_features, sizeof(int), 1, ffile);
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    fwrite(&it->first, sizeof(GraphFeature), 1, ffile);
    double value = it->second.getValue();
    fwrite(&value, sizeof(double), 1, ffile);
  }
  for (auto f = factor_features_.begin(); f != factor_features_.end(); f++) {
    int size_of_factor = f->first.size();
    fwrite(&size_of_factor, sizeof(int), 1, ffile);
    for (auto var = f->first.begin(); var != f->first.end(); var++) {
      int var_val = *var;
      fwrite(&var_val, sizeof(int), 1, ffile);
    }
    double value = f->second;
    fwrite(&value, sizeof(double), 1, ffile);
  }
  fclose(ffile);

  FILE* sfile = fopen(StringPrintf("%s_strings", file_prefix.c_str()).c_str(), "wb");
  strings_.saveToFile(sfile);
  fclose(sfile);
  LOG(INFO) << "Saving model done";
}

Nice2Query* GraphInference::CreateQuery() const {
  return new GraphQuery(&strings_, &label_checker_);
}
Nice2Assignment* GraphInference::CreateAssignment(Nice2Query* query) const {
  GraphQuery* q = static_cast<GraphQuery*>(query);
  return new GraphNodeAssignment(q, &q->label_set_);
}
void GraphInference::PerformAssignmentOptimization(GraphNodeAssignment* a) const {
  double score = a->GetTotalScore(*this);
  VLOG(1) << "Start score " << score;
  if (FLAGS_initial_greedy_assignment_pass) {
    a->InitialGreedyAssignmentPass(*this);
    score = a->GetTotalScore(*this);
    VLOG(1) << "Past greedy pass score " << score;
  }

  int passes = std::max(FLAGS_graph_per_node_passes, std::max(FLAGS_graph_loopy_bp_passes, FLAGS_graph_per_arc_passes));
  size_t per_node_beam_size = kStartPerNodeBeamSize;
  size_t per_arc_beam_size = kStartPerArcBeamSize;
  for (int pass = 0; pass < passes; ++pass) {
    if (pass < FLAGS_graph_loopy_bp_passes) {
      VLOG(1) << "prescore  " << score;
      int64 start_time = GetCurrentTimeMicros();
      LoopyBPInference bp(*a, *this);
      bp.Run(a);
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "LoopyBP pass " << (end_time - start_time)/1000 << "ms.";
      VLOG(1) << "BP score  " << a->GetTotalScore(*this);
    }
    if (pass < FLAGS_graph_per_node_passes) {
      int64 start_time = GetCurrentTimeMicros();
      if (FLAGS_duplicate_name_resolution) {
        a->LocalPerNodeOptimizationPassWithDuplicateNameResolution(*this, per_node_beam_size);
      } else {
        a->LocalPerNodeOptimizationPass(*this, per_node_beam_size);
      }
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "Per node pass " << (end_time - start_time)/1000 << "ms.";

      per_node_beam_size = std::min( per_node_beam_size * 2, kMaxPerNodeBeamSize);
    }
    if (pass < FLAGS_graph_per_arc_passes) {
      int64 start_time = GetCurrentTimeMicros();
      a->LocalPerArcOptimizationPass(*this, per_arc_beam_size);
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "Per arc pass " << (end_time - start_time)/1000 << "ms.";

      per_arc_beam_size = std::min(per_arc_beam_size * 2, kMaxPerArcBeamSize);
    }
    if (pass < FLAGS_graph_per_factor_passes) {
      int64 start_time = GetCurrentTimeMicros();
      a->LocalPerFactorOptimizationPass(*this, FLAGS_factors_limit);
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "Per factor pass " << (end_time - start_time)/1000 << "ms.";
    }

    double updated_score = a->GetTotalScore(*this);
    VLOG(2) << "Got to score " << updated_score;
    if (updated_score == score) break;
    score = updated_score;
  }
  VLOG(1) << "End score   " << score;
#ifdef GRAPH_INFERENCE_STATS
  VLOG(2) << a->stats_.ToString();
#endif
}

void GraphInference::MapInference(
    const Nice2Query* query,
    Nice2Assignment* assignment) const {
  GraphNodeAssignment* a = static_cast<GraphNodeAssignment*>(assignment);
  PerformAssignmentOptimization(a);
}

double GraphInference::GetAssignmentScore(const Nice2Assignment* assignment) const {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);
  return a->GetTotalScore(*this);
}

void GraphInference::InitializeFeatureWeights(double regularization) {
  regularizer_ = 1 / regularization;
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    it->second.setValue(regularizer_ * 0.5);
  }
  for (auto it = factor_features_.begin(); it != factor_features_.end(); it++) {
    it->second = regularizer_ * 0.5;
  }
}

void GraphInference::SSVMInit(double margin) {
  svm_margin_ = margin;
}

void GraphInference::PLInit(int beam_size) {
  beam_size_ = beam_size;
}

void GraphInference::SSVMLearn(
    const Nice2Query* query,
    const Nice2Assignment* assignment,
    double learning_rate,
    PrecisionStats* stats) {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);

  GraphNodeAssignment new_assignment(*a);
  new_assignment.SetUpEqualityPenalty(svm_margin_);
  PerformAssignmentOptimization(&new_assignment);

  int correct_labels = 0, incorrect_labels = 0;
  for (size_t i = 0; i < new_assignment.assignments_.size(); ++i) {
    if (new_assignment.assignments_[i].must_infer) {
      if (new_assignment.assignments_[i].label == a->assignments_[i].label) {
        ++correct_labels;
      } else {
        ++incorrect_labels;
      }
    }
  }
  {  // Atomic update stats.
    std::lock_guard<std::mutex> guard(stats->lock);
    stats->correct_labels += correct_labels;
    stats->incorrect_labels += incorrect_labels;
    ++num_svm_training_samples_;
    if (num_svm_training_samples_ % 10000 == 0) {
      double error_rate = stats->incorrect_labels / (static_cast<double>(stats->incorrect_labels + stats->correct_labels));
      LOG(INFO) << "At training sample " << num_svm_training_samples_ << ": error rate of " << std::fixed << error_rate;
    }
  }

  // Perform gradient descent.
  SimpleFeaturesMap affected_features;  // Gradient for each affected feature.
  FactorFeaturesMap factor_affected_features;
  affected_features.set_empty_key(GraphFeature(-1, -1, -1));
  affected_features.set_deleted_key(GraphFeature(-2, -2, -2));
  a->GetAffectedFeatures(&affected_features, learning_rate);
  a->GetAffectedFactorFeatures(&factor_affected_features, learning_rate);
  new_assignment.GetAffectedFeatures(&affected_features, -learning_rate);
  new_assignment.GetAffectedFactorFeatures(&factor_affected_features, -learning_rate);
  for (auto it = affected_features.begin(); it != affected_features.end(); ++it) {
    if (it->second < -1e-9 || it->second > 1e-9) {
      VLOG(3) << a->GetLabelName(it->first.a_) << " " << a->GetLabelName(it->first.b_) << " " << a->GetLabelName(it->first.type_) << " " << it->second;
      auto features_it = features_.find(it->first);
      if (features_it != features_.end()) {
        features_it->second.atomicAddRegularized(it->second, 0, regularizer_);
      }
    }
  }

  for (auto f_feature = factor_affected_features.begin(); f_feature != factor_affected_features.end(); f_feature++) {
    if (f_feature->second < -1e-9 || f_feature->second > 1e-9) {
      auto factor_feature = factor_features_.find(f_feature->first);
      if (factor_feature != factor_features_.end()) {
        factor_feature->second += f_feature->second;
        // L_inf regularize the new value.
        if (factor_feature->second < 0) factor_feature->second = 0;
        if (factor_feature->second > regularizer_) factor_feature->second = regularizer_;
      }
    }
  }
}

void GraphInference::PLLearn(
    const Nice2Query* query,
    const Nice2Assignment* assignment,
    double learning_rate) {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);

  // Perform gradient descent
  SimpleFeaturesMap affected_features;  // Gradient for each affected feature.
  affected_features.set_empty_key(GraphFeature(-1, -1, -1));
  affected_features.set_deleted_key(GraphFeature(-2, -2, -2));

  FactorFeaturesMap factor_affected_features;

  for (uint i = 0; i < a->assignments_.size(); i++) {
    if (a->assignments_[i].must_infer) {
      std::vector<int> candidates;
      a->GetLabelCandidates(*this, i, &candidates, beam_size_);

      // Compute estimated normalisation constant
      double normalization_constant = -a->GetNodePenalty(i);
      candidates.push_back(a->assignments_[i].label);
      for (const int label : candidates) {
        normalization_constant += exp(a->GetNodeScoreGivenAssignmentToANode(*this, i, i, label));
      }
      for (int label : candidates) {
        double marginal_probability = exp(a->GetNodeScoreGivenAssignmentToANode(*this, i, i, label)) / normalization_constant;
        a->GetNeighboringAffectedFeatures(&affected_features, i, label, -learning_rate * marginal_probability);
        a->GetFactorAffectedFeaturesOfNode(&factor_affected_features, i, label, -learning_rate * marginal_probability);
      }
    }
  }

  a->GetAffectedFeatures(&affected_features, beam_size_ * learning_rate);
  a->GetAffectedFactorFeatures(&factor_affected_features, beam_size_ * learning_rate);
  for (auto it = affected_features.begin(); it != affected_features.end(); ++it) {
    if (it->second < -1e-9 || it->second > 1e-9) {
      auto features_it = features_.find(it->first);
      if (features_it != features_.end()) {
        features_it->second.atomicAddRegularized(it->second, 0, regularizer_);
      }
    }
  }

  for (auto f_feature = factor_affected_features.begin(); f_feature != factor_affected_features.end(); f_feature++) {
    if (f_feature->second < -1e-9 || f_feature->second > 1e-9) {
      auto factor_feature = factor_features_.find(f_feature->first);
      if (factor_feature != factor_features_.end()) {
        factor_feature->second += f_feature->second;
        // L_inf regularize the new value.
        if (factor_feature->second < 0) factor_feature->second = 0;
        if (factor_feature->second > regularizer_) factor_feature->second = regularizer_;
      }
    }
  }
}

void GraphInference::DisplayGraph(
    const Nice2Query* query,
    const Nice2Assignment* assignment,
    Json::Value* graph) const {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);
  Json::Value& nodes = (*graph)["nodes"];
  for (size_t i = 0; i < a->assignments_.size(); ++i) {
    if (a->assignments_[i].must_infer ||
        !a->query_->arcs_adjacent_to_node_[i].empty()) {
      // Include the node.
      Json::Value node;
      node["id"] = Json::Value(StringPrintf("N%d", static_cast<int>(i)));
      int label = a->assignments_[i].label;
      node["label"] = Json::Value(label < 0 ? StringPrintf("%d", label).c_str() : a->GetLabelName(label));
      node["color"] = Json::Value(a->assignments_[i].must_infer ? "#6c9ba4" : "#96816a");
      nodes.append(node);
    }
  }
  std::unordered_map<IntPair, std::string> dedup_arcs;
  for (const Arc& arc : a->query_->arcs_) {
    std::string& s = dedup_arcs[IntPair(std::min(arc.node_a, arc.node_b),std::max(arc.node_a, arc.node_b))];
    if (!s.empty()) {
      s.append(", ");
    }
    StringAppendF(&s, "%s - %.2f",
        a->GetLabelName(arc.type),
        a->GetNodePairScore(*this, arc.node_a, arc.node_b, a->assignments_[arc.node_a].label, a->assignments_[arc.node_b].label));
  }

  Json::Value& edges = (*graph)["edges"];
  int edge_id = 0;
  for (auto it = dedup_arcs.begin(); it != dedup_arcs.end(); ++it) {
    Json::Value edge;
    edge["id"] = Json::Value(StringPrintf("Edge%d", edge_id));
    edge["label"] = Json::Value(it->second);
    edge["source"] = Json::Value(StringPrintf("N%d", static_cast<int>(it->first.first)));
    edge["target"] = Json::Value(StringPrintf("N%d", static_cast<int>(it->first.second)));
    edges.append(edge);
    edge_id++;
  }
}

void GraphInference::AddQueryToModel(const Json::Value& query, const Json::Value& assignment) {
  CHECK(query.isArray());
  CHECK(assignment.isArray());
  JsonValueNumberer numb;
  std::unordered_map<int, int> values;
  for (const Json::Value& a : assignment) {
    int value;
    if (a.isMember("inf")) {
      value = strings_.addString(a.get("inf", Json::Value::null).asCString());
    } else {
      CHECK(a.isMember("giv"));
      value = strings_.addString(a.get("giv", Json::Value::null).asCString());
    }
    values[numb.ValueToNumber(a.get("v", Json::Value::null))] = value;
  }

  for (const Json::Value& arc : query) {
    if (arc.isMember("f2")) {
      GraphFeature feature(
          FindWithDefault(values, numb.ValueToNumber(arc.get("a", Json::Value::null)), -1),
          FindWithDefault(values, numb.ValueToNumber(arc.get("b", Json::Value::null)), -1),
          strings_.addString(arc.get("f2", Json::Value::null).asCString()));
      if (feature.a_ != -1 && feature.b_ != -1) {
        features_[feature].nonAtomicAdd(1);
      }
    }

    if (arc.isMember("group")) {
      const Json::Value& v = arc["group"];
      if (v.isArray()) {
        Factor factor_vars;
        for (const Json::Value& item : v) {
          int value = FindWithDefault(values, numb.ValueToNumber(item), -1);
          if (value == -1) {
            factor_vars.clear();
            break;
          }
          factor_vars.insert(value);
        }
        if (factor_vars.empty()) {
          continue;
        }
        factor_features_[factor_vars] += 1;
      }
    }
  }
}

void GraphInference::PrepareForInference() {
  if (!label_checker_.IsLoaded()) {
    LOG(INFO) << "Loading LabelChecker...";
    label_checker_.Load(FLAGS_valid_labels, &strings_);
    LOG(INFO) << "LabelChecker loaded";
  }
  num_svm_training_samples_ = 0;

  best_features_for_type_.clear();
  best_features_for_a_type_.clear();
  best_features_for_b_type_.clear();
  best_factor_features_.clear();
  best_factor_features_first_level_.clear();

  for (auto it = features_.begin(); it != features_.end(); ++it) {
    const GraphFeature& f = it->first;
    double feature_weight = it->second.getValue();
    best_features_for_type_[f.type_].push_back(std::pair<double, GraphFeature>(feature_weight, f));
    best_features_for_a_type_[IntPair(f.a_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.b_));
    best_features_for_b_type_[IntPair(f.b_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.a_));
  }

  for (auto factor_feature = factor_features_.begin(); factor_feature != factor_features_.end(); factor_feature++) {
    Factor f = factor_feature->first;
    double feature_weight = factor_feature->second;
    Factor visited_labels;
    best_factor_features_first_level_[f.size()].InsertFactorFeature(feature_weight, f, 0, FLAGS_maximum_depth, -1, visited_labels);

    // Take out 1 by 1 each of the variables and then use the remainings as keys for the best features
    // each entry will have as key factors minus 1 variable and as value the pair with a weight and the corresponding variable that added to the
    // factor gives you that weight
    for (auto current_var = f.begin(); current_var != f.end(); current_var++) {
      Factor f_key;
      for (auto var = f.begin(); var != f.end(); var++) {
        if (*current_var != *var) {
          f_key.insert(*var);
        }
      }
      best_factor_features_[f_key].push_back(std::pair<double, int>(feature_weight, *current_var));
    }
  }


  LOG(INFO) << "Preparing GraphInference for MAP inference...";
  for (auto it = best_features_for_type_.begin(); it != best_features_for_type_.end(); ++it) {
    std::sort(it->second.begin(), it->second.end(), std::greater<std::pair<double, GraphFeature> >());
  }
  for (auto it = best_features_for_a_type_.begin(); it != best_features_for_a_type_.end(); ++it) {
    std::sort(it->second.begin(), it->second.end(), std::greater<std::pair<double, int> >());
  }
  for (auto it = best_features_for_b_type_.begin(); it != best_features_for_b_type_.end(); ++it) {
    std::sort(it->second.begin(), it->second.end(), std::greater<std::pair<double, int> >());
  }
  for (auto it = best_factor_features_.begin(); it != best_factor_features_.end(); it++) {
    std::sort(it->second.begin(), it->second.end(), std::greater<std::pair<double, int> >());
  }
  for (auto it = best_factor_features_first_level_.begin(); it != best_factor_features_first_level_.end(); it++) {
    it->second.SortFactorFeatures();
  }
  LOG(INFO) << "GraphInference prepared for MAP inference.";
}

void GraphInference::PrintDebugInfo() {
  NBest<int, double> best_connected_labels;
  std::unordered_map<int, NBest<int, double> > best_connections_per_label;
  std::unordered_map<IntPair, NBest<int, double> > best_connections_per_label_type;
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    double score = it->second.getValue();
    best_connected_labels.AddScoreToItem(it->first.a_, score);
    best_connected_labels.AddScoreToItem(it->first.b_, score);
    best_connections_per_label[it->first.a_].AddScoreToItem(it->first.type_, score);
    best_connections_per_label[it->first.b_].AddScoreToItem(it->first.type_, score);
    best_connections_per_label_type[IntPair(it->first.a_, it->first.type_)].AddScoreToItem(it->first.b_, score);
    best_connections_per_label_type[IntPair(it->first.b_, it->first.type_)].AddScoreToItem(it->first.a_, score);
  }

  printf("Best connected labels\n");
  for (auto v : best_connected_labels.produce_nbest(96)) {
    printf("%.3f : %12s :\n", v.first, v.second < 0 ? "-1" : strings_.getString(v.second));
    for (auto vv : best_connections_per_label[v.second].produce_nbest(3)) {
      printf("         (%5.3f) %40s : ", vv.first, vv.second < 0 ? "-1" : strings_.getString(vv.second));
      for (auto vvv : best_connections_per_label_type[IntPair(v.second, vv.second)].produce_nbest(3)) {
        printf(" %20s (%.3f) ", vvv.second < 0 ? "-1" : strings_.getString(vvv.second), vvv.first);
      }
      printf("\n");
    }
    printf("\n");
  }
}

void GraphInference::PrintConfusionStatistics(
    const Nice2Query* query,
    const Nice2Assignment* assignment,
    NodeConfusionStats* stats) {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);
  const GraphQuery* q = a->query_;

  std::map<std::vector<Arc>, std::vector<int> > nodes_per_confusion;

  for (int node_id = 0; node_id < static_cast<int>(q->arcs_adjacent_to_node_.size()); ++node_id) {
    if (!a->assignments_[node_id].must_infer) continue;
    std::vector<Arc> arcs(q->arcs_adjacent_to_node_[node_id]);
    for (size_t i = 0; i < arcs.size(); ++i) {
      if (arcs[i].node_a == node_id) arcs[i].node_a = -1;
      if (arcs[i].node_b == node_id) arcs[i].node_b = -1;
    }
    std::sort(arcs.begin(), arcs.end());
    nodes_per_confusion[arcs].push_back(node_id);
  }

  for (auto it = nodes_per_confusion.begin(); it != nodes_per_confusion.end(); ++it) {
    if (it->second.size() <= 1) {
      CHECK_EQ(it->second.size(), 1);
      ++stats->num_non_confusable_nodes;
      continue;
    }

    stats->num_confusable_nodes += it->second.size();
    stats->num_expected_confusions += it->second.size() - 1;

    const auto& arcs = it->first;
    const auto& nodes = it->second;

    std::string labels;
    for (size_t i = 0; i < nodes.size(); ++i) {
      int label = a->assignments_[nodes[i]].label;
      if (!labels.empty()) labels.append(" ");
      labels.append(a->GetLabelName(label));
    }
    std::string predicted_by;
    for (size_t i = 0; i < arcs.size(); ++i) {
      if (!predicted_by.empty()) predicted_by.append(", ");
      const char* label_a = (arcs[i].node_a == -1) ? ("<X>") : a->GetLabelName(a->assignments_[arcs[i].node_a].label);
      const char* label_b = (arcs[i].node_b == -1) ? ("<X>") : a->GetLabelName(a->assignments_[arcs[i].node_b].label);
      const char* arc = a->label_set_->ss()->getString(arcs[i].type);
      StringAppendF(&predicted_by, "%s[%s %s]", arc, label_a, label_b);
    }
    if (predicted_by.empty()) predicted_by = "<no adjacent edges>";

    LOG(INFO) << "Confusion:\nLabels:      " << labels << "\nPredicted by: " << predicted_by << "\n\n";
  }
}


