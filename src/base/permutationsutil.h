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
