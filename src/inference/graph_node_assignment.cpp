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

#include <unordered_set>

#include "updatable_priority_queue.h"
#include "gflags/gflags.h"
#include "graph_node_assignment.h"
#include "stringprintf.h"

DEFINE_int32(skip_per_arc_optimization_for_nodes_above_degree, 32,
    "Skip the per-arc optimization pass if an edge is connected to a node with the in+out degree more than the given value");
DEFINE_uint64(permutations_beam_size, 64, "Permutations beam size");

static const size_t kInitialAssignmentBeamSize = 4;

GraphNodeAssignment::GraphNodeAssignment(const GraphQuery* query, LabelSet* label_set) : query_(query), label_set_(label_set) {
  assignments_.assign(query_->numberer_.size(), Assignment());
  penalties_.assign(assignments_.size(), LabelPenalty());
}

GraphNodeAssignment::~GraphNodeAssignment() {
}

void GraphNodeAssignment::SetUpEqualityPenalty(double penalty) {
  ClearPenalty();
  for (size_t i = 0; i < assignments_.size(); ++i) {
    if (assignments_[i].must_infer) {
      penalties_[i].label = assignments_[i].label;
      penalties_[i].penalty = penalty;
    }
  }
}

void GraphNodeAssignment::PrintInferredAssignments() {
  LOG(INFO) << assignments_.size();
  for(uint i = 0; i < assignments_.size(); i++) {
    const Assignment a = assignments_[i];
    if (a.must_infer == true) {
      LOG(INFO) << "Variable number: " << std::fixed << i << " Label: " << std::fixed << label_set_->GetLabelName(a.label);
    }
  }
}

void GraphNodeAssignment::ClearPenalty() {
  penalties_.assign(assignments_.size(), LabelPenalty());
}

void GraphNodeAssignment::FromJSON(const Json::Value& assignment) {
  CHECK(assignment.isArray());
  assignments_.assign(query_->numberer_.size(), Assignment());
  for (const Json::Value& a : assignment) {
    Assignment aset;
    if (a.isMember("inf")) {
      aset.label = label_set_->AddLabelName(a["inf"].asCString());
      aset.must_infer = true;
    } else {
      CHECK(a.isMember("giv"));
      aset.label = label_set_->AddLabelName(a["giv"].asCString());
      aset.must_infer = false;
    }
    int number = query_->numberer_.ValueToNumberWithDefault(a.get("v", Json::Value::null), -1);
    if (number != -1) {
      assignments_[number] = aset;
    }
  }

  ClearPenalty();
}

 void GraphNodeAssignment::ToJSON(Json::Value* assignment) const {
  *assignment = Json::Value(Json::arrayValue);
  for (size_t i = 0; i < assignments_.size(); ++i) {
    if (assignments_[i].label < 0) continue;

    Json::Value obj(Json::objectValue);
    obj["v"] = query_->numberer_.NumberToValue(i);
    const char* str_value = label_set_->GetLabelName(assignments_[i].label);
    if (assignments_[i].must_infer) {
      obj["inf"] = Json::Value(str_value);
    } else {
      obj["giv"] = Json::Value(str_value);
    }
    assignment->append(obj);
  }
}

 void GraphNodeAssignment::ClearInferredAssignment() {
  for (size_t i = 0; i < assignments_.size(); ++i) {
    if (assignments_[i].must_infer) {
      assignments_[i].label = -1;
    }
  }
}

 void GraphNodeAssignment::CompareAssignments(const Nice2Assignment* reference, PrecisionStats* stats) const {
  const GraphNodeAssignment* ref = static_cast<const GraphNodeAssignment*>(reference);
  int correct_labels = 0;
  int incorrect_labels = 0;
  for (size_t i = 0; i < assignments_.size(); ++i) {
    if (assignments_[i].must_infer) {
      if (assignments_[i].label == ref->assignments_[i].label) {
        ++correct_labels;
      } else {
        ++incorrect_labels;
      }
    }
  }
  std::lock_guard<std::mutex> guard(stats->lock);
  stats->correct_labels += correct_labels;
  stats->incorrect_labels += incorrect_labels;
}

 void GraphNodeAssignment::CompareAssignmentErrors(const Nice2Assignment* reference, SingleLabelErrorStats* error_stats) const {
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

std::string GraphNodeAssignment::DebugString() const {
  std::string result;
  for (int node = 0; node < static_cast<int>(assignments_.size()); ++node) {
    StringAppendF(&result, "[%d:%s]%s ", node, label_set_->GetLabelName(assignments_[node].label), assignments_[node].must_infer ? "" : "*");
  }
  return result;
}

const char* GraphNodeAssignment::GetLabelName(int label_id) const {
  return label_set_->GetLabelName(label_id);
}

// Returns the penalty associated with a node and its label (used in Max-Margin training).
double GraphNodeAssignment::GetNodePenalty(int node) const {
  return (assignments_[node].label == penalties_[node].label) ? penalties_[node].penalty : 0.0;
}
// Gets the score contributed by all arcs adjacent to a node.
double GraphNodeAssignment::GetNodeScore(const GraphInference& fweights, int node) const {
  double sum = -GetNodePenalty(node);

  const GraphInference::FeaturesMap& features = fweights.features_;
  const GraphInference::FactorFeaturesMap& factor_features = fweights.factor_features_;
  for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
    GraphFeature feature(
        assignments_[arc.node_a].label,
        assignments_[arc.node_b].label,
        arc.type);
    auto feature_it = features.find(feature);
    if (feature_it != features.end()) {
      sum += feature_it->second.getValue();
    }
  }

  for (uint i = 0; i < query_->factors_of_a_node_[node].size(); i++) {
    Factor factor;
    factor.insert(assignments_[node].label);
    for (auto var = query_->factors_of_a_node_[node][i].begin(); var != query_->factors_of_a_node_[node][i].end(); var++) {
      factor.insert(assignments_[*var].label);
    }

    auto factor_feature = factor_features.find(factor);
    if (factor_feature != factor_features.end()) {
      sum += factor_feature->second;
    }
  }
  return sum;
}

// Gets the node score given an assignment
double GraphNodeAssignment::GetNodeScoreGivenAssignmentToANode(const GraphInference& fweights, int node, int node_assigned, int node_assignment) const {
  double sum = -GetNodePenalty(node);
  const GraphInference::FeaturesMap& features = fweights.features_;
  const GraphInference::FactorFeaturesMap& factor_features = fweights.factor_features_;
  for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
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
  for (uint i = 0; i < query_->factors_of_a_node_[node].size(); i++) {
    Factor factor;
    factor.insert(node_label);
    for (auto var = query_->factors_of_a_node_[node][i].begin(); var != query_->factors_of_a_node_[node][i].end(); var++) {
      factor.insert(assignments_[*var].label);
    }
    auto factor_feature = factor_features.find(factor);
    if (factor_feature != factor_features.end()) {
      sum += factor_feature->second;
    }
  }

  return sum;
}

double GraphNodeAssignment::GetNodeScoreOnAssignedNodes(
    const GraphInference& fweights, int node,
    const std::vector<bool>& assigned) const {
  double sum = -GetNodePenalty(node);
  const GraphInference::FeaturesMap& features = fweights.features_;
  for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
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
  // TODO Add factors to the calculation (?)
  //for (int i =0; i < query_->factors_of_a_node_[node].size(); i++) {
  //  std::vector<int> f(query_->factors_of_a_node_[node][i]);
  //  f.push_back(node);
  //  std::sort(f.begin(), f.end());
  //}
  return sum;
}

bool GraphNodeAssignment::HasDuplicationConflictsAtNode(int node) const {
  int node_label = assignments_[node].label;
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
int GraphNodeAssignment::GetNodeWithDuplicationConflict(int node) const {
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
double GraphNodeAssignment::GetNodePairScore(const GraphInference& fweights, int node1, int node2, int label1, int label2) const {
  double sum = 0;
  const GraphInference::FeaturesMap& features = fweights.features_;
  for (const Arc& arc : FindWithDefault(query_->arcs_connecting_node_pair_, IntPair(node1, node2), std::vector<Arc>())) {
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

int GraphNodeAssignment::GetNumAdjacentArcs(int node) const {
  return query_->arcs_adjacent_to_node_[node].size();
}

void GraphNodeAssignment::GetFactorCandidates(const GraphInference& fweights,
    int factor_size,
    std::vector<Factor>* candidates,
    Factor giv_labels,
    uint beam_size) {
  FactorFeaturesLevel empty_level;
  FactorFeaturesLevel v;
  v = FindWithDefault(fweights.best_factor_features_first_level_, factor_size, empty_level);
  auto it = giv_labels.begin();
  v.GetFactors(giv_labels, 0, *it, candidates, beam_size);
  for (unsigned int i = 0; i < v.factor_features.size() && i < beam_size; i++) {
    candidates->push_back(v.factor_features[i].second);
  }
}

void GraphNodeAssignment::GetLabelCandidates(const GraphInference& fweights, int node,
    std::vector<int>* candidates, size_t beam_size) const {
  std::vector<std::pair<double, int> > empty_vec;
  for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
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

  for (uint i = 0; i < query_->factors_of_a_node_[node].size(); i++) {
    Factor f_with_assignments;
    for (auto var = query_->factors_of_a_node_[node][i].begin(); var != query_->factors_of_a_node_[node][i].end(); var++) {
      Assignment a = assignments_[*var];
      if (a.must_infer == false) {
        f_with_assignments.insert(assignments_[*var].label);
      } else {
        f_with_assignments.clear();
        break;
      }
    }
    if (f_with_assignments.empty()) {
      continue;
    }
    const std::vector<std::pair<double, int>>& v = FindWithDefault(fweights.best_factor_features_, f_with_assignments, empty_vec);
    for (uint j = 0; j < v.size() && j < beam_size; j++) {
      candidates->push_back(v[j].second);
    }
  }

#ifdef GRAPH_INFERENCE_STATS
  stats_.label_candidates_per_node.AddCount(candidates->size(), 1);
#endif
  std::sort(candidates->begin(), candidates->end());
  candidates->erase(std::unique(candidates->begin(), candidates->end()), candidates->end());
}

double GraphNodeAssignment::GetTotalScore(const GraphInference& fweights) const {
  double sum = 0;
  const GraphInference::FeaturesMap& features = fweights.features_;
  for (const Arc& arc : query_->arcs_) {
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

void GraphNodeAssignment::GetAffectedFeatures(
    GraphInference::SimpleFeaturesMap* affected_features,
    double gradient_weight) const {
  for (const Arc& arc : query_->arcs_) {
    GraphFeature feature(
        assignments_[arc.node_a].label,
        assignments_[arc.node_b].label,
        arc.type);
    (*affected_features)[feature] += gradient_weight;
  }
}

void GraphNodeAssignment::GetAffectedFactorFeatures(
    GraphInference::FactorFeaturesMap* affected_factor_features,
    double gradient_weight) const {
  for (const Factor& factor : query_->factors_) {
    Factor f;
    for (auto var = factor.begin(); var != factor.end(); var++) {
      f.insert(assignments_[*var].label);
    }
    LOG(INFO) << "Updating f with weight " << gradient_weight;
    for (auto it = f.begin(); it != f.end(); it++) {
      LOG(INFO) << *it;
    }
    (*affected_factor_features)[f] += gradient_weight;
  }
}
// Method that given a certain node and a label, first assign that label to the given node and then add the gradient_weight
// to every affected feature related to the given node and its neighbours
void GraphNodeAssignment::GetNeighboringAffectedFeatures(
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

void GraphNodeAssignment::GetFactorAffectedFeaturesOfNode(
    GraphInference::FactorFeaturesMap* factor_affected_features,
    int node,
    int label,
    double gradient_weight) const {
  int node_label = label;
  for (const Factor& f : query_->factors_of_a_node_[node]) {
    Factor factor;
    factor.insert(node_label);
    for (auto var = f.begin(); var != f.end(); var++) {
      factor.insert(assignments_[(*var)].label);
    }
    (*factor_affected_features)[factor] += gradient_weight;
  }
}

void GraphNodeAssignment::InitialGreedyAssignmentPass(const GraphInference& fweights) {
  std::vector<bool> assigned(assignments_.size(), false);
  for (size_t node = 0; node < assignments_.size(); ++node) {
    assigned[node] = !assignments_[node].must_infer;
  }
  UpdatablePriorityQueue<int, int> p_queue;
  for (size_t node = 0; node < assignments_.size(); ++node) {
    if (assignments_[node].must_infer) {
      int score = 0;
      for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
        if (assigned[arc.node_a] || assigned[arc.node_b]) ++score;
      }
      p_queue.SetValue(node, -score);
    }
  }

  std::vector<int> candidates;
  while (!p_queue.IsEmpty()) {
    int node = p_queue.GetKeyWithMinValue();
    p_queue.PermanentlyRemoveKeyFromQueue(node);
    for (const Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a == node) {
        p_queue.SetValue(arc.node_b, p_queue.GetValue(arc.node_b) - 1);
      } else if (arc.node_b == node) {
        p_queue.SetValue(arc.node_a, p_queue.GetValue(arc.node_a) - 1);
      }
    }

    Assignment& nodea = assignments_[node];
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

void GraphNodeAssignment::LocalPerNodeOptimizationPass(const GraphInference& fweights, size_t beam_size) {
  std::vector<int> candidates;
  for (size_t node = 0; node < assignments_.size(); ++node) {
    Assignment& nodea = assignments_[node];
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

void GraphNodeAssignment::LocalPerNodeOptimizationPassWithDuplicateNameResolution(const GraphInference& fweights, size_t beam_size) {
  std::vector<int> candidates;
  for (size_t node = 0; node < assignments_.size(); ++node) {
    Assignment& nodea = assignments_[node];
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

void GraphNodeAssignment::LocalPerArcOptimizationPass(const GraphInference& fweights, size_t beam_size) {
  std::vector<std::pair<double, GraphFeature> > empty;
  for (const Arc& arc : query_->arcs_) {
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
void GraphNodeAssignment::LocalPerFactorOptimizationPass(const GraphInference& fweights, size_t beam_size) {
  std::vector<std::pair<double, Factor>> empty;
  for (const Factor& factor : query_->factors_) {
    std::vector<int> inf_vars;
    Factor giv_labels;
    for (auto var = factor.begin(); var != factor.end(); var++) {
      Assignment a= assignments_[(*var)];
      if (a.must_infer == true) {
        inf_vars.push_back((*var));
      } else {
        giv_labels.insert(a.label);
      }
    }
    if (inf_vars.size() <= 1) {
      continue;
    }

    std::vector<Factor> factors;
    GetFactorCandidates(fweights, factor.size(), &factors, giv_labels, beam_size);
    double best_score = 0;
    std::vector<int> best_assignments;
    for (auto var = inf_vars.begin(); var != inf_vars.end(); var++) {
      best_score += GetNodeScore(fweights, *var);
      best_assignments.push_back(assignments_[*var].label);
    }
    std::vector<Factor> factors_candidates;
    for (uint j = 0; j < factors.size(); j++) {
      int factor_matches_giv_vars = true;
      for (auto label = giv_labels.begin(); label != giv_labels.end(); label++) {
        if (factors[j].count(*label) == 0) {
          factor_matches_giv_vars = false;
          break;
        }
      }
      if (factor_matches_giv_vars) {
        factors_candidates.push_back(factors[j]);
      }
    }
    for (uint j = 0; j < factors_candidates.size(); j++) {
      Factor current_candidate = factors_candidates[j];
      std::set<int> distinct_labels;
      std::vector<int> candidate_inf_labels;
      std::unordered_set<std::vector<int>> permutations;
      for (auto label = current_candidate.begin(); label != current_candidate.end(); label++) {
        if (giv_labels.count(*label) == 0) {
          candidate_inf_labels.push_back(*label);
          distinct_labels.insert(*label);
        }
      }
      uint64 num_permutations = CalculateFactorial(distinct_labels.size());
      if (distinct_labels.size() > 64 || num_permutations > FLAGS_permutations_beam_size) {
        RandomPermute(candidate_inf_labels, permutations);
      } else {
        HeapPermute(candidate_inf_labels, permutations, candidate_inf_labels.size());
      }
      for (auto it = permutations.begin(); it != permutations.end(); it++) {
        for (uint z = 0; z < inf_vars.size(); z++) {
          assignments_[inf_vars[z]].label = (*it)[z];
        }

        bool is_assignment_valid = true;
        for (uint z = 0; z < inf_vars.size(); z++) {
          if (HasDuplicationConflictsAtNode(inf_vars[z]) ||
              !fweights.label_checker_.IsLabelValid(assignments_[inf_vars[z]].label)) {
            is_assignment_valid = false;
            break;
          }
        }
        if (is_assignment_valid == false) {
          continue;
        }
        double score = 0;
        std::vector<int> assignment;
        for (uint z = 0; z < inf_vars.size(); z++) {
          score += GetNodeScore(fweights, inf_vars[z]);
          assignment.push_back(assignments_[inf_vars[z]].label);
        }
        if (score > best_score) {
          best_assignments = assignment;
          best_score = score;
        }
      }
    }
    for (uint j = 0; j < inf_vars.size(); j++) {
      assignments_[inf_vars[j]].label = best_assignments[j];
    }
  }
}

void GraphNodeAssignment::HeapPermute(std::vector<int> v, std::unordered_set<std::vector<int>>& permutations, int n) {
  if (permutations.size() > FLAGS_permutations_beam_size) {
    return;
  }
  if (n == 1) {
    permutations.insert(v);
  } else {
    for (int i = 0; i < n; i++) {
      HeapPermute(v, permutations, n-1);
      if (n % 2 == 1) {
        int temp = v[0];
        v[0] = v[n-1];
        v[n-1] = temp;
      } else {
        int temp = v[i];
        v[i] = v[n-1];
        v[n-1] = temp;
      }
    }
  }
}

void GraphNodeAssignment::RandomPermute(std::vector<int> v, std::unordered_set<std::vector<int>>& permutations) {
  std::random_device rd;
  std::mt19937 g(rd());
  while (permutations.size() < FLAGS_permutations_beam_size) {
    std::shuffle(v.begin(), v.end(), g);
    if (permutations.count(v) == 0) {
      permutations.insert(v);
    }
  }
}

uint64 GraphNodeAssignment::CalculateFactorial(int n) {
  uint64 result = 1;
  for (uint i = n; i > 0; i--) {
    result *= i;
  }
  return result;
}



