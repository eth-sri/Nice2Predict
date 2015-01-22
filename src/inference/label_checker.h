/*
 * label_checker.h
 *
 *  Created on: Dec 2, 2014
 *      Author: veselin
 */

#ifndef INFERENCE_LABEL_CHECKER_H_
#define INFERENCE_LABEL_CHECKER_H_

#include "stringset.h"
#include <google/dense_hash_map>

#include "maputil.h"

// Utility class to check labels if they are in a valid range.

class LabelChecker {
public:
  LabelChecker();
  virtual ~LabelChecker();

  bool IsLoaded() const {
    return is_loaded_;
  }

  void Load(const std::string& filename, StringSet* ss);

  // Returns if a label is valid.
  bool IsLabelValid(int label) const {
    return FindWithDefault(valid_labels_, label, false);
  }

private:
  bool IsRegEx(const char* c) const;

  google::dense_hash_map<int, bool> valid_labels_;
  bool is_loaded_;
};


#endif /* INFERENCE_LABEL_CHECKER_H_ */
