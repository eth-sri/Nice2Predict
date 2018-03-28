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

#include <stdio.h>
#include <math.h>
#include <queue>
#include <unordered_set>
#include <algorithm>
#include <iterator>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "base/base.h"
#include "base/maputil.h"
#include "base/nbest.h"
#include "base/simple_histogram.h"
#include "base/updatable_priority_queue.h"

#include "graph_inference.h"
#include "label_set.h"

using nice2protos::Feature;
using nice2protos::InferResponse;
using nice2protos::NBestResponse;
using nice2protos::ShowGraphResponse;


DEFINE_bool(initial_greedy_assignment_pass, true, "Whether to run an initial greedy assignment pass.");
DEFINE_bool(duplicate_name_resolution, true, "Whether to attempt a duplicate name resultion on conflicts.");
DEFINE_int32(graph_per_node_passes, 8, "Number of per-node passes for inference");
DEFINE_int32(graph_per_arc_passes, 5, "Number of per-arc passes for inference");
DEFINE_int32(graph_per_factor_passes, 1, "Number of per-factor passes for inference");
DEFINE_int32(graph_loopy_bp_passes, 0, "Number of loopy belief propagation passes for inference");
DEFINE_int32(graph_loopy_bp_steps_per_pass, 3, "Number of loopy belief propagation steps in each inference pass");
DEFINE_int32(skip_per_arc_optimization_for_nodes_above_degree, 32,
    "Skip the per-arc optimization pass if an edge is connected to a node with the in+out degree more than the given value");

DEFINE_bool(use_factors, true, "Flag that enable the use of the factors in training and MAP inference.");
DEFINE_int32(maximum_depth, 2, "Maximum depth of the multi-level map used to store the factor features");
DEFINE_int32(factors_limit, 128, "Maximum number of factor candidates considered for inference using factor features");
DEFINE_uint64(permutations_beam_size, 64, "Maximum number of permutations of the assignments to unknown labels in a factor");

DEFINE_string(valid_labels, "valid_names.txt", "A file describing valid names");
DEFINE_string(unknown_label,
    "", "A special label that denotes that a label is of low frequency (and thus unknown).");
DEFINE_int32(min_freq_known_label, 0,
    "Minimum number of graphs a label must appear it in order to not be declared unknown");

static const size_t kInitialAssignmentBeamSize = 4;

static const size_t kStartPerArcBeamSize = 4;
static const size_t kMaxPerArcBeamSize = 64;

static const size_t kStartPerNodeBeamSize = 4;
static const size_t kMaxPerNodeBeamSize = 64;
static const size_t kLoopyBPBeamSize = 32;

static const size_t kFactorsLimitBeforeGoingDepperMultiLevelMap = 16;

static const size_t MAX_NAME_LEN = 1024;

// Returns -1 if the result will overflow.
uint64 CalculateFactorial(int n) {
  uint64 result = 1;
  for (size_t i = n; i > 0; i--) {
    // Check if result will overflow.
    if (result > (UINT64_MAX / i)) {
      return -1;
    }
    result *= i;
  }
  return result;
}

// http://stackoverflow.com/a/6867612
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
uint64 HashInt(uint64 x) {
  x++;
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccd;
  x ^= x >> 33;
  x *= 0xc4ceb9fe1a85ec53;
  x ^= x >> 33;
  return x;
}


class GraphQuery : public Nice2Query {
public:
  explicit GraphQuery(const StringSet* ss, const LabelChecker* checker) : label_set_(ss, checker) {
    arcs_connecting_node_pair_.set_empty_key(IntPair(-1, -1));
    arcs_connecting_node_pair_.set_deleted_key(IntPair(-2, -2));
  }
  virtual ~GraphQuery() {
  }

  virtual void FromFeaturesQueryProto(const FeaturesQuery &query) override {
    arcs_.clear();
    factors_.clear();

    int max_index = 0;
    for (const Feature& feature : query) {
      if (feature.has_binary_relation()) {
        // A factor connecting two facts (an arc).
        Arc a;
        a.node_a = feature.binary_relation().first_node();
        a.node_b = feature.binary_relation().second_node();
        max_index = std::max({max_index, a.node_a, a.node_b});
        a.type = label_set_.ss()->findString(feature.binary_relation().relation().c_str());
        if (a.type < 0) continue;
        arcs_.push_back(a);
      }
      else if (feature.has_constraint() && !feature.constraint().nodes().empty()) {
        // A scope that lists names that cannot be assigned to the same value.
        std::vector<int> scope_vars;
        scope_vars.reserve(feature.constraint().nodes_size());
        for (const auto& item : feature.constraint().nodes()) {
          scope_vars.push_back(item);
        }
        std::sort(scope_vars.begin(), scope_vars.end());
        max_index = std::max(max_index, scope_vars.back());
        scope_vars.erase(std::unique(scope_vars.begin(), scope_vars.end()), scope_vars.end());
        nodes_in_scope_.push_back(std::move(scope_vars));
      }
      if (FLAGS_use_factors && feature.has_factor_variables()) {
        Factor factor_vars;
        for (const auto& item : feature.factor_variables().nodes()) {
          factor_vars.insert(item);
          max_index = std::max(max_index, item);
        }
        factors_.push_back(std::move(factor_vars));
      }
    }
    std::sort(arcs_.begin(), arcs_.end());

    arcs_adjacent_to_node_.assign(max_index + 1, std::vector<Arc>());
    for (const Arc& a : arcs_) {
      arcs_adjacent_to_node_[a.node_a].push_back(a);
      arcs_adjacent_to_node_[a.node_b].push_back(a);
    }
    for (std::vector<Arc>& v : arcs_adjacent_to_node_) {
      std::sort(v.begin(), v.end());
      v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    arcs_connecting_node_pair_.clear();
    for (const Arc& a : arcs_) {
      arcs_connecting_node_pair_[IntPair(a.node_a, a.node_b)].push_back(a);
      arcs_connecting_node_pair_[IntPair(a.node_b, a.node_a)].push_back(a);
    }

    scopes_per_nodes_.assign(max_index + 1, std::vector<int>());
    for (size_t scope = 0; scope < nodes_in_scope_.size(); ++scope) {
      for (int node : nodes_in_scope_[scope]) {
        scopes_per_nodes_[node].push_back(scope);
      }
    }

    factors_of_a_node_.assign(max_index + 1, std::vector<int>());
    for (size_t i = 0; i < factors_.size(); ++i) {
      for (auto var = factors_[i].begin(); var != factors_[i].end(); ++var) {
        factors_of_a_node_[*var].push_back(i);
      }
    }

  }

private:
  struct Arc {
    int node_a, node_b, type;
    bool operator==(const Arc& o) const {
      return node_a == o.node_a && node_b == o.node_b && type == o.type;
    }
    bool operator<(const Arc& o) const {
      if (node_a != o.node_a) return node_a < o.node_a;
      if (node_b != o.node_b) return node_b < o.node_b;
      return type < o.type;
    }
  };

  std::vector<std::vector<Arc> > arcs_adjacent_to_node_;
  std::vector<std::vector<int> > factors_of_a_node_;
  std::vector<Arc> arcs_;
  std::vector<Factor> factors_;
  google::dense_hash_map<IntPair, std::vector<Arc> > arcs_connecting_node_pair_;

  LabelSet label_set_;

  std::vector<std::vector<int> > nodes_in_scope_;
  std::vector<std::vector<int> > scopes_per_nodes_;

  friend class GraphNodeAssignment;
  friend class LoopyBPInference;
  friend class GraphInference;
};

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


class GraphNodeAssignment : public Nice2Assignment {
public:
  GraphNodeAssignment(const GraphQuery* query, LabelSet* label_set, int unknown_label)
    : query_(query), label_set_(label_set), unknown_label_(unknown_label) {
  }
  virtual ~GraphNodeAssignment() {
  }

  virtual void SetUpEqualityPenalty(double penalty) override {
    ClearPenalty();
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].must_infer) {
        penalties_[i].label = assignments_[i].label;
        penalties_[i].penalty = penalty;
      }
    }
  }

  virtual void ClearPenalty() override {
    penalties_.assign(assignments_.size(), LabelPenalty());
  }

  virtual void FromNodeAssignmentsProto(const NodeAssignments &assignments) override {
    size_t variables_count = query_->arcs_adjacent_to_node_.size();
    assignments_.assign(variables_count, Assignment());
    for (const auto& assignment : assignments) {
      Assignment aset;
      aset.label = label_set_->AddLabelName(assignment.label().substr(0, MAX_NAME_LEN).c_str());
      aset.must_infer = !assignment.given();
      if (assignment.node_index() < variables_count) {
        assignments_[assignment.node_index()] = aset;
      }
    }
    ClearPenalty();
  }

  virtual void FillInferResponse(InferResponse* response) const override {
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].label < 0) continue;

      auto *assignment = response->add_node_assignments();
      assignment->set_node_index(i);
      assignment->set_given(!assignments_[i].must_infer);
      assignment->set_label(label_set_->GetLabelName(assignments_[i].label));
    }
  }

  void GetCandidatesForNode(
      Nice2Inference* inference, 
      const int node, 
      std::vector<std::pair<int, double>>* scored_candidates) {
    GraphInference* graphInference = static_cast<GraphInference*>(inference);
    std::vector<int> candidates;
    GetLabelCandidates(*graphInference, node, &candidates, kMaxPerArcBeamSize);

    scored_candidates->clear();
    Assignment& nodea = assignments_[node];
    int original_label = nodea.label;
    for (size_t i = 0; i < candidates.size() ; i++) {
      int candidate = candidates[i];
      nodea.label = candidate;
      if (!graphInference->label_checker_.IsLabelValid(candidate)) continue;
      double score = GetNodeScore(*graphInference, node);
      scored_candidates->push_back(std::pair<int, double>(candidate, score));
    }
    nodea.label = original_label;

    std::sort(scored_candidates->begin(), scored_candidates->end(), [](const std::pair<int,double> &left, const std::pair<int,double> &right) {
      return right.second < left.second;
    });
  }
 

  virtual void GetNBestCandidates(
      Nice2Inference* inference,
      const int n,
      nice2protos::NBestResponse* response) override {
    std::vector<std::pair<int, double>> scored_candidates;
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].must_infer) {
        GetCandidatesForNode(inference, i, &scored_candidates);
        auto *distribution = response->add_candidates_distributions();
        distribution->set_node(i);
        // Take only the top-n candidates to the response
        for (size_t j = 0; j < scored_candidates.size() && j < (size_t)((unsigned)n) ; j++) {
          auto *candidate = distribution->add_candidates();
          auto *assignment = new nice2protos::NodeAssignment();
          assignment->set_label(label_set_->GetLabelName(scored_candidates[j].first));
          assignment->set_node_index(i);
          assignment->set_given(false);

          candidate->set_allocated_node_assignment(assignment);
          candidate->set_score(scored_candidates[j].second);
        }
      }
    }
  }

  virtual void ClearInferredAssignment() override {
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].must_infer) {
        assignments_[i].label = -1;
      }
    }
  }

  virtual void CompareAssignments(const Nice2Assignment* reference, PrecisionStats* stats) const override {
    const GraphNodeAssignment* ref = static_cast<const GraphNodeAssignment*>(reference);
    int correct_labels = 0;
    int incorrect_labels = 0;
    int num_known_predictions = 0;
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].must_infer) {
        if (assignments_[i].label != unknown_label_) {
          ++num_known_predictions;
        }
        if (assignments_[i].label == ref->assignments_[i].label &&
            assignments_[i].label != unknown_label_) {
          ++correct_labels;
        } else {
          ++incorrect_labels;
        }
      }
    }
    std::lock_guard<std::mutex> guard(stats->lock);
    stats->correct_labels += correct_labels;
    stats->incorrect_labels += incorrect_labels;
    stats->num_known_predictions += num_known_predictions;
  }

  virtual void CompareAssignmentErrors(const Nice2Assignment* reference, SingleLabelErrorStats* error_stats) const override {
    const GraphNodeAssignment* ref = static_cast<const GraphNodeAssignment*>(reference);
    std::lock_guard<std::mutex> guard(error_stats->lock);
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].must_infer) {
        if (assignments_[i].label != ref->assignments_[i].label) {
          error_stats->errors_and_counts[StringPrintf(
              "%s -> %s",
              ref->assignments_[i].label == -1 ? "[none]" : label_set_->GetLabelName(ref->assignments_[i].label),
              assignments_[i].label == -1 ? "[keep-original]" : label_set_->GetLabelName(assignments_[i].label))]++;
        }
      }
    }
  }

  std::string DebugString() const {
    std::string result;
    for (int node = 0; node < static_cast<int>(assignments_.size()); ++node) {
      StringAppendF(&result, "[%d:%s]%s ", node, label_set_->GetLabelName(assignments_[node].label), assignments_[node].must_infer ? "" : "*");
    }
    return result;
  }

  const char* GetLabelName(int label_id) const {
    return label_set_->GetLabelName(label_id);
  }

  // Returns the penalty associated with a node and its label (used in Max-Margin training).
  double GetNodePenalty(int node) const {
    return (assignments_[node].label == penalties_[node].label) ? penalties_[node].penalty : 0.0;
  }
  // Gets the score contributed by all arcs adjacent to a node.
  double GetNodeScore(const GraphInference& fweights, int node) const {
    double sum = -GetNodePenalty(node);
    const GraphInference::FeaturesMap& features = fweights.features_;
    const GraphInference::Uint64FactorFeaturesMap& factor_features = fweights.factor_features_;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      GraphFeature feature(
          assignments_[arc.node_a].label,
          assignments_[arc.node_b].label,
          arc.type);
      auto feature_it = features.find(feature);
      if (feature_it != features.end()) {
        sum += feature_it->second.getValue();
      }
    }

    const std::vector<int>& factors_of_a_node = query_->factors_of_a_node_[node];
    for (size_t i = 0; i < factors_of_a_node.size(); ++i) {
      uint64 hash = 0;
      const Factor& factor = query_->factors_[factors_of_a_node[i]];
      for (auto var_it = factor.begin();
                var_it != factor.end(); ++var_it) {
        hash += HashInt(assignments_[*var_it].label);
      }

      auto factor_feature = factor_features.find(hash);
      if (factor_feature != factor_features.end()) {
        sum += factor_feature->second;
      }
    }
    return sum;
  }

  // Gets the node score given an assignment
  double GetNodeScoreGivenAssignmentToANode(const GraphInference& fweights, int node, int node_assigned, int node_assignment) const {
    double sum = -GetNodePenalty(node);
    const GraphInference::FeaturesMap& features = fweights.features_;
    const GraphInference::Uint64FactorFeaturesMap& factor_features = fweights.factor_features_;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      int node_a_label;
      int node_b_label;
      if (arc.node_a == node_assigned) {
        node_a_label = node_assignment;
      } else {
        node_a_label = assignments_[arc.node_a].label;
      }
      if (arc.node_b == node_assigned) {
        node_b_label = node_assignment;
      } else {
        node_b_label = assignments_[arc.node_b].label;
      }
      GraphFeature feature(
          node_a_label,
          node_b_label,
          arc.type);
      auto feature_it = features.find(feature);
      if (feature_it != features.end()) {
        sum += feature_it->second.getValue();
      }
    }

    int node_label = assignments_[node].label;
    if (node == node_assigned) {
      node_label = node_assignment;
    }
    const std::vector<int>& factors_of_a_node = query_->factors_of_a_node_[node];
    for (size_t i = 0; i < factors_of_a_node.size(); ++i) {
      uint64 hash = HashInt(node_label);
      const Factor& factor = query_->factors_[factors_of_a_node[i]];
      for (auto var = factor.begin();
                var != factor.end(); ++var) {
        if (*var != node) {
          hash += HashInt(assignments_[*var].label);
        }
      }
      auto factor_feature = factor_features.find(hash);
      if (factor_feature != factor_features.end()) {
        sum += factor_feature->second;
      }
    }

    return sum;
  }

  double GetNodeScoreOnAssignedNodes(
      const GraphInference& fweights, int node,
      const std::vector<bool>& assigned) const {
    double sum = -GetNodePenalty(node);
    const GraphInference::FeaturesMap& features = fweights.features_;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a != node && !assigned[arc.node_a]) continue;
      if (arc.node_b != node && !assigned[arc.node_b]) continue;
      GraphFeature feature(
          assignments_[arc.node_a].label,
          assignments_[arc.node_b].label,
          arc.type);
      auto feature_it = features.find(feature);
      if (feature_it != features.end()) {
        sum += feature_it->second.getValue();
      }
    }
    return sum;
  }

  bool HasDuplicationConflictsAtNode(int node) const {
    int node_label = assignments_[node].label;
    if (node_label == unknown_label_) return false;
    const std::vector<int>& scopes = query_->scopes_per_nodes_[node];
    for (int scope : scopes) {
      const std::vector<int>& nodes_per_scope = query_->nodes_in_scope_[scope];
      for (int other_node : nodes_per_scope) {
        if (other_node != node && assignments_[other_node].label == node_label)
          return true;
      }
    }
    return false;
  }

  // Returns the node with a duplication conflict to the current node. Returns -1 if there is no such node or there are multiple such nodes.
  int GetNodeWithDuplicationConflict(int node) const {
    int conflict_node = -1;
    int node_label = assignments_[node].label;
    const std::vector<int>& scopes = query_->scopes_per_nodes_[node];
    for (int scope : scopes) {
      const std::vector<int>& nodes_per_scope = query_->nodes_in_scope_[scope];
      for (int other_node : nodes_per_scope) {
        if (other_node != node && assignments_[other_node].label == node_label) {
          if (conflict_node == -1) {
            conflict_node = other_node;  // We have found a conflict node.
          } else {
            if (conflict_node != other_node) return -1;  // There are multiple conflict nodes.
          }
        }
      }
    }
    return conflict_node;
  }

  // Gets the score connecting a pair of nodes.
  double GetNodePairScore(const GraphInference& fweights, int node1, int node2, int label1, int label2) const {
    double sum = 0;
    const GraphInference::FeaturesMap& features = fweights.features_;
    for (const GraphQuery::Arc& arc : FindWithDefault(query_->arcs_connecting_node_pair_, IntPair(node1, node2), std::vector<GraphQuery::Arc>())) {
      auto feature_it = features.find(
          arc.node_a == node1 ?
              GraphFeature(label1, label2, arc.type) :
              GraphFeature(label2, label1, arc.type));
      if (feature_it != features.end()) {
        sum += feature_it->second.getValue();
      }
    }
    return sum;
  }

  int GetNumAdjacentArcs(int node) const {
    return query_->arcs_adjacent_to_node_[node].size();
  }

  void GetFactorCandidates(const GraphInference& fweights,
      const int factor_size,
      std::vector<Factor>* candidates,
      const Factor& giv_labels,
      const size_t beam_size) {
    FactorFeaturesLevel empty_level;
    const FactorFeaturesLevel& v = FindWithDefault(fweights.best_factor_features_first_level_, factor_size, empty_level);
    auto it = giv_labels.begin();
    v.GetFactors(giv_labels, *it, candidates, beam_size);
  }

  void GetLabelCandidates(const GraphInference& fweights, int node,
      std::vector<int>* candidates, size_t beam_size) const {
    std::vector<std::pair<double, int> > empty_vec;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a == node) {
        const std::vector<std::pair<double, int> >& v =
            FindWithDefault(fweights.best_features_for_b_type_,
                IntPair(assignments_[arc.node_b].label, arc.type), empty_vec);
        for (size_t i = 0; i < v.size() && i < beam_size; ++i) {
          candidates->push_back(v[i].second);
        }
      }
      if (arc.node_b == node) {
        const std::vector<std::pair<double, int> >& v =
            FindWithDefault(fweights.best_features_for_a_type_,
                IntPair(assignments_[arc.node_a].label, arc.type), empty_vec);
        for (size_t i = 0; i < v.size() && i < beam_size; ++i) {
          candidates->push_back(v[i].second);
        }
      }
    }

#ifdef GRAPH_INFERENCE_STATS
    stats_.label_candidates_per_node.AddCount(candidates->size(), 1);
#endif
    std::sort(candidates->begin(), candidates->end());
    candidates->erase(std::unique(candidates->begin(), candidates->end()), candidates->end());
  }

  void ReplaceLabelsWithUnknown(const GraphInference& fweights) {
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (fweights.label_frequency_.find(assignments_[i].label) == fweights.label_frequency_.end()) {
        assignments_[i].label = unknown_label_;
      }
    }
  }

  double GetTotalScore(const GraphInference& fweights) const {
    double sum = 0;
    const GraphInference::FeaturesMap& features = fweights.features_;
    for (const GraphQuery::Arc& arc : query_->arcs_) {
      GraphFeature feature(
          assignments_[arc.node_a].label,
          assignments_[arc.node_b].label,
          arc.type);
      auto feature_it = features.find(feature);
      if (feature_it != features.end()) {
        sum += feature_it->second.getValue();
      }
      VLOG(3) << " " << label_set_->GetLabelName(feature.a_) << " " << label_set_->GetLabelName(feature.b_) << " " << label_set_->GetLabelName(feature.type_)
          << " " << ((feature_it != features.end()) ? feature_it->second.getValue() : 0.0);
    }
    for (size_t i = 0; i < assignments_.size(); ++i) {
      sum -= GetNodePenalty(i);
    }
    VLOG(3) << "=" << sum;
    return sum;
  }

  void GetAffectedFeatures(
      GraphInference::SimpleFeaturesMap* affected_features,
      double gradient_weight) const {
    for (const GraphQuery::Arc& arc : query_->arcs_) {
      GraphFeature feature(
          assignments_[arc.node_a].label,
          assignments_[arc.node_b].label,
          arc.type);
      (*affected_features)[feature] += gradient_weight;
    }
  }

  void GetAffectedFactorFeatures(
      GraphInference::Uint64FactorFeaturesMap* affected_factor_features,
      double gradient_weight) const {
    for (const Factor& factor : query_->factors_) {
      uint64 hash = 0;
      for (auto var = factor.begin(); var != factor.end(); ++var) {
        hash += HashInt(assignments_[*var].label);
      }
      (*affected_factor_features)[hash] += gradient_weight;
    }
  }

  // Method that given a certain node and a label, first assign that label to the given node and then add the gradient_weight
  // to every affected feature related to the given node and its neighbours
  void GetNeighboringAffectedFeatures(
      GraphInference::SimpleFeaturesMap* affected_features,
      int node,
      int label,
      double gradient_weight) const {
    for (const auto & arc : query_->arcs_adjacent_to_node_[node]) {
      int label_node_a = assignments_[arc.node_a].label;
      int label_node_b = assignments_[arc.node_b].label;
      if (arc.node_a == node) {
        label_node_a = label;
      }
      if (arc.node_b == node) {
        label_node_b = label;
      }
      GraphFeature feature(
          label_node_a,
          label_node_b,
          arc.type);
      (*affected_features)[feature] += gradient_weight;
    }
  }

  void GetFactorAffectedFeaturesOfNode(
      GraphInference::Uint64FactorFeaturesMap* factor_affected_features,
      int node,
      int label,
      double gradient_weight) const {
    int node_label = label;
    const std::vector<int>& factors_of_a_node = query_->factors_of_a_node_[node];
    for (size_t i = 0; i < factors_of_a_node.size(); ++i) {
      uint64 hash = HashInt(node_label);
      const Factor& f = query_->factors_[factors_of_a_node[i]];
      for (auto var = f.begin(); var != f.end(); ++var) {
        if (*var != node) {
          hash += HashInt(assignments_[(*var)].label);
        }
      }
      (*factor_affected_features)[hash] += gradient_weight;
    }
  }

  void InitialGreedyAssignmentPass(const GraphInference& fweights) {
    std::vector<bool> assigned(assignments_.size(), false);
    for (size_t node = 0; node < assignments_.size(); ++node) {
      assigned[node] = !assignments_[node].must_infer;
    }
    UpdatablePriorityQueue<int, int> p_queue;
    for (size_t node = 0; node < assignments_.size(); ++node) {
      if (assignments_[node].must_infer) {
        int score = 0;
        for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
          if (assigned[arc.node_a] || assigned[arc.node_b]) ++score;
        }
        p_queue.SetValue(node, -score);
      }
    }

    std::vector<int> candidates;
    while (!p_queue.IsEmpty()) {
      int node = p_queue.GetKeyWithMinValue();
      p_queue.PermanentlyRemoveKeyFromQueue(node);
      for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
        if (arc.node_a == node) {
          p_queue.SetValue(arc.node_b, p_queue.GetValue(arc.node_b) - 1);
        } else if (arc.node_b == node) {
          p_queue.SetValue(arc.node_a, p_queue.GetValue(arc.node_a) - 1);
        }
      }

      GraphNodeAssignment::Assignment& nodea = assignments_[node];
      if (!nodea.must_infer) continue;
      candidates.clear();
      GetLabelCandidates(fweights, node, &candidates, kInitialAssignmentBeamSize);
      if (candidates.empty()) continue;
      double best_score = GetNodeScoreOnAssignedNodes(fweights, node, assigned);
      int best_label = nodea.label;
      for (size_t i = 0; i < candidates.size(); ++i) {
        nodea.label = candidates[i];
        if (!fweights.label_checker_.IsLabelValid(assignments_[node].label)) continue;
        if (HasDuplicationConflictsAtNode(node)) continue;
        double score = GetNodeScoreOnAssignedNodes(fweights, node, assigned);
        if (score > best_score) {
          best_label = nodea.label;
          best_score = score;
        }
      }
      nodea.label = best_label;
      assigned[node] = true;
    }

  }

  void LocalPerNodeOptimizationPass(const GraphInference& fweights, size_t beam_size) {
    std::vector<int> candidates;
    for (size_t node = 0; node < assignments_.size(); ++node) {
      GraphNodeAssignment::Assignment& nodea = assignments_[node];
      if (!nodea.must_infer) continue;
      candidates.clear();
      GetLabelCandidates(fweights, node, &candidates, beam_size);
      if (candidates.empty()) continue;
      double best_score = GetNodeScore(fweights, node);
      int best_label = nodea.label;
#ifdef GRAPH_INFERENCE_STATS
      int best_position = -1;
#endif
      for (size_t i = 0; i < candidates.size(); ++i) {
        nodea.label = candidates[i];
        if (!fweights.label_checker_.IsLabelValid(assignments_[node].label)) continue;
        if (HasDuplicationConflictsAtNode(node)) continue;
        double score = GetNodeScore(fweights, node);
        if (score > best_score) {
          best_label = nodea.label;
          best_score = score;
#ifdef GRAPH_INFERENCE_STATS
          best_position = i;
#endif
        }
      }
#ifdef GRAPH_INFERENCE_STATS
      stats_.position_of_best_per_node_label.AddCount(best_position + 1, 1);
#endif
      nodea.label = best_label;
    }
  }

  void LocalPerNodeOptimizationPassWithDuplicateNameResolution(const GraphInference& fweights, size_t beam_size) {
    std::vector<int> candidates;
    for (size_t node = 0; node < assignments_.size(); ++node) {
      GraphNodeAssignment::Assignment& nodea = assignments_[node];
      if (!nodea.must_infer) continue;
      candidates.clear();
      GetLabelCandidates(fweights, node, &candidates, beam_size);
      if (candidates.empty()) continue;
      double best_score = GetNodeScore(fweights, node);
      int initial_label = nodea.label;
      int best_label = initial_label;
      int best_node2 = -1;
      for (size_t i = 0; i < candidates.size(); ++i) {
        nodea.label = candidates[i];
        if (!fweights.label_checker_.IsLabelValid(assignments_[node].label)) continue;
        if (HasDuplicationConflictsAtNode(node)) {
          int node2 = GetNodeWithDuplicationConflict(node);
          if (node2 == -1 || assignments_[node2].must_infer == false) continue;
          assignments_[node2].label = initial_label;  // Set label to node2.
          double score = GetNodeScore(fweights, node) + GetNodeScore(fweights, node2);
          bool correct = !HasDuplicationConflictsAtNode(node2) && !HasDuplicationConflictsAtNode(node);
          assignments_[node2].label = candidates[i];  // Revert label of node2.
          if (correct) {
            score -= GetNodeScore(fweights, node2);  // The score on node2 is essentially the gain of the score on node2.
            if (score > best_score) {
              best_label = nodea.label;
              best_score = score;
              best_node2 = node2;
            }
          }
        } else {
          double score = GetNodeScore(fweights, node);
          if (score > best_score) {
            best_label = nodea.label;
            best_score = score;
            best_node2 = -1;
          }
        }
      }
      nodea.label = best_label;
      if (best_node2 != -1)
        assignments_[best_node2].label = initial_label;
    }
  }

  void LocalPerArcOptimizationPass(const GraphInference& fweights, size_t beam_size) {
    std::vector<std::pair<double, GraphFeature> > empty;
    for (const GraphQuery::Arc& arc : query_->arcs_) {
      if (arc.node_a == arc.node_b) continue;
      if (assignments_[arc.node_a].must_infer == false || assignments_[arc.node_b].must_infer == false) continue;
      if (static_cast<int>(query_->arcs_adjacent_to_node_[arc.node_a].size()) >
              FLAGS_skip_per_arc_optimization_for_nodes_above_degree) continue;
      if (static_cast<int>(query_->arcs_adjacent_to_node_[arc.node_b].size()) >
              FLAGS_skip_per_arc_optimization_for_nodes_above_degree) continue;

      // Get candidate labels for labels of node_a and node_b.
      const std::vector<std::pair<double, GraphFeature> >& candidates =
          FindWithDefault(fweights.best_features_for_type_, arc.type, empty);
      if (candidates.empty()) continue;

      // Iterate over all candidate labels to see if some of them improves the score over the current labels.
      int best_a = assignments_[arc.node_a].label;
      int best_b = assignments_[arc.node_b].label;
      double best_score = GetNodeScore(fweights, arc.node_a) + GetNodeScore(fweights, arc.node_b);
#ifdef GRAPH_INFERENCE_STATS
      int best_position = -1;
#endif
      for (size_t i = 0; i < candidates.size() && i < beam_size; ++i) {
        assignments_[arc.node_a].label = candidates[i].second.a_;
        assignments_[arc.node_b].label = candidates[i].second.b_;
        if (HasDuplicationConflictsAtNode(arc.node_a) ||
            HasDuplicationConflictsAtNode(arc.node_b)) continue;
        if (!fweights.label_checker_.IsLabelValid(assignments_[arc.node_a].label)) continue;
        if (!fweights.label_checker_.IsLabelValid(assignments_[arc.node_b].label)) continue;
        double score = GetNodeScore(fweights, arc.node_a) + GetNodeScore(fweights, arc.node_b);
        if (score > best_score) {
          best_a = assignments_[arc.node_a].label;
          best_b = assignments_[arc.node_b].label;
          best_score = score;
#ifdef GRAPH_INFERENCE_STATS
          best_position = i;
#endif
        }
      }
#ifdef GRAPH_INFERENCE_STATS
      stats_.position_of_best_per_arc_label.AddCount(best_position + 1, 1);
#endif
      assignments_[arc.node_a].label = best_a;
      assignments_[arc.node_b].label = best_b;
    }
  }

  // Perform optimization based on factor features
  void LocalPerFactorOptimizationPass(const GraphInference& fweights, size_t beam_size) {
    std::vector<std::pair<double, Factor>> empty;
    for (const Factor& factor : query_->factors_) {
      std::vector<int> inf_nodes;
      inf_nodes.reserve(factor.size());
      Factor giv_labels;
      // Separates between given and to be inferred.
      for (auto var = factor.begin(); var != factor.end(); ++var) {
        Assignment a = assignments_[(*var)];
        if (a.must_infer == true) {
          inf_nodes.push_back((*var));
        } else {
          giv_labels.insert(a.label);
        }
      }

      std::vector<Factor> factors;
      GetFactorCandidates(fweights, factor.size(), &factors, giv_labels, beam_size);
      double best_score = 0;
      std::vector<int> best_assignments(inf_nodes.size());
      // Initialize the best score and best assignments with the current assignments on the to be inferred nodes
      // of the factor.
      for (size_t j = 0; j < inf_nodes.size(); ++j) {
        best_score += GetNodeScore(fweights, inf_nodes[j]);
        best_assignments[j] = assignments_[inf_nodes[j]].label;
      }
      std::vector<Factor> factors_candidates;
      // Determines which of the factors match the given labels.
      for (size_t j = 0; j < factors.size(); ++j) {
        bool factor_matches_giv_vars = true;
        for (auto label = giv_labels.begin(); label != giv_labels.end(); ++label) {
          // Each given label contained in giv_labels needs to be present in the factor candidate at least the same number of times that it is contained
          // in the given labels set.
          if (factors[j].count(*label) < giv_labels.count(*label)) {
            factor_matches_giv_vars = false;
            break;
          }
        }
        if (factor_matches_giv_vars) {
          factors_candidates.push_back(factors[j]);
        }
      }

      // Go over all factors containing the labels in the "given" set, and create
      // the initial permutation. It includes all labels in a factor that are not in the "given" set.
      for (size_t j = 0; j < factors_candidates.size(); ++j) {
        Factor giv_labels_copy = giv_labels;
        std::vector<int> candidate_inf_labels;
        candidate_inf_labels.reserve(factors_candidates[j].size());
        for (auto label = factors_candidates[j].begin(); label != factors_candidates[j].end(); ++label) {
          // Check if the label belongs to the "given" set.
          const auto& it = giv_labels_copy.find(*label);
          if (it != giv_labels_copy.end()) {
            giv_labels_copy.erase(it);
          } else {
            candidate_inf_labels.push_back(*label);
          }
        }

        bool is_assignment_label_valid = true;
        for (size_t z = 0; z < candidate_inf_labels.size(); ++z) {
          if (!fweights.label_checker_.IsLabelValid(candidate_inf_labels[z])) {
            is_assignment_label_valid = false;
          }
        }
        if (!is_assignment_label_valid) {
          continue;
        }
        uint64 num_permutations = CalculateFactorial(candidate_inf_labels.size());
        // If the factorial will go in overflow it will return -1.
        size_t current_num_permutations = 0;
        if (num_permutations < 0 || num_permutations > FLAGS_permutations_beam_size) {
          while (current_num_permutations < FLAGS_permutations_beam_size) {
            PerformPermutationOptimization(inf_nodes, fweights, candidate_inf_labels, &best_assignments, &best_score);
            std::random_shuffle(candidate_inf_labels.begin(), candidate_inf_labels.end());
            current_num_permutations++;
          }
        } else {
          std::sort(candidate_inf_labels.begin(), candidate_inf_labels.end());
          do {
            PerformPermutationOptimization(inf_nodes, fweights, candidate_inf_labels, &best_assignments, &best_score);
            current_num_permutations++;
          } while(std::next_permutation(candidate_inf_labels.begin(), candidate_inf_labels.end()) &&
                  current_num_permutations < FLAGS_permutations_beam_size);
        }
      }
      for (size_t j = 0; j < inf_nodes.size(); ++j) {
        assignments_[inf_nodes[j]].label = best_assignments[j];
      }
    }
  }

  void PerformPermutationOptimization(const std::vector<int>& inf_nodes,
                                      const GraphInference& fweights,
                                      const std::vector<int>& candidate_inf_labels,
                                      std::vector<int>* best_assignments,
                                      double* best_score) {
    for (size_t z = 0; z < inf_nodes.size(); ++z) {
      assignments_[inf_nodes[z]].label = candidate_inf_labels[z];
    }
    for (size_t z = 0; z < inf_nodes.size(); ++z) {
      if (HasDuplicationConflictsAtNode(inf_nodes[z])) {
        return;
      }
    }
    double score = 0;
    for (size_t z = 0; z < inf_nodes.size(); ++z) {
      score += GetNodeScore(fweights, inf_nodes[z]);
    }
    if (score > *best_score) {
      for (size_t z = 0; z < inf_nodes.size(); ++z) {
        (*best_assignments)[z] = assignments_[inf_nodes[z]].label;
      }
      *best_score = score;
    }
  }

private:
  struct Assignment {
    Assignment() : must_infer(false), label(-1) {}
    bool must_infer;
    int label;
  };

  std::vector<Assignment> assignments_;

  struct LabelPenalty {
    LabelPenalty() : label(-2), penalty(0) {}
    int label;
    double penalty;
  };

  std::vector<LabelPenalty> penalties_;

  const GraphQuery* query_;
  LabelSet* label_set_;
  int unknown_label_;

#ifdef GRAPH_INFERENCE_STATS
  mutable GraphInferenceStats stats_;
#endif

  friend class GraphInference;
  friend class LoopyBPInference;
};


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
    for (const GraphQuery::Arc& arc : a_.query_->arcs_adjacent_to_node_[node]) {
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
      for (const GraphQuery::Arc& arc : a_.query_->arcs_adjacent_to_node_[node]) {
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




GraphInference::GraphInference() : unknown_label_(-1), regularizer_(1.0), svm_margin_(1e-9), beam_size_(0), num_svm_training_samples_(0) {
  // Initialize dense_hash_map.
  features_.set_empty_key(GraphFeature(-1, -1, -1));
  features_.set_deleted_key(GraphFeature(-2, -2, -2));
  best_features_for_type_.set_empty_key(-1);
  best_features_for_type_.set_deleted_key(-2);
  best_factor_features_first_level_.set_empty_key(-1);
  best_factor_features_first_level_.set_deleted_key(-2);
  label_frequency_.set_empty_key(-1);
  label_frequency_.set_deleted_key(-2);
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
  for (int i = 0; i < num_features; ++i) {
    GraphFeature f(0, 0, 0);
    double score;
    CHECK_EQ(1, fread(&f, sizeof(GraphFeature), 1, ffile));
    CHECK_EQ(1, fread(&score, sizeof(double), 1, ffile));
    features_[f].setValue(score);
  }

  int ret = fread(&num_factor_features, sizeof(int), 1, ffile);
  if (ret == 1) {
    for (int i = 0; i < num_factor_features; ++i) {
      Factor f;
      int size_of_factor = 0;
      CHECK_EQ(1, fread(&size_of_factor, sizeof(int), 1, ffile));
      uint64 hash = 0;
      for (int j = 0; j < size_of_factor; ++j) {
        int f_var = -1;
        CHECK_EQ(1, fread(&f_var, sizeof(int), 1, ffile));
        f.insert(f_var);
        hash += HashInt(f_var);
      }
      double score;
      CHECK_EQ(1, fread(&score, sizeof(double), 1, ffile));
      factor_features_[hash] = score;
    }
  }
  fclose(ffile);
  CHECK_EQ(features_.size(), num_features);

  FILE* sfile = fopen(StringPrintf("%s_strings", file_prefix.c_str()).c_str(), "rb");
  strings_.loadFromFile(sfile);
  fclose(sfile);

  if (!FLAGS_unknown_label.empty()) {
    int a, b, size;
    label_frequency_.clear();
    FILE* lffile = fopen(StringPrintf("%s_lfreq", file_prefix.c_str()).c_str(), "rb");
    CHECK_EQ(1, fread(&size, sizeof(int), 1, lffile));
    for (int i = 0; i < size; ++i) {
      CHECK_EQ(1, fread(&a, sizeof(int), 1, lffile));
      CHECK_EQ(1, fread(&b, sizeof(int), 1, lffile));
      label_frequency_[a] = b;
    }
    CHECK_EQ(static_cast<int>(label_frequency_.size()), size);
    fclose(lffile);
  }

  LOG(INFO) << "Loading model done";

  PrepareForInference();
}

void GraphInference::SaveModel(const std::string& file_prefix) {
  LOG(INFO) << "Saving model " << file_prefix << "...";
  FILE* ffile = fopen(StringPrintf("%s_features", file_prefix.c_str()).c_str(), "wb");
  int num_features = features_.size();
  int num_factor_features = factors_set_.size();
  fwrite(&num_features, sizeof(int), 1, ffile);
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    fwrite(&it->first, sizeof(GraphFeature), 1, ffile);
    double value = it->second.getValue();
    fwrite(&value, sizeof(double), 1, ffile);
  }

  fwrite(&num_factor_features, sizeof(int), 1, ffile);
  for (auto f = factors_set_.begin(); f != factors_set_.end(); ++f) {
    int size_of_factor = f->size();
    uint64 hash = 0;
    fwrite(&size_of_factor, sizeof(int), 1, ffile);
    for (auto var = f->begin(); var != f->end(); ++var) {
      int var_val = *var;
      hash += HashInt(var_val);
      fwrite(&var_val, sizeof(int), 1, ffile);
    }
    double value = factor_features_[hash];
    fwrite(&value, sizeof(double), 1, ffile);
  }
  fclose(ffile);

  FILE* sfile = fopen(StringPrintf("%s_strings", file_prefix.c_str()).c_str(), "wb");
  strings_.saveToFile(sfile);
  fclose(sfile);

  if (!FLAGS_unknown_label.empty()) {
    int x;
    FILE* lffile = fopen(StringPrintf("%s_lfreq", file_prefix.c_str()).c_str(), "wb");
    x = label_frequency_.size();
    fwrite(&x, sizeof(int), 1, lffile);
    for (auto it = label_frequency_.begin(); it != label_frequency_.end(); ++it) {
      x = it->first;
      fwrite(&x, sizeof(int), 1, lffile);
      x = it->second;
      fwrite(&x, sizeof(int), 1, lffile);
    }
    fclose(lffile);
  }

  LOG(INFO) << "Saving model done";
}

Nice2Query* GraphInference::CreateQuery() const {
  return new GraphQuery(&strings_, &label_checker_);
}
Nice2Assignment* GraphInference::CreateAssignment(Nice2Query* query) const {
  GraphQuery* q = static_cast<GraphQuery*>(query);
  return new GraphNodeAssignment(q, &q->label_set_, unknown_label_);
}
void GraphInference::PerformAssignmentOptimization(GraphNodeAssignment* a) const {
  if (unknown_label_ >= 0) {
    a->ReplaceLabelsWithUnknown(*this);
  }
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

void GraphInference::UpdateStats(
    const GraphNodeAssignment& assignment,
    const GraphNodeAssignment& new_assignment,
    PrecisionStats *stats,
    const double margin) {

  int correct_labels = 0, incorrect_labels = 0, num_known_predictions = 0;
  for (size_t i = 0; i < new_assignment.assignments_.size(); ++i) {
    if (new_assignment.assignments_[i].must_infer) {
      if (new_assignment.assignments_[i].label != unknown_label_) {
        ++num_known_predictions;
      }
      if (new_assignment.assignments_[i].label == assignment.assignments_[i].label &&
          new_assignment.assignments_[i].label != unknown_label_) {
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
    stats->num_known_predictions += num_known_predictions;
    ++num_svm_training_samples_;
    if (num_svm_training_samples_ % 10000 == 0) {
      double error_rate = stats->incorrect_labels / (static_cast<double>(stats->incorrect_labels + stats->correct_labels));
      double recall = stats->num_known_predictions / (static_cast<double>(stats->incorrect_labels + stats->correct_labels));
      LOG(INFO) << "At training sample " << num_svm_training_samples_ << ": error rate of " << std::fixed << error_rate
          << " . Recall " << std::fixed << recall;
    }
  }
}

void GraphInference::InitializeFeatureWeights(double regularization) {
  regularizer_ = 1 / regularization;
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    it->second.setValue(regularizer_ * 0.5);
  }
  for (auto it = factor_features_.begin(); it != factor_features_.end(); ++it) {
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

  UpdateStats((*a), new_assignment, stats, svm_margin_);

  // Perform gradient descent.
  SimpleFeaturesMap affected_features;  // Gradient for each affected feature.
  Uint64FactorFeaturesMap factor_affected_features;
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

  for (auto f_feature = factor_affected_features.begin(); f_feature != factor_affected_features.end(); ++f_feature) {
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
  CHECK_GT(beam_size_, 0) << "PLInit not called or beam size was set to an invalid value.";
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);

  // Perform gradient descent
  SimpleFeaturesMap affected_features;  // Gradient for each affected feature.
  affected_features.set_empty_key(GraphFeature(-1, -1, -1));
  affected_features.set_deleted_key(GraphFeature(-2, -2, -2));

  Uint64FactorFeaturesMap factor_affected_features;

  for (size_t i = 0; i < a->assignments_.size(); ++i) {
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

  for (auto f_feature = factor_affected_features.begin(); f_feature != factor_affected_features.end(); ++f_feature) {
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

void GraphInference::FillGraphProto(
    const Nice2Query* query,
    const Nice2Assignment* assignment,
    nice2protos::ShowGraphResponse* graph) const {
  const GraphNodeAssignment* a = static_cast<const GraphNodeAssignment*>(assignment);
  for (size_t i = 0; i < a->assignments_.size(); ++i) {
    if (a->assignments_[i].must_infer ||
        !a->query_->arcs_adjacent_to_node_[i].empty()) {
      // Include the node.
      auto *node = graph->add_nodes();
      node->set_id(i);
      int label = a->assignments_[i].label;
      node->set_label(label < 0 ? StringPrintf("%d", label).c_str() : a->GetLabelName(label));
      node->set_color(a->assignments_[i].must_infer ? "#6c9ba4" : "#96816a");
    }
  }
  std::unordered_map<IntPair, std::string> dedup_arcs;
  for (const GraphQuery::Arc& arc : a->query_->arcs_) {
    std::string& s = dedup_arcs[IntPair(std::min(arc.node_a, arc.node_b),std::max(arc.node_a, arc.node_b))];
    if (!s.empty()) {
      s.append(", ");
    }
    StringAppendF(&s, "%s - %.2f",
                  a->GetLabelName(arc.type),
                  a->GetNodePairScore(*this, arc.node_a, arc.node_b, a->assignments_[arc.node_a].label, a->assignments_[arc.node_b].label));
  }

  int edge_id = 0;
  for (const auto &arc : dedup_arcs) {
    auto *edge = graph->add_edges();
    edge->set_id(edge_id);
    edge->set_label(arc.second);
    edge->set_source(arc.first.first);
    edge->set_target(arc.first.second);
    edge_id++;
  }
}

void GraphInference::AddQueryToModel(const nice2protos::Query &query) {
  std::unordered_map<int, int> values;
  std::set<int> unique_values;
  for (const auto& a : query.node_assignments()) {
    int value = strings_.addString(a.label().c_str());
    values[a.node_index()] = value;
    unique_values.insert(value);
  }
  for (int value : unique_values) {
    ++label_frequency_[value];
  }

  for (const auto& f : query.features()) {
    if (f.has_binary_relation()) {
      GraphFeature feature(
          FindWithDefault(values, f.binary_relation().first_node(), -1),
          FindWithDefault(values, f.binary_relation().second_node(), -1),
          strings_.addString(f.binary_relation().relation().c_str()));
      if (feature.a_ != -1 && feature.b_ != -1) {
        features_[feature].nonAtomicAdd(1);
      }
    }

    if (FLAGS_use_factors && f.has_factor_variables()) {
      const auto& fv = f.factor_variables();
      Factor factor_vars;
      uint64 hash = 0;
      for (const auto& item : fv.nodes()) {
        int value = FindWithDefault(values, item, -1);
        if (value == -1) {
          factor_vars.clear();
          break;
        }
        factor_vars.insert(value);
        hash += HashInt(value);
      }
      if (factor_vars.empty()) {
        continue;
      }
      factors_set_.insert(factor_vars);
      factor_features_[hash] += 1;
    }
  }
}

void GraphInference::PrepareForInference() {
  if (!FLAGS_unknown_label.empty()) {
    unknown_label_ = strings_.addString(FLAGS_unknown_label.c_str());
  }
  if (!label_checker_.IsLoaded()) {
    LOG(INFO) << "Loading LabelChecker...";
    label_checker_.Load(FLAGS_valid_labels, &strings_);
    LOG(INFO) << "LabelChecker loaded";
  }
  if (unknown_label_ >= 0 && FLAGS_min_freq_known_label > 0) {
    LOG(INFO) << "Replacing rare labels with unknown label " << FLAGS_unknown_label << " ...";
    {
      google::dense_hash_map<int, int> updated_freq;
      updated_freq.set_empty_key(label_frequency_.empty_key());
      updated_freq.set_deleted_key(label_frequency_.deleted_key());
      for (auto it = label_frequency_.begin(); it != label_frequency_.end(); ++it) {
        if (it->second >= FLAGS_min_freq_known_label) {
          updated_freq[it->first] = it->second;
        }
      }
      LOG(INFO) << "Removed " << (label_frequency_.size() - updated_freq.size())
                  << " low frequency labels out of " << label_frequency_.size() << " labels.";
      label_frequency_.swap(updated_freq);
    }
    {
      FeaturesMap updated_map;
      updated_map.set_empty_key(features_.empty_key());
      updated_map.set_deleted_key(features_.deleted_key());
      for (auto it = features_.begin(); it != features_.end(); ++it) {
        GraphFeature f = it->first;
        double feature_weight = it->second.getValue();
        if (label_frequency_.find(f.a_) == label_frequency_.end()) {
          f.a_ = unknown_label_;
        }
        if (label_frequency_.find(f.b_) == label_frequency_.end()) {
          f.b_ = unknown_label_;
        }
        updated_map[f].nonAtomicAdd(feature_weight);
      }
      LOG(INFO) << "Removed " << (features_.size() - updated_map.size())
                  << " out of " << features_.size() << " features.";
      features_.swap(updated_map);
    }
  }
  num_svm_training_samples_ = 0;


  best_features_for_type_.clear();
  best_features_for_a_type_.clear();
  best_features_for_b_type_.clear();
  best_factor_features_first_level_.clear();

  for (auto it = features_.begin(); it != features_.end(); ++it) {
    const GraphFeature& f = it->first;
    double feature_weight = it->second.getValue();
    best_features_for_type_[f.type_].push_back(std::pair<double, GraphFeature>(feature_weight, f));
    best_features_for_a_type_[IntPair(f.a_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.b_));
    best_features_for_b_type_[IntPair(f.b_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.a_));
  }
  for (auto factor_feature = factors_set_.begin(); factor_feature != factors_set_.end(); ++factor_feature) {
    Factor f = *factor_feature;
    uint64 hash = 0;
    for (auto current_var = f.begin(); current_var != f.end(); ++current_var) {
      hash += HashInt(*current_var);
    }
    double feature_weight = factor_features_[hash];
    Factor visited_labels;
    std::shared_ptr<std::pair<double, Factor>> factor_feature_shared_pointer = std::make_shared<std::pair<double, Factor>>(feature_weight, f);
    best_factor_features_first_level_[f.size()].InsertFactorFeature(factor_feature_shared_pointer, f, 0, FLAGS_maximum_depth, -1, visited_labels, kFactorsLimitBeforeGoingDepperMultiLevelMap);
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
  for (auto it = best_factor_features_first_level_.begin(); it != best_factor_features_first_level_.end(); ++it) {
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

  std::map<std::vector<GraphQuery::Arc>, std::vector<int> > nodes_per_confusion;

  for (int node_id = 0; node_id < static_cast<int>(q->arcs_adjacent_to_node_.size()); ++node_id) {
    if (!a->assignments_[node_id].must_infer) continue;
    std::vector<GraphQuery::Arc> arcs(q->arcs_adjacent_to_node_[node_id]);
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


