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

#include "base.h"

#include <stddef.h>
#include <sys/time.h>

int64 GetCurrentTimeMicros() {
	// gettimeofday has microsecond resolution.
	struct timeval tv;
	if (gettimeofday(&tv, NULL) < 0) {
		return 0;
	}
	return (static_cast<int64>(tv.tv_sec) * 1000000) + tv.tv_usec;
}
