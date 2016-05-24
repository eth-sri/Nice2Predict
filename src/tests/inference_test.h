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

#ifndef TESTS_INFERENCE_TEST_H_
#define TESTS_INFERENCE_TEST_H_

#include "gtest/gtest.h"

#include "graph_inference.h"
#include "base.h"
#include "label_set.h"

void SetUpUnitUnderTest(const std::string training_data_sample, GraphInference& unit_under_test) {
  Json::Reader jsonreader;
  Json::Value training_data_sample_value;
  jsonreader.parse(training_data_sample, training_data_sample_value, false);
  unit_under_test.AddQueryToModel(training_data_sample_value["query"], training_data_sample_value["assign"]);
  unit_under_test.PrepareForInference();
}

void ComputePrecisionStats(std::vector<std::string> ref_data_samples,
    PrecisionStats* precision_stats,
    GraphInference unit_under_test,
    Nice2Assignment* inferred_assignment) {
  Json::Reader jsonreader;
  for(auto it = ref_data_samples.begin(); it != ref_data_samples.end(); it++) {
    Json::Value ref_data_sample;
    jsonreader.parse(*it, ref_data_sample, false);
    Nice2Query* ref_query = unit_under_test.CreateQuery();
    ref_query->FromJSON(ref_data_sample["query"]);
    Nice2Assignment* ref_assignment = unit_under_test.CreateAssignment(ref_query);
    ref_assignment->FromJSON(ref_data_sample["assign"]);
    inferred_assignment->CompareAssignments(ref_assignment, precision_stats);
  }
}

TEST(MapInferenceTest, GivesCorrectAssignmentWithPairwiseFeature) {
  const std::string training_data_sample = "{\"query\":[{\"a\":0,\"b\":3,\"f2\":\"mock\"}]," \
        "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
        "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"a\":0,\"b\":3,\"f2\":\"mock\"}]," \
        "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
        "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test);
  Nice2Query* query = unit_under_test.CreateQuery();
  query->FromJSON(data_sample_value["query"]);
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromJSON(data_sample_value["assign"]);
  unit_under_test.MapInference(query, assignment);

  const std::string ref_data_sample = "{\"query\":[{\"a\":0,\"b\":3,\"f2\":\"mock\"}]," \
        "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
        "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment);

  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesOneOfPermutationsOfFactorFeatureTest) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test);
  Nice2Query* query = unit_under_test.CreateQuery();
  query->FromJSON(data_sample_value["query"]);
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromJSON(data_sample_value["assign"]);

  unit_under_test.MapInference(query, assignment);


  const std::string ref_data_sample_first_permutation = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  const std::string ref_data_sample_second_permutation = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"props\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"base\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample_first_permutation);
  ref_data_samples.push_back(ref_data_sample_second_permutation);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment);

  EXPECT_GE(2, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesCorrectPermutationOfFactorFeatureTest) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test);
  Nice2Query* query = unit_under_test.CreateQuery();
  query->FromJSON(data_sample_value["query"]);
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromJSON(data_sample_value["assign"]);

  unit_under_test.MapInference(query, assignment);

  const std::string ref_data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment);

  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesCorrectPermutationOfFactorFeatureTestGivenOneVarInf) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test);
  Nice2Query* query = unit_under_test.CreateQuery();
  query->FromJSON(data_sample_value["query"]);
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromJSON(data_sample_value["assign"]);

  unit_under_test.MapInference(query, assignment);


  const std::string ref_data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment);

  // Since it either 1 of the two permutations the number of errors must be less than or equal 2
  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesOneOfPermutationsOfFactorFeatureTestGivenAllInfVars) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test);
  Nice2Query* query = unit_under_test.CreateQuery();
  query->FromJSON(data_sample_value["query"]);
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromJSON(data_sample_value["assign"]);

  unit_under_test.MapInference(query, assignment);


  const std::string ref_data_sample_first_permutation = "{\"query\":[{\"group\":[0,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  const std::string ref_data_sample_second_permutation = "{\"query\":[{\"group\":[0,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"props\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"base\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample_first_permutation);
  ref_data_samples.push_back(ref_data_sample_second_permutation);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment);

  // Since it either 1 of the two permutations the number of errors must be less than or equal to 2
  EXPECT_GE(2, precision_stats.incorrect_labels);
}

#endif /* TESTS_INFERENCE_TEST_H_ */
