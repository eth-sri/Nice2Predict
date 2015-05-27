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

#include "inference.h"
#include "lock_free_weight.h"
#include "stringset.h"
#include "maputil.h"
#include "label_checker.h"


struct NodeConfusionStats {
  NodeConfusionStats() : num_non_confusable_nodes(0), num_confusable_nodes(0), num_expected_confusions(0) { }

  int num_non_confusable_nodes;
  int num_confusable_nodes;
  int num_expected_confusions;
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

namespace std { namespace tr1 {
  template <> struct hash<GraphFeature> {
    size_t operator()(const GraphFeature& x) const {
      return x.a_ * 6037 + x.b_ * 6047 + x.type_;
    }
  };
}}

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

  virtual void SSVMInit(double regularization, double margin) override;

  // This method is thread-safe for Hogwild training. i.e. two instance of SSVMLearn can be
  // called in parallel, but they cannot be called in parallel with other method.
  virtual void SSVMLearn(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      double learning_rate,
      PrecisionStats* stats) override;

  virtual void AddQueryToModel(const Json::Value& query, const Json::Value& assignment) override;
  virtual void PrepareForInference() override;

  virtual void DisplayGraph(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      Json::Value* graph) const override;

  void PrintDebugInfo();

  void PrintConfusionStatistics(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      NodeConfusionStats* stats);

private:
  friend class GraphNodeAssignment;
  friend class LoopyBPInference;

  void PerformAssignmentOptimization(GraphNodeAssignment* a) const;

  typedef google::dense_hash_map<GraphFeature, LockFreeWeights> FeaturesMap;
  typedef google::dense_hash_map<GraphFeature, double> SimpleFeaturesMap;

  // std::unordered_map<GraphFeature, double> features_;
  FeaturesMap features_;
  //google::dense_hash_map<IntPair, std::vector<std::pair<double, int> > > best_features_for_a_type_, best_features_for_b_type_;
  std::unordered_map<IntPair, std::vector<std::pair<double, int> > > best_features_for_a_type_, best_features_for_b_type_;
  google::dense_hash_map<int, std::vector<std::pair<double, GraphFeature> > > best_features_for_type_;
  StringSet strings_;
  LabelChecker label_checker_;
  double svm_regularizer_;
  double svm_margin_;

  int num_svm_training_samples_;
};


#endif /* N2_INFERENCE_GRAPH_INFERENCE_H_ */
