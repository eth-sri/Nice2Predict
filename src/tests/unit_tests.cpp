/*
 * inference_test.cpp
 *
 *  Created on: May 13, 2016
 *      Author: matteo
 */
#define GTEST_HAS_TR1_TUPLE 0

#include "gtest/gtest.h"

#include "base.h"
#include "graph_inference.h"

#include "factor_features_level_test.h"

GTEST_API_ int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  //testing::UnitTest& unit_test = *testing::UnitTest::GetInstance();
  return RUN_ALL_TESTS();

}


