/*
   Copyright 2015 Software Reliability Lab, ETH Zurich

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

#ifndef BASE_SIMPLE_HISTOGRAM_H_
#define BASE_SIMPLE_HISTOGRAM_H_

#include <vector>
#include <string>

#include "stringprintf.h"

class SimpleHistogram {
public:
  SimpleHistogram(size_t max_value) : counts_(max_value + 1, 0), total_count_(0) {
  }

  void AddCount(size_t value, int added_count) {
    if (value >= counts_.size()) value = counts_.size() - 1;
    counts_[value] += added_count;
    total_count_ += added_count;
  }

  std::string ToString() const {
    std::string result;
    StringAppendF(&result, "total: %d\n", total_count_);
    int count = counts_.size();
    for (int i = 0; i < count; ++i) {
      double ratio = (total_count_ > 0) ? (static_cast<double>(counts_[i]) / total_count_) : 0.0;
      StringAppendF(&result, "  %3d%c : %d (%3.1f%%)\n", i, (i==count-1)?'+':' ', counts_[i], ratio*100);
    }
    result.push_back('\n');
    return result;
  }

private:
  std::vector<int> counts_;
  int total_count_;
};

#endif /* BASE_SIMPLE_HISTOGRAM_H_ */
