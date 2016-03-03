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

#ifndef BASE_MAPUTIL_H_
#define BASE_MAPUTIL_H_

template<class Collection, class Key, class Value>
const Value& FindWithDefault(const Collection& c, const Key& key, const Value& default_value) {
  auto it = c.find(key);
  if (it == c.end()) return default_value;
  return it->second;
}


// A pair data structure with the corresponding hashes.

struct IntPair {
  IntPair() : first(0), second(0) {}
  IntPair(int a, int b) : first(a), second(b) {}

  bool operator<(const IntPair& o) const {
    if (first == o.first) return first < o.first;
    return second < o.second;
  }

  bool operator==(const IntPair& o) const {
    return first == o.first && second == o.second;
  }

  int first;
  int second;
};

namespace std {
  template <> struct hash<IntPair> {
    size_t operator()(const IntPair& x) const {
      return x.first * 6037 + x.second;
    }
  };
}


#endif /* BASE_MAPUTIL_H_ */
