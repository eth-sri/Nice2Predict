/*
   Copyright 2016 Software Reliability Lab, ETH Zurich

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

#ifndef BASE_PERMUTATIONSUTIL_H_
#define BASE_PERMUTATIONSUTIL_H_

#include <limits>

// permutations_beam_size is the maximum number of permutations that the method returns
void ComputeAllPermutations(std::vector<int> v,
    std::unordered_set<std::vector<int>>* permutations,
    int n,
    size_t permutations_beam_size) {
  std::vector<size_t> count;
  count.assign(v.size(), 0);
  permutations->insert(v);
  for (size_t i = 1; i < v.size();) {
    if (count[i] < i) {
      int swap = i % 2 * count[i];
      int tmp = v[swap];
      v[swap] = v[i];
      v[i] = tmp;
      permutations->insert(v);
      if (permutations->size() > permutations_beam_size) {
        break;
      }
      count[i]++;
      i = 1;
    } else {
      count[i++] = 0;
    }
  }
}

// max_num_duplicates determines the max number of random duplicate permutations after which the algorithm stops even though it did not
// computed all the require permutations. This is necessary in order to avoid that the function ends up in an infinite loop
void ComputeRandomPermutations(std::vector<int> v,
    std::unordered_set<std::vector<int>>* permutations,
    size_t permutations_beam_size,
    size_t max_num_duplicates) {
  std::random_device rd;
  std::mt19937 g(rd());
  size_t num_duplicates = 0;
  while (permutations->size() < permutations_beam_size
      && num_duplicates < max_num_duplicates) {
    std::shuffle(v.begin(), v.end(), g);
    if (permutations->count(v) == 0) {
      permutations->insert(v);
    } else {
      num_duplicates++;
    }
  }
}

// returns -1 if the result will overflow
uint64 CalculateFactorial(int n) {
  uint64 result = 1;
  for (size_t i = n; i > 0; i--) {
    // check if result will overflow
    if (result > (UINT64_MAX / i)) {
      return -1;
    }
    result *= i;
  }
  return result;
}


#endif /* BASE_PERMUTATIONSUTIL_H_ */
