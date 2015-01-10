/*
 * simple_histogram.h
 *
 *  Created on: Dec 3, 2014
 *      Author: veselin
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
