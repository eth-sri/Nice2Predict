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

#ifndef BASE_JSONUTIL_H_
#define BASE_JSONUTIL_H_

#include "glog/logging.h"

#include "base.h"

namespace std {
  template <> struct hash<Json::Value> {
    size_t operator()(const Json::Value& v) const {
      if (v.isInt()) {
        return hash<int>()(v.asInt());
      }
      if (v.isString()) {
        const char* str = v.asCString();
        size_t r = 1;
        while (*str) {
          r = (r * 17) + (*str);
          ++str;
        }
        return r;
      }
      LOG(INFO) << v;
      return 0;
    }
  };
}

class JsonValueNumberer {
public:
  int ValueToNumber(const Json::Value& val) {
    auto ins = data_.insert(std::pair<Json::Value, int>(val, data_.size()));
    if (ins.second) {
      number_to_value_.push_back(val);
    }
    return ins.first->second;
  }

  int ValueToNumberOrDie(const Json::Value& val) const {
    auto it = data_.find(val);
    CHECK(it != data_.end());
    return it->second;
  }

  int ValueToNumberWithDefault(const Json::Value& val, int default_number) const {
    auto it = data_.find(val);
    if(it == data_.end()) return default_number;
    return it->second;
  }

  const Json::Value& NumberToValue(int number) const {
    return number_to_value_[number];
  }

  int size() const {
    return data_.size();
  }

private:
  std::unordered_map<Json::Value, int> data_;
  std::vector<Json::Value> number_to_value_;
};

#endif /* BASE_JSONUTIL_H_ */
