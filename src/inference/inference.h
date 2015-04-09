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
#include "jsoncpp/json/json.h"

// Abstract classes for inference.


// A single query to be asked.
class Nice2Query {
public:
  virtual ~Nice2Query();

  virtual void FromJSON(const Json::Value& query) = 0;
};

struct PrecisionStats {
  PrecisionStats() : correct_labels(0), incorrect_labels(0) {}
  void AddStats(const PrecisionStats& o) {
    correct_labels += o.correct_labels;
    incorrect_labels += o.incorrect_labels;
  }

  int correct_labels;
  int incorrect_labels;

  std::mutex lock;
};

struct SingleLabelErrorStats {
  std::map<std::string, int> errors_and_counts;
  std::mutex lock;
};

// Assigned results for the query (including the pre-assigned values).
class Nice2Assignment {
public:
  virtual ~Nice2Assignment();

  // Used for maximum margin training.
  virtual void SetUpEqualityPenalty(double penalty) = 0;
  virtual void ClearPenalty() = 0;

  virtual void FromJSON(const Json::Value& assignment) = 0;
  virtual void ToJSON(Json::Value* assignment) const = 0;

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

  // Initializes SSVM learning.
  virtual void SSVMInit(double regularization, double margin) = 0;

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

  // All queries that a SSVM should learn from must be given first with AddQueryToModel.
  // This is to ensure all the relevant features are added to the model.
  virtual void AddQueryToModel(const Json::Value& query, const Json::Value& assignment) = 0;

  // Must be called [at least] once before calling SSVMLearn or MapInference.
  virtual void PrepareForInference() = 0;


  // DEBUG methods.
  // Given a query and an assignment, return a graph to visualize the query.
  virtual void DisplayGraph(
      const Nice2Query* query,
      const Nice2Assignment* assignment,
      Json::Value* graph) const = 0;
};

#endif
