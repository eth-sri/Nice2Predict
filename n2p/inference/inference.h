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

#ifndef N2_INFERENCE_H__
#define N2_INFERENCE_H__

#include <mutex>
#include <string>
#include <map>

#include "n2p/protos/interface.pb.h"

typedef ::google::protobuf::RepeatedPtrField<nice2protos::Feature> FeaturesQuery;
typedef ::google::protobuf::RepeatedPtrField<nice2protos::NodeAssignment> NodeAssignments;

// Abstract classes for inference.

// A single query to be asked.
class Nice2Query {
public:
  virtual ~Nice2Query();

  virtual void FromFeaturesQueryProto(const FeaturesQuery &query) = 0;
};

struct PrecisionStats {
  PrecisionStats() : correct_labels(0), incorrect_labels(0), num_known_predictions(0) {}
  void AddStats(const PrecisionStats& o) {
    correct_labels += o.correct_labels;
    incorrect_labels += o.incorrect_labels;
    num_known_predictions += o.num_known_predictions;
  }

  int correct_labels;  // When making an unknown prediction, this is considered an incorrect label.
  int incorrect_labels;

  // Only includes labels that were predicted not be unknown.
  int num_known_predictions;

  std::mutex lock;
};

struct SingleLabelErrorStats {
  std::map<std::string, int> errors_and_counts;
  std::mutex lock;
};

// Forward declaration
class Nice2Inference;

// Assigned results for the query (including the pre-assigned values).
class Nice2Assignment {
public:
  virtual ~Nice2Assignment();

  // Used for maximum margin training.
  virtual void SetUpEqualityPenalty(double penalty) = 0;
  virtual void ClearPenalty() = 0;

  virtual void FromNodeAssignmentsProto(const NodeAssignments &property) = 0;
  virtual void FillInferResponse(nice2protos::InferResponse* response) const = 0;

  virtual void GetNBestCandidates(
      Nice2Inference* inference,
      const int n,
      nice2protos::NBestResponse* response) = 0;

  // Deletes all labels that must be inferred (does not affect the given known labels).
  virtual void ClearInferredAssignment() = 0;
  // Compare two assignments (it is assumed the two assignments are for the same Nice2Query).
  virtual void CompareAssignments(const Nice2Assignment* reference, PrecisionStats* stats) const = 0;
  // Compare two assignments and return the label errors observed in them.
  virtual void CompareAssignmentErrors(const Nice2Assignment* reference, SingleLabelErrorStats* error_stats) const = 0;
};


class Nice2Inference {
public:
  virtual ~Nice2Inference();

  virtual void LoadModel(const std::string& file_prefix) = 0;
  virtual void SaveModel(const std::string& file_prefix) = 0;

  virtual Nice2Query* CreateQuery() const = 0;
  virtual Nice2Assignment* CreateAssignment(Nice2Query* query) const = 0;

  // Updates assignment with the most likely assignment (assignment with highest score).
  // Must be called after PrepareForInference
  virtual void MapInference(
      const Nice2Query* query,
      Nice2Assignment* assignment) const = 0;

  // Gets the score of a given assignment.
  virtual double GetAssignmentScore(const Nice2Assignment* assignment) const = 0;

  // Method containing common initializations between the different learning algorithms
  virtual void InitializeFeatureWeights(double regularization) = 0;

  // Initializes SSVM learning.
  virtual void SSVMInit(double margin) = 0;

  // Initializes PL learning
  virtual void PLInit(int beam_size) = 0;

  // Train on a single query + assignment of properties.
  // PrepareForInference and SSVMInit must have been called before SSVMLearn.
  //
  // Implementations of this method (and only this method) are typically thread-safe
  // allowing for multiple training data instances to be processed at once (via Hogwild training).
  virtual void SSVMLearn(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      double learning_rate,
      PrecisionStats* stats) = 0;


  // This method executes a training based on the optimization of the pseudolikelihood
  virtual void PLLearn(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      double learning_rate) = 0;

  // All queries that a SSVM should learn from must be given first with AddQueryToModel.
  // This is to ensure all the relevant features are added to the model.
  virtual void AddQueryToModel(const nice2protos::Query &query) = 0;

  // Must be called [at least] once before calling SSVMLearn or MapInference.
  virtual void PrepareForInference() = 0;

  // Given a query and an assignment, return a graph to visualize the query.

  virtual void FillGraphProto(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      nice2protos::ShowGraphResponse* graph) const = 0;


};

#endif
