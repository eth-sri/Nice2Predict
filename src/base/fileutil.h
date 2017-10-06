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

#ifndef BASE_FILEUTIL_H_
#define BASE_FILEUTIL_H_

#include <stdio.h>
#include <string>

#include "glog/logging.h"

void ReadFileToStringOrDie(const char* filename, std::string* r);
void WriteStringToFileOrDie(const char* filename, const std::string& s);
bool FileExists(const char* filename);

#endif /* BASE_FILEUTIL_H_ */
