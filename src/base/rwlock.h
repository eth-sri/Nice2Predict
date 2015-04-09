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


#ifndef BASE_RWLOCK_H_
#define BASE_RWLOCK_H_

#include <pthread.h>
#include "glog/logging.h"

class ReadLock {
public:
  ReadLock(pthread_rwlock_t* lock) :lock_(lock){
    CHECK(pthread_rwlock_rdlock(lock_) == 0);
  }
  ~ReadLock() {
    CHECK(pthread_rwlock_unlock(lock_) == 0);
  }
private:
  pthread_rwlock_t* lock_;
};

class WriteLock {
public:
  WriteLock(pthread_rwlock_t* lock) :lock_(lock){
    CHECK(pthread_rwlock_wrlock(lock_) == 0);
  }
  ~WriteLock() {
    CHECK(pthread_rwlock_unlock(lock_) == 0);
  }
private:
  pthread_rwlock_t* lock_;
};

#endif /* BASE_RWLOCK_H_ */
