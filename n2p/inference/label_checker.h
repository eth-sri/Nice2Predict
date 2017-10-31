/*
 * label_checker.h
 *
 *  Created on: Dec 2, 2014
 *      Author: veselin
 */

#ifndef INFERENCE_LABEL_CHECKER_H_
#define INFERENCE_LABEL_CHECKER_H_

#include <regex>

#include "google/dense_hash_map"

#include "base/stringset.h"
#include "base/maputil.h"

// Utility class to check labels if they are in a valid range.

class LabelChecker {
public:
  LabelChecker();
  virtual ~LabelChecker();

  bool IsLoaded() const {
    return is_loaded_;
  }

  void Load(const std::string& filename, const StringSet* ss);

  // Returns if a label is valid.
  bool IsLabelValid(int label) const {
    return FindWithDefault(valid_labels_, label, false);
  }

  bool IsStringLabelValid(const char* s) const;

private:
  void LoadRules(const std::string& filename);
  void ApplyRulesOnAllValuesInSS(const StringSet* ss);

  bool IsRegEx(const char* c) const;

  struct CheckingRule {
    CheckingRule(bool allow, const char* re_str) : valid_(allow), re_str_(re_str), re_(re_str) { }
    bool valid_;
    std::string re_str_;
    std::regex re_;
  };

  std::vector<CheckingRule> rules_;

  google::dense_hash_map<int, bool> valid_labels_;
  bool is_loaded_;
};


#endif /* INFERENCE_LABEL_CHECKER_H_ */
