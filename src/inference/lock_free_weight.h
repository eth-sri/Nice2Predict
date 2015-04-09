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

#ifndef INFERENCE_LOCK_FREE_WEIGHT_H_
#define INFERENCE_LOCK_FREE_WEIGHT_H_

#include <atomic>

// A structure to store a score, lock-free.
struct LockFreeWeights {
  LockFreeWeights() : value(0) {}
  LockFreeWeights(const LockFreeWeights& o) : value(o.getValue()) {}
  double getValue() const {
    return value.load();
  }
  void setValue(double v) {
    value.store(v);
  }
  void nonAtomicAdd(double value_added) {
    value.store(value.load() + value_added);
  }
  void atomicAdd(double value_added) {
    double expected = value.load();
    double desired;
    do {
      desired = expected + value_added;
    } while (!value.compare_exchange_weak(expected, desired));
  }
  void atomicAddRegularized(double value_added, double min, double max) {
    double expected = value.load();
    double desired;
    do {
      desired = expected + value_added;
      // L_inf regularize the new value.
      if (desired < min) desired = min;
      if (desired > max) desired = max;
    } while (!value.compare_exchange_weak(expected, desired));
    // If CAS fails, expected is updated with the latest value from heap.
  }

private:
  std::atomic<double> value;
};


#endif /* INFERENCE_LOCK_FREE_WEIGHT_H_ */
