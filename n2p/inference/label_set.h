/*
 * label_set.h
 *
 *  Created on: Jan 27, 2015
 *      Author: veselin
 */

#ifndef INFERENCE_LABEL_SET_H_
#define INFERENCE_LABEL_SET_H_

#include <unordered_map>
#include <utility>

#include <glog/logging.h>

#include "base/maputil.h"
#include "base/stringset.h"

// Utility class to keep a set of labels.
//
// The labels are typically strings that are mostly shared among training data
// instances (in a StringSet) or an additional set of labels specific to a new
// instance.
class LabelSet {
public:
  explicit LabelSet(const StringSet* ss, const LabelChecker* checker)
    : ss_(ss), checker_(checker), ss_size_(ss_->getSize()) {
  }

  // Adds a label by name. Returns a non-negative id for this label.
  int AddLabelName(const char* label_name) {
    int label_id = ss_->findString(label_name);
    if (label_id >= 0) return label_id;

    // The label_name was not found in the common StringSet. Look for it in the LabelSet.
    auto insertion =
        added_labels_by_name_.insert(std::pair<std::string, int>(
            label_name, added_labels_by_label_id_.size()));
    if (insertion.second == false) {
      // Label already present in the LabelSet..
      return insertion.first->second + ss_size_;  // Get its id.
    }

    added_labels_by_label_id_.push_back(label_name);
    validity_by_label_id_.push_back(checker_->IsStringLabelValid(label_name));
    return insertion.first->second + ss_size_;
  }

  // Gets a label by label_id (previously returned by AddLabelName).
  const char* GetLabelName(int label_id) const {
    if (label_id  >= ss_size_) {
      int local_index = label_id - ss_size_;
      CHECK_LT(local_index, added_labels_by_label_id_.size());
      return added_labels_by_label_id_[label_id - ss_size_].c_str();
    }
    return ss_->getString(label_id);
  }

  // Checks if a suggested label is a valid for prediction.
  bool IsLabelIdValid(int label_id) const {
    if (label_id >= ss_size_) {
      return validity_by_label_id_[label_id - ss_size_];
    }
    return checker_->IsLabelValid(label_id);
  }

  const StringSet* ss() const { return ss_; }

private:
  const StringSet* ss_;
  const LabelChecker* checker_;
  int ss_size_;
  std::unordered_map<std::string, int> added_labels_by_name_;
  std::vector<std::string> added_labels_by_label_id_;
  std::vector<bool> validity_by_label_id_;
};


#endif /* INFERENCE_LABEL_SET_H_ */
