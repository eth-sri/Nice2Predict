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
#include <queue>
#include <unordered_set>

#include "stringprintf.h"
#include "glog/logging.h"

#include "base.h"
#include "maputil.h"
#include "simple_histogram.h"

DEFINE_int32(graph_per_node_passes, 6, "Number of per-node passes for inference");
DEFINE_int32(graph_per_arc_passes, 2, "Number of per-arc passes for inference");
DEFINE_int32(graph_loopy_bp_passes, 0, "Number of loopy belief propagation passes for inference");
DEFINE_int32(graph_loopy_bp_steps_per_pass, 3, "Number of loopy belief propagation steps in each inference pass");
DEFINE_int32(skip_per_arc_optimization_for_nodes_above_degree, 32,
    "Skip the per-arc optimization pass if an edge is connected to a node with the in+out degree more than the given value");


DEFINE_string(valid_labels, "valid_names.txt", "A file describing valid names");

static const size_t kPerArcBeamSize = 64;
static const size_t kPerNodeBeamSize = 64;
static const size_t kLoopyBPBeamSize = 32;

namespace std {
  template <> struct hash<Json::Value> {
    size_t operator()(const Json::Value& v) const {
      if (v.isInt()) {
        return hash<int>()(v.asInt());
      }
      if (v.isString()) {
        const char* str = v.asCString();
        size_t r = 1;
        while (*str) {
          r = (r * 17) + (*str);
          ++str;
        }
        return r;
      }
      LOG(INFO) << v;
      return 0;
    }
  };
}

class JsonValueNumberer {
public:
  int ValueToNumber(const Json::Value& val) {
    auto ins = data_.insert(std::pair<Json::Value, int>(val, data_.size()));
    if (ins.second) {
      number_to_value_.push_back(val);
    }
    return ins.first->second;
  }

  int ValueToNumberOrDie(const Json::Value& val) const {
    auto it = data_.find(val);
    CHECK(it != data_.end());
    return it->second;
  }

  int ValueToNumberWithDefault(const Json::Value& val, int default_number) const {
    auto it = data_.find(val);
    if(it == data_.end()) return default_number;
    return it->second;
  }

  const Json::Value& NumberToValue(int number) const {
    return number_to_value_[number];
  }

  int size() const {
    return data_.size();
  }

private:
  std::unordered_map<Json::Value, int> data_;
  std::vector<Json::Value> number_to_value_;
};


class GraphQuery : public Nice2Query {
public:
  explicit GraphQuery(const StringSet* ss) : ss_(ss) {
    arcs_connecting_node_pair_.set_empty_key(IntPair(-1, -1));
    arcs_connecting_node_pair_.set_deleted_key(IntPair(-1, -1));
  }
  virtual ~GraphQuery() {
  }

  virtual void FromJSON(const Json::Value& query) override {
    arcs_.clear();

    CHECK(query.isArray());
    for (const Json::Value& arc : query) {
      if (arc.isMember("f2")) {
        // A factor connecting two facts (an arc).
        Arc a;
        a.node_a = numberer_.ValueToNumber(arc["a"]);
        a.node_b = numberer_.ValueToNumber(arc["b"]);
        a.type = ss_->findString(arc["f2"].asCString());
        arcs_.push_back(a);
      }
      if (arc.isMember("cn")) {
        // A scope that lists names that cannot be assigned to the same value.
        const Json::Value& v = arc["n"];
        if (v.isArray()) {
          std::vector<int> scope_vars;
          scope_vars.reserve(v.size());
          for (const Json::Value& item : v) {
            scope_vars.push_back(numberer_.ValueToNumber(item));
          }
          std::sort(scope_vars.begin(), scope_vars.end());
          scope_vars.erase(std::unique(scope_vars.begin(), scope_vars.end()), scope_vars.end());
          nodes_in_scope_.push_back(std::move(scope_vars));
        }
      }
    }
    std::sort(arcs_.begin(), arcs_.end());

    arcs_adjacent_to_node_.assign(numberer_.size(), std::vector<Arc>());
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

    scopes_per_nodes_.assign(numberer_.size(), std::vector<int>());
    for (size_t scope = 0; scope < nodes_in_scope_.size(); ++scope) {
      for (int node : nodes_in_scope_[scope]) {
        scopes_per_nodes_[node].push_back(scope);
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
  std::vector<Arc> arcs_;
  google::dense_hash_map<IntPair, std::vector<Arc> > arcs_connecting_node_pair_;
  JsonValueNumberer numberer_;
  const StringSet* ss_;

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
  GraphNodeAssignment(const GraphQuery* query, const StringSet* ss) : query_(query), ss_(ss) {
    assignments_.assign(query_->numberer_.size(), Assignment());
    penalties_.assign(assignments_.size(), LabelPenalty());
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

  virtual void FromJSON(const Json::Value& assignment) override {
    CHECK(assignment.isArray());
    assignments_.assign(query_->numberer_.size(), Assignment());
    for (const Json::Value& a : assignment) {
      Assignment aset;
      if (a.isMember("inf")) {
        aset.label = ss_->findString(a["inf"].asCString());
        aset.must_infer = true;
      } else {
        CHECK(a.isMember("giv"));
        aset.label = ss_->findString(a["giv"].asCString());
        aset.must_infer = false;
      }
      int number = query_->numberer_.ValueToNumberWithDefault(a.get("v", Json::Value::null), -1);
      if (number != -1) {
        assignments_[number] = aset;
      }
    }
    ClearPenalty();
  }

  virtual void ToJSON(Json::Value* assignment) const override {
    *assignment = Json::Value(Json::arrayValue);
    for (size_t i = 0; i < assignments_.size(); ++i) {
      if (assignments_[i].label < 0) continue;

      Json::Value obj(Json::objectValue);
      obj["v"] = query_->numberer_.NumberToValue(i);
      const char* str_value = ss_->getString(assignments_[i].label);
      if (assignments_[i].must_infer) {
        obj["inf"] = Json::Value(str_value);
      } else {
        obj["giv"] = Json::Value(str_value);
      }
      assignment->append(obj);
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


  std::string DebugString() const {
    std::string result;
    for (int node = 0; node < static_cast<int>(assignments_.size()); ++node) {
      StringAppendF(&result, "[%d:%s]%s ", node, ss_->getString(assignments_[node].label), assignments_[node].must_infer ? "" : "*");
    }
    return result;
  }

  // Returns the penalty associated with a node and its label (used in Max-Margin training).
  double GetNodePenalty(int node) const {
    return (assignments_[node].label == penalties_[node].label) ? penalties_[node].penalty : 0.0;
  }

  // Gets the score contributed by all arcs adjacent to a node.
  double GetNodeScore(const GraphInference& fweights, int node) const {
    double sum = -GetNodePenalty(node);
    const GraphInference::FeaturesMap& features = fweights.features_;
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
    return sum;
  }

  bool HasDuplicationConflictsAtNode(int node) const {
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

  // Gets the score contributed by all arcs adjacent to a node not connecting to a given node.
  double GetNodeScoreNotConnecting(const GraphInference& fweights, int node, int not_connecting) const {
    double sum = -GetNodePenalty(node);
    const GraphInference::FeaturesMap& features = fweights.features_;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a == not_connecting || arc.node_b == not_connecting) continue;
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

  // Gets the score contributed by all arcs adjacent to a node.
  double GetNodeScoreOnlyToGivenAssignments(const GraphInference& fweights, int node) const {
    double sum = 0;
    const GraphInference::FeaturesMap& features = fweights.features_;
    for (const GraphQuery::Arc& arc : query_->arcs_adjacent_to_node_[node]) {
      if (arc.node_a != arc.node_b &&
          assignments_[arc.node_a].must_infer &&
          assignments_[arc.node_b].must_infer) continue;
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
      VLOG(3) << " " << ss_->getString(feature.a_) << " " << ss_->getString(feature.b_) << " " << ss_->getString(feature.type_)
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

  void LocalPerNodeOptimizationPass(const GraphInference& fweights) {
    std::vector<int> candidates;
    for (size_t node = 0; node < assignments_.size(); ++node) {
      GraphNodeAssignment::Assignment& nodea = assignments_[node];
      if (!nodea.must_infer) continue;
      candidates.clear();
      GetLabelCandidates(fweights, node, &candidates, kPerNodeBeamSize);
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

  void LocalPerArcOptimizationPass(const GraphInference& fweights) {
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
      for (size_t i = 0; i < candidates.size() && i < kPerArcBeamSize; ++i) {
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
  const StringSet* ss_;

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
        StringAppendF(&result, "  Label %s  -- %f:\n", a_.ss_->getString(label), score.total_score);
        for (auto it = score.incoming_node_to_message.begin(); it != score.incoming_node_to_message.end(); ++it) {
          StringAppendF(&result, "    From %d: %s -- %f [ arc %f ]\n", it->first, a_.ss_->getString(it->second.label), it->second.score,
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




GraphInference::GraphInference() : svm_regularizer_(1.0), svm_margin_(1e-9), num_svm_training_samples_(0) {
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
  CHECK_EQ(1, fread(&num_features, sizeof(int), 1, ffile));
  for (int i = 0; i < num_features; ++i) {
    GraphFeature f(0, 0, 0);
    double score;
    CHECK_EQ(1, fread(&f, sizeof(GraphFeature), 1, ffile));
    CHECK_EQ(1, fread(&score, sizeof(double), 1, ffile));
    features_[f].setValue(score);
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
  fwrite(&num_features, sizeof(int), 1, ffile);
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    fwrite(&it->first, sizeof(GraphFeature), 1, ffile);
    double value = it->second.getValue();
    fwrite(&value, sizeof(double), 1, ffile);
  }
  fclose(ffile);

  FILE* sfile = fopen(StringPrintf("%s_strings", file_prefix.c_str()).c_str(), "wb");
  strings_.saveToFile(sfile);
  fclose(sfile);
  LOG(INFO) << "Saving model done";
}

Nice2Query* GraphInference::CreateQuery() const {
  return new GraphQuery(&strings_);
}
Nice2Assignment* GraphInference::CreateAssignment(const Nice2Query* query) const {
  return new GraphNodeAssignment(static_cast<const GraphQuery*>(query), &strings_);
}
void GraphInference::PerformAssignmentOptimization(GraphNodeAssignment* a) const {
  double score = a->GetTotalScore(*this);
  VLOG(1) << "Start score " << score;
  int passes = std::max(FLAGS_graph_per_node_passes, std::max(FLAGS_graph_loopy_bp_passes, FLAGS_graph_per_arc_passes));
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
      a->LocalPerNodeOptimizationPass(*this);
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "Per node pass " << (end_time - start_time)/1000 << "ms.";
    }
    if (pass < FLAGS_graph_per_arc_passes) {
      int64 start_time = GetCurrentTimeMicros();
      a->LocalPerArcOptimizationPass(*this);
      int64 end_time = GetCurrentTimeMicros();
      VLOG(2) << "Per arc pass " << (end_time - start_time)/1000 << "ms.";
    }

    double updated_score = a->GetTotalScore(*this);
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

void GraphInference::SSVMInit(double regularization, double margin) {
  svm_regularizer_ = 1 / regularization;
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    it->second.setValue(svm_regularizer_ * 0.5);
  }
  svm_margin_ = margin;
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
  affected_features.set_empty_key(GraphFeature(-1, -1, -1));
  affected_features.set_deleted_key(GraphFeature(-2, -2, -2));
  a->GetAffectedFeatures(&affected_features, learning_rate);
  new_assignment.GetAffectedFeatures(&affected_features, -learning_rate);

  for (auto it = affected_features.begin(); it != affected_features.end(); ++it) {
    if (it->second < -1e-9 || it->second > 1e-9) {
      VLOG(3) << strings_.getString(it->first.a_) << " " << strings_.getString(it->first.b_) << " " << strings_.getString(it->first.type_) << " " << it->second;
      auto features_it = features_.find(it->first);
      if (features_it != features_.end()) {
        features_it->second.atomicAddRegularized(it->second, 0, svm_regularizer_);
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
      node["label"] = Json::Value(label < 0 ? StringPrintf("%d", label).c_str() : strings_.getString(label));
      node["color"] = Json::Value(a->assignments_[i].must_infer ? "#33c" : "#500");
      nodes.append(node);
    }
  }
  Json::Value& edges = (*graph)["edges"];
  int edge_id = 0;
  for (const GraphQuery::Arc& arc : a->query_->arcs_) {
    Json::Value edge;
    edge["id"] = Json::Value(StringPrintf("Edge%d", edge_id));
    edge["label"] = Json::Value(StringPrintf("%s - %.2f",
        strings_.getString(arc.type),
        a->GetNodePairScore(*this, arc.node_a, arc.node_b, a->assignments_[arc.node_a].label, a->assignments_[arc.node_b].label)));
    edge["source"] = Json::Value(StringPrintf("N%d", static_cast<int>(arc.node_a)));
    edge["target"] = Json::Value(StringPrintf("N%d", static_cast<int>(arc.node_b)));
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
  for (auto it = features_.begin(); it != features_.end(); ++it) {
    const GraphFeature& f = it->first;
    double feature_weight = it->second.getValue();
    best_features_for_type_[f.type_].push_back(std::pair<double, GraphFeature>(feature_weight, f));
    best_features_for_a_type_[IntPair(f.a_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.b_));
    best_features_for_b_type_[IntPair(f.b_, f.type_)].push_back(std::pair<double, int>(feature_weight, f.a_));
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
  LOG(INFO) << "GraphInference prepared for MAP inference.";
}

