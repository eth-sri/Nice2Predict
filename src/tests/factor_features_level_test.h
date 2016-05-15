/*
 * factor_features_level_test.h
 *
 *  Created on: May 15, 2016
 *      Author: matteo
 */

#ifndef TESTS_FACTOR_FEATURES_LEVEL_TEST_H_
#define TESTS_FACTOR_FEATURES_LEVEL_TEST_H_

#define GTEST_HAS_TR1_TUPLE 0

#include "gtest/gtest.h"

#include "base.h"
#include "graph_inference.h"

TEST(FactorFeaturesLevelTest, NextLevelZeroEntryWhenCurrentDepthGreaterThanMaximumDepth) {
  FactorFeaturesLevel unit_under_test;
  Factor mock_factor;
  mock_factor.insert(1);
  mock_factor.insert(2);
  mock_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 5;
  int maximum_depth = 2;
  double weight = 0.5;

  unit_under_test.InsertFactorFeature(weight, mock_factor, current_depth, maximum_depth, current_label, visited_labels);

  EXPECT_EQ(0, unit_under_test.next_level.size());
}

TEST(FactorFeaturesLevelTest, FactorFeaturesOneEntryWhenInsertingFactorFeature) {
  FactorFeaturesLevel unit_under_test;
  Factor mock_factor;
  mock_factor.insert(1);
  mock_factor.insert(2);
  mock_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 5;
  int maximum_depth = 2;
  double weight = 0.5;

  unit_under_test.InsertFactorFeature(weight, mock_factor, current_depth, maximum_depth, current_label, visited_labels);

  EXPECT_EQ(1, unit_under_test.factor_features.size());
}

TEST(FactorFeaturesLevelTest, NextLevelCorrectNumberOfEntriesWhenOneOfTheOtherLabelWasVisited) {
  FactorFeaturesLevel unit_under_test;
  Factor mock_factor;
  mock_factor.insert(1);
  mock_factor.insert(2);
  mock_factor.insert(3);
  Factor visited_labels;
  visited_labels.insert(2);
  int current_label = 1;
  int current_depth = 1;
  int maximum_depth = 2;
  double weight = 0.5;

  unit_under_test.InsertFactorFeature(weight, mock_factor, current_depth, maximum_depth, current_label, visited_labels);

  EXPECT_EQ(1, unit_under_test.next_level.size());
}

TEST(FactorFeaturesLevelTest, NextLevelCorrectNumberOfEntriesWithOneDuplicatedLabelInFactor) {
  FactorFeaturesLevel unit_under_test;
  Factor mock_factor;
  mock_factor.insert(1);
  mock_factor.insert(1);
  mock_factor.insert(2);
  mock_factor.insert(3);
  Factor visited_labels;
  int current_label = 1;
  int current_depth = 1;
  int maximum_depth = 2;
  double weight = 0.5;

  unit_under_test.InsertFactorFeature(weight, mock_factor, current_depth, maximum_depth, current_label, visited_labels);

  EXPECT_EQ(3, unit_under_test.next_level.size());
}


#endif /* TESTS_FACTOR_FEATURES_LEVEL_TEST_H_ */
