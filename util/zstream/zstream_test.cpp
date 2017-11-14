/*
   Copyright 2017 DeepCode GmbH

   Author: Veselin Raychev
 */


#include "zstream.h"

#include "base/stringprintf.h"
#include "base/stringset.h"
#include "glog/logging.h"
#include <sys/stat.h>
#include <iostream>

#include "gtest/gtest.h"

TEST(ZLibTest, TestCompression) {
  std::string gzip_filename = testing::TempDir() + "/testtemp1.gz";

  std::string teststr1 = "This is a test line string.";
  std::string teststr2 = "This is another line string.";
  {
    zstr::ofstream outfile(gzip_filename);
    outfile << teststr1 << "\n";
    for (size_t i = 0; i < 50; ++i) {
      outfile << teststr2 << "\n";
    }
  }

  struct stat st;
  stat(gzip_filename.c_str(), &st);
  size_t filesize = st.st_size;
  EXPECT_LT(filesize, teststr1.size() + 2 * teststr2.size());

  {
    std::string line;
    zstr::ifstream infile(gzip_filename);
    std::getline(infile, line);
    EXPECT_EQ(line, teststr1);
    for (size_t i = 0; i < 50; ++i) {
      std::getline(infile, line);
      EXPECT_EQ(line, teststr2);
    }
  }
}

int main(int argc, char **argv) {
  google::InstallFailureSignalHandler();
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
