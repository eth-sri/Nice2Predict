/*
   Copyright 2013 Software Reliability Lab, ETH Zurich

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

#ifndef BASE_H_
#define BASE_H_

#include <stddef.h>

typedef long long int64;
typedef unsigned long long uint64;

int64 GetCurrentTimeMicros();

inline unsigned FingerprintCat(unsigned a, unsigned b) {
  return a * 6037 + ((b * 17) ^ (b >> 16));
}

inline size_t FingerprintMem(const void* memory, unsigned size) {
  size /= sizeof(uint64);
  const uint64* mem = static_cast<const uint64*>(memory);
  size_t r = 0;
  for (unsigned i = 0; i < size; ++i) {
    uint64 tmp = mem[i];
    r = r * 6037 + ((tmp * 19) ^ (tmp >> 48));
  }
  return r;
}

#endif /* BASE_H_ */
