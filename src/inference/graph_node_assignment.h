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

#ifndef INFERENCE_GRAPH_NODE_ASSIGNMENT_H_
#define INFERENCE_GRAPH_NODE_ASSIGNMENT_H_

#include "base.h"
#include "graph_inference.h"
#include "graph_query.h"

struct Assignment {
  Assignment() : must_infer(false), label(-1) {}
  bool must_infer;
  int label;
};

struct LabelPenalty {
  LabelPenalty() : label(-2), penalty(0) {}
  int label;
  double penalty;
};


class GraphNodeAssignment : public Nice2Assignment {
public:
  GraphNodeAssignment(const GraphQuery* query, LabelSet* label_set) ;
  virtual ~GraphNodeAssignment();

  virtual void SetUpEqualityPenalty(double penalty) override;

  virtual void PrintInferredAssignments() override;

  virtual void ClearPenalty() override;

  virtual void FromJSON(const Json::Value& assignment) override;

  virtual void ToJSON(Json::Value* assignment) const override;

  virtual void ClearInferredAssignment() override;

  virtual void CompareAssignments(const Nice2Assignment* reference, PrecisionStats* stats) const override;

  virtual void CompareAssignmentErrors(const Nice2Assignment* reference, SingleLabelErrorStats* error_stats) const override;

  std::string DebugString() const;
  const char* GetLabelName(int label_id) const;

  // Returns the penalty associated with a node and its label (used in Max-Margin training).
  double GetNodePenalty(int node) const;

  // Gets the score contributed by all arcs adjacent to a node.
  double GetNodeScore(const GraphInference& fweights, int node) const;

  // Gets the node score given an assignment
  double GetNodeScoreGivenAssignmentToANode(const GraphInference& fweights, int node, int node_assigned, int node_assignment) const;

  double GetNodeScoreOnAssignedNodes(
      const GraphInference& fweights, int node,
      const std::vector<bool>& assigned) const;

  bool HasDuplicationConflictsAtNode(int node) const;

  // Returns the node with a duplication conflict to the current node. Returns -1 if there is no such node or there are multiple such nodes.
  int GetNodeWithDuplicationConflict(int node) const;

  // Gets the score connecting a pair of nodes.
  double GetNodePairScore(const GraphInference& fweights, int node1, int node2, int label1, int label2) const;

  int GetNumAdjacentArcs(int node) const;

  void GetFactorCandidates(const GraphInference& fweights,
      int factor_size,
      std::vector<Factor>* candidates,
      Factor giv_labels,
      uint beam_size);

  void GetLabelCandidates(const GraphInference& fweights, int node,
      std::vector<int>* candidates, size_t beam_size) const;

  double GetTotalScore(const GraphInference& fweights) const;

  void GetAffectedFeatures(
      GraphInference::SimpleFeaturesMap* affected_features,
      double gradient_weight) const;

  void GetAffectedFactorFeatures(
      GraphInference::FactorFeaturesMap* affected_factor_features,
      double gradient_weight) const;
  // Method that given a certain node and a label, first assign that label to the given node and then add the gradient_weight
  // to every affected feature related to the given node and its neighbours
  void GetNeighboringAffectedFeatures(
      GraphInference::SimpleFeaturesMap* affected_features,
      int node,
      int label,
      double gradient_weight) const;

  void GetFactorAffectedFeaturesOfNode(
      GraphInference::FactorFeaturesMap* factor_affected_features,
      int node,
      int label,
      double gradient_weight) const;

  void InitialGreedyAssignmentPass(const GraphInference& fweights);

  void LocalPerNodeOptimizationPass(const GraphInference& fweights, size_t beam_size);

  void LocalPerNodeOptimizationPassWithDuplicateNameResolution(const GraphInference& fweights, size_t beam_size);

  void LocalPerArcOptimizationPass(const GraphInference& fweights, size_t beam_size);

  // Perform optimization based on factor features
  void LocalPerFactorOptimizationPass(const GraphInference& fweights, size_t beam_size);
  void HeapPermute(std::vector<int> v, std::unordered_set<std::vector<int>>& permutations, int n);
  void RandomPermute(std::vector<int> v, std::unordered_set<std::vector<int>>& permutations);
  uint64 CalculateFactorial(int n);

private:
  std::vector<Assignment> assignments_;
  std::vector<LabelPenalty> penalties_;

  const GraphQuery* query_;
  LabelSet* label_set_;

#ifdef GRAPH_INFERENCE_STATS
  mutable GraphInferenceStats stats_;
#endif

  friend class GraphInference;
  friend class LoopyBPInference;
};


#endif /* INFERENCE_GRAPH_NODE_ASSIGNMENT_H_ */
