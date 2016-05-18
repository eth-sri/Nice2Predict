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

#ifndef TESTS_FACTOR_FEATURES_LEVEL_TEST_H_
#define TESTS_FACTOR_FEATURES_LEVEL_TEST_H_

#define GTEST_HAS_TR1_TUPLE 0

#include "gtest/gtest.h"

#include "base.h"
#include "graph_inference.h"

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

  unit_under_test.InsertFactorFeature(weight, fake_factor, current_depth, maximum_depth, current_label, visited_labels);

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

  unit_under_test.InsertFactorFeature(weight, fake_factor, current_depth, maximum_depth, current_label, visited_labels);

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

  unit_under_test.InsertFactorFeature(weight, fake_factor, current_depth, maximum_depth, current_label, visited_labels);

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

  unit_under_test.InsertFactorFeature(weight, fake_factor, current_depth, maximum_depth, current_label, visited_labels);

  EXPECT_EQ(3, unit_under_test.next_level.size());
}


#endif /* TESTS_FACTOR_FEATURES_LEVEL_TEST_H_ */
