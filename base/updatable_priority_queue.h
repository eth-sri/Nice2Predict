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

#ifndef BASE_UPDATABLE_PRIORITY_QUEUE_H_
#define BASE_UPDATABLE_PRIORITY_QUEUE_H_

#include <map>
#include <set>
#include <utility>

template <class KeyType, class ValueType>
class UpdatablePriorityQueue {
public:
  // Set and GetValue operate on key/values.
  void SetValue(KeyType key, ValueType value) {
    if (permanently_removed_keys_.count(key) == 0) {
      RemoveKeyValueFromQueue(key, GetValue(key));
      InsertKeyValueInQueue(key, value);
    }
    value_for_key_[key] = value;
  }
  double GetValue(KeyType key) const {
    auto it = value_for_key_.find(key);
    if (it == value_for_key_.end()) return 0;
    return it->second;
  }

  // Removes a key only for
  void PermanentlyRemoveKeyFromQueue(KeyType key) {
    RemoveKeyValueFromQueue(key, GetValue(key));
    permanently_removed_keys_.insert(key);
  }
  bool IsEmpty() const {
    return keys_sorted_by_value_.empty();
  }
  int GetKeyWithMinValue() const {
    return keys_sorted_by_value_.begin()->second;
  }

private:
  void RemoveKeyValueFromQueue(KeyType key, ValueType value) {
    keys_sorted_by_value_.erase(std::pair<ValueType, KeyType>(value, key));
  }
  void InsertKeyValueInQueue(KeyType key, ValueType value) {
    keys_sorted_by_value_.insert(std::pair<ValueType, KeyType>(value, key));
  }

  std::map<KeyType, ValueType> value_for_key_;
  std::set<KeyType> permanently_removed_keys_;
  std::set<std::pair<ValueType, KeyType> > keys_sorted_by_value_;
};

#endif /* BASE_UPDATABLE_PRIORITY_QUEUE_H_ */
