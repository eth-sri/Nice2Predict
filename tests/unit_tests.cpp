/*
   Copyright 2016 Software Reliability Lab, ETH Zurich

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

#include <glog/logging.h>

#include "gtest/gtest.h"
#include "json/json.h"

#include "n2p/inference/graph_inference.h"
#include "n2p/json_server/json_adapter.h"

static const size_t mockFactorsLimit = 0;

TEST(FactorFeaturesLevelTest, NextLevelZeroEntryWhenCurrentDepthGreaterThanMaximumDepth) {
  FactorFeaturesLevel unit_under_test;
  Factor fake_factor;
  fake_factor.insert(1);
  fake_factor.insert(2);
  fake_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 5;
  int maximum_depth = 2;
  double weight = 0.5;

  std::shared_ptr<std::pair<double, Factor>> fake_factor_feature = std::make_shared<std::pair<double, Factor>>(weight, fake_factor);
  unit_under_test.InsertFactorFeature(fake_factor_feature, fake_factor, current_depth, maximum_depth, current_label, visited_labels, mockFactorsLimit);

  EXPECT_EQ(0, unit_under_test.next_level.size());
}

TEST(FactorFeaturesLevelTest, FactorFeaturesOneEntryWhenInsertingFactorFeature) {
  FactorFeaturesLevel unit_under_test;
  Factor fake_factor;
  fake_factor.insert(1);
  fake_factor.insert(2);
  fake_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 5;
  int maximum_depth = 2;
  double weight = 0.5;

  std::shared_ptr<std::pair<double, Factor>> fake_factor_feature = std::make_shared<std::pair<double, Factor>>(weight, fake_factor);
  unit_under_test.InsertFactorFeature(fake_factor_feature, fake_factor, current_depth, maximum_depth, current_label, visited_labels, mockFactorsLimit);

  EXPECT_EQ(1, unit_under_test.factor_features.size());
}

TEST(FactorFeaturesLevelTest, NextLevelCorrectNumberOfEntriesWhenOneOfTheOtherLabelWasVisited) {
  FactorFeaturesLevel unit_under_test;
  Factor fake_factor;
  fake_factor.insert(1);
  fake_factor.insert(2);
  fake_factor.insert(3);
  Factor visited_labels;
  visited_labels.insert(2);
  int current_label = 1;
  int current_depth = 1;
  int maximum_depth = 2;
  double weight = 0.5;

  std::shared_ptr<std::pair<double, Factor>> fake_factor_feature = std::make_shared<std::pair<double, Factor>>(weight, fake_factor);
  unit_under_test.InsertFactorFeature(fake_factor_feature, fake_factor, current_depth, maximum_depth, current_label, visited_labels, mockFactorsLimit);

  EXPECT_EQ(1, unit_under_test.next_level.size());
}

TEST(FactorFeaturesLevelTest, NextLevelCorrectNumberOfEntriesWithOneDuplicatedLabelInFactor) {
  FactorFeaturesLevel unit_under_test;
  Factor fake_factor;
  fake_factor.insert(1);
  fake_factor.insert(1);
  fake_factor.insert(2);
  fake_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 1;
  int maximum_depth = 2;
  double weight = 0.5;

  std::shared_ptr<std::pair<double, Factor>> fake_factor_feature = std::make_shared<std::pair<double, Factor>>(weight, fake_factor);
  unit_under_test.InsertFactorFeature(fake_factor_feature, fake_factor, current_depth, maximum_depth, current_label, visited_labels, mockFactorsLimit);

  EXPECT_EQ(3, unit_under_test.next_level.size());
}

void SetUpUnitUnderTest(const std::string &training_data_sample, GraphInference& unit_under_test, JsonAdapter &adapter) {
  Json::Reader jsonreader;
  Json::Value training_data_sample_value;
  jsonreader.parse(training_data_sample, training_data_sample_value, false);
  unit_under_test.AddQueryToModel(adapter.JsonToQuery(training_data_sample_value));
  unit_under_test.PrepareForInference();
}

void ComputePrecisionStats(std::vector<std::string> ref_data_samples,
    PrecisionStats* precision_stats,
    GraphInference &unit_under_test,
    Nice2Assignment* inferred_assignment,
    JsonAdapter &adapter) {
  Json::Reader jsonreader;
  for(auto it = ref_data_samples.begin(); it != ref_data_samples.end(); it++) {
    Json::Value ref_data_sample;
    jsonreader.parse(*it, ref_data_sample, false);
    Nice2Query* ref_query = unit_under_test.CreateQuery();
    nice2protos::Query proto_query = adapter.JsonToQuery(ref_data_sample);
    ref_query->FromFeaturesQueryProto(proto_query.features());
    Nice2Assignment* ref_assignment = unit_under_test.CreateAssignment(ref_query);
    ref_assignment->FromNodeAssignmentsProto(proto_query.node_assignments());
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

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());
  unit_under_test.MapInference(query, assignment);

  const std::string ref_data_sample = "{\"query\":[{\"a\":0,\"b\":3,\"f2\":\"mock\"}]," \
        "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
        "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesOneOfPermutationsOfFactorFeatureTest) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

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
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  EXPECT_GE(2, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesCorrectPermutationOfFactorFeatureTest) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

  unit_under_test.MapInference(query, assignment);

  const std::string ref_data_sample = "{\"query\":[{\"group\":[0,1,2,3]},{\"group\":[0,1,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesCorrectPermutationOfFactorFeatureTestGivenOneVarInf) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

  unit_under_test.MapInference(query, assignment);

  const std::string ref_data_sample = "{\"query\":[{\"group\":[1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

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

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

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
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  // Since it either 1 of the two permutations the number of errors must be less than or equal to 2
  EXPECT_GE(2, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesOneOfPermutationsOfFactorFeatureTestGivenOneGivVar) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

  unit_under_test.MapInference(query, assignment);


  const std::string ref_data_sample_first_permutation = "{\"query\":[{\"group\":[0,1,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"base\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"props\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  const std::string ref_data_sample_second_permutation = "{\"query\":[{\"group\":[0,1,2]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"props\"},{\"v\":1,\"giv\":\"AST_Node\"}," \
      "{\"v\":2,\"inf\":\"base\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample_first_permutation);
  ref_data_samples.push_back(ref_data_sample_second_permutation);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  // Since it either 1 of the two permutations the number of errors must be less than or equal to 2
  EXPECT_GE(2, precision_stats.incorrect_labels);
}

TEST(MapInferenceTest, GivesCorrectPermutationsOfFactorFeatureTestGivenDuplicateLabels) {
  const std::string training_data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"split\"},{\"v\":1,\"giv\":\"split\"}," \
      "{\"v\":2,\"inf\":\"split\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  const std::string data_sample = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"a\"},{\"v\":1,\"giv\":\"split\"}," \
      "{\"v\":2,\"inf\":\"b\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";

  JsonAdapter adapter;
  Json::Reader jsonreader;
  Json::Value data_sample_value;
  jsonreader.parse(data_sample, data_sample_value, false);
  GraphInference unit_under_test;
  SetUpUnitUnderTest(training_data_sample, unit_under_test, adapter);
  Nice2Query* query = unit_under_test.CreateQuery();
  nice2protos::Query proto_query = adapter.JsonToQuery(data_sample_value);
  query->FromFeaturesQueryProto(proto_query.features());
  Nice2Assignment* assignment = unit_under_test.CreateAssignment(query);
  assignment->FromNodeAssignmentsProto(proto_query.node_assignments());

  unit_under_test.MapInference(query, assignment);


  const std::string ref_data_sample_first_permutation = "{\"query\":[{\"group\":[0,1,2,3]}]," \
      "\"assign\":[{\"v\":0,\"inf\":\"split\"},{\"v\":1,\"giv\":\"split\"}," \
      "{\"v\":2,\"inf\":\"split\"},{\"v\":3,\"giv\":\"split\"},{\"v\":4,\"giv\":\"step\"}]}";
  std::vector<std::string> ref_data_samples;
  ref_data_samples.push_back(ref_data_sample_first_permutation);
  PrecisionStats precision_stats;
  ComputePrecisionStats(ref_data_samples, &precision_stats, unit_under_test, assignment, adapter);

  EXPECT_EQ(0, precision_stats.incorrect_labels);
}

GTEST_API_ int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  //testing::UnitTest& unit_test = *testing::UnitTest::GetInstance();
  return RUN_ALL_TESTS();

}


