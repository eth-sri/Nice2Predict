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

#ifndef N2_INFERENCE_GRAPH_INFERENCE_H_
#define N2_INFERENCE_GRAPH_INFERENCE_H_

#include <unordered_map>
#include <google/dense_hash_map>
#include <string.h>
#include <iterator>

#include "base/base.h"
#include "base/maputil.h"
#include "base/stringset.h"

#include "inference.h"
#include "label_checker.h"
#include "lock_free_weight.h"

typedef std::multiset<int> Factor;

namespace std {
  template <> struct hash<std::vector<int>> {
    size_t operator()(const std::vector<int>& x) const {
      int hc = x.size();
      for (unsigned int i = 0; i < x.size(); i++) {
        hc = hc * 6037 + x[i];
      }
      return hc;
    }
  };
}

namespace std {
  template <> struct hash<std::multiset<int>> {
    size_t operator()(const std::multiset<int>& x) const {
      int hc = x.size();
      for (auto var = x.begin(); var != x.end(); var++) {
        hc = hc * 6037 + *var;
      }
      return hc;
    }
  };
}

struct NodeConfusionStats {
  NodeConfusionStats() : num_non_confusable_nodes(0), num_confusable_nodes(0), num_expected_confusions(0) { }

  int num_non_confusable_nodes;
  int num_confusable_nodes;
  int num_expected_confusions;
};

struct FactorFeaturesLevel {
  FactorFeaturesLevel() : factor_features(std::vector<std::shared_ptr<std::pair<double, Factor>>>()), next_level(std::unordered_map<int, std::shared_ptr<FactorFeaturesLevel>>()) {}

  ~FactorFeaturesLevel() {
    factor_features.clear();
    next_level.clear();
  }

  void InsertFactorFeature(std::shared_ptr<std::pair<double, Factor>> factor_feature,
                           const Factor& f,
                           int current_depth,
                           int maximum_depth,
                           int current_label,
                           Factor visited_labels,
                           const size_t factors_limit_before_going_to_next_level) {
    factor_features.push_back(factor_feature);

    Factor next_level_labels_visited;
    if (current_label > 0) {
      visited_labels.insert(current_label);
    }
    if (current_depth < maximum_depth && visited_labels.size() < f.size() && factor_features.size() > factors_limit_before_going_to_next_level ) {
      for (auto it = f.begin(); it != f.end(); ++it) {
        if ((visited_labels.count(*it) + next_level_labels_visited.count(*it)) < f.count(*it)) {
          next_level_labels_visited.insert(*it);
          if (next_level.count(*it) == 0) {
            next_level[*it] = std::make_shared<FactorFeaturesLevel>();
          }
          next_level[*it]->InsertFactorFeature(factor_feature, f,current_depth + 1, maximum_depth, *it, visited_labels, factors_limit_before_going_to_next_level);
        }
      }
    }
  }

  void GetFactors(Factor giv_labels, int next_level_label, std::vector<Factor>* candidates, size_t beam_size) const {
    if (next_level.empty() || giv_labels.empty() || next_level.count(next_level_label) == 0) {
      for (auto it = factor_features.begin(); it != factor_features.end() && candidates->size() < beam_size; ++it) {
        candidates->push_back((*it)->second);
      }
    } else {
      auto it = giv_labels.begin();
      giv_labels.erase(it);
      it++;
      const auto& nl = next_level.find(next_level_label);
      if (nl != next_level.end()) {
        nl->second->GetFactors(giv_labels, *it, candidates, beam_size);
      }
    }
  }

  void SortFactorFeatures() {
    std::sort(factor_features.begin(), factor_features.end(), [](std::shared_ptr<std::pair<double, Factor>> a, std::shared_ptr<std::pair<double, Factor>> b) {
      return a->first > b->first;
    });
    for (auto it = next_level.begin(); it != next_level.end(); ++it) {
      it->second->SortFactorFeatures();
    }
  }

  std::vector<std::shared_ptr<std::pair<double, Factor>>> factor_features;
  std::unordered_map<int, std::shared_ptr<FactorFeaturesLevel>> next_level;
};

class GraphFeature {
public:
  GraphFeature() : a_(-1), b_(-1), type_(-1) {}
  explicit GraphFeature(int a, int b, int type) : a_(a), b_(b), type_(type) {
  }

  bool operator==(const GraphFeature& o) const {
    return a_ == o.a_ && b_ == o.b_ && type_ == o.type_;
  }

  bool operator<(const GraphFeature& o) const {
    if (a_ != o.a_) return a_ < o.a_;
    if (b_ != o.b_) return b_ < o.b_;
    return type_ < o.type_;
  }

  int a_, b_, type_;
};

namespace std {
  template <> struct hash<GraphFeature> {
    size_t operator()(const GraphFeature& x) const {
      return x.a_ * 6037 + x.b_ * 6047 + x.type_;
    }
  };
}

class GraphNodeAssignment;

class GraphInference : public Nice2Inference {
public:
  GraphInference();
  virtual ~GraphInference() override;

  virtual void LoadModel(const std::string& file_prefix) override;
  virtual void SaveModel(const std::string& file_prefix) override;

  virtual Nice2Query* CreateQuery() const override;
  virtual Nice2Assignment* CreateAssignment(Nice2Query* query) const override;

  virtual void MapInference(
      const Nice2Query* query,
      Nice2Assignment* assignment) const override;

  virtual double GetAssignmentScore(const Nice2Assignment* assignment) const override;

  virtual void UpdateStats(
      const GraphNodeAssignment& assignment,
      const GraphNodeAssignment& new_assignment,
      PrecisionStats *stats,
      const double margin);

  virtual void InitializeFeatureWeights(double regularization) override;

  virtual void SSVMInit(double margin) override;

  virtual void PLInit(int beam_size) override;

  // This method is thread-safe for Hogwild training. i.e. two instance of SSVMLearn can be
  // called in parallel, but they cannot be called in parallel with other method.
  virtual void SSVMLearn(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      double learning_rate,
      PrecisionStats* stats) override;

  // This method executes a training based on the optimization of the pseudolikelihood
  virtual void PLLearn(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      double learning_rate) override;

  virtual void AddQueryToModel(const nice2protos::Query &query) override;
  virtual void PrepareForInference() override;

  virtual void FillGraphProto(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      nice2protos::ShowGraphResponse* graph) const override;

  void PrintDebugInfo();

  void PrintConfusionStatistics(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      NodeConfusionStats* stats);

  typedef std::unordered_map<int, std::vector<std::pair<double, Factor>>> LabelFactorsMap;

private:
  friend class GraphNodeAssignment;
  friend class LoopyBPInference;

  void PerformAssignmentOptimization(GraphNodeAssignment* a) const;

  typedef google::dense_hash_map<GraphFeature, LockFreeWeights> FeaturesMap;
  typedef google::dense_hash_map<GraphFeature, double> SimpleFeaturesMap;
  typedef std::unordered_map<uint64, double> Uint64FactorFeaturesMap;
  // std::unordered_map<GraphFeature, double> features_;
  FeaturesMap features_;
  std::set<Factor> factors_set_;
  Uint64FactorFeaturesMap factor_features_;

  //google::dense_hash_map<IntPair, std::vector<std::pair<double, int> > > best_features_for_a_type_, best_features_for_b_type_;
  std::unordered_map<IntPair, std::vector<std::pair<double, int> > > best_features_for_a_type_, best_features_for_b_type_;

  google::dense_hash_map<int, FactorFeaturesLevel> best_factor_features_first_level_;

  google::dense_hash_map<int, std::vector<std::pair<double, GraphFeature> > > best_features_for_type_;
  google::dense_hash_map<int, int> label_frequency_;
  int unknown_label_;
  StringSet strings_;
  LabelChecker label_checker_;
  double regularizer_;
  double svm_margin_;
  int beam_size_;
  int num_svm_training_samples_;
};


#endif /* N2_INFERENCE_GRAPH_INFERENCE_H_ */
