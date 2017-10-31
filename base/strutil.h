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

#ifndef STRUTIL_H_
#define STRUTIL_H_

#include <string>
#include <vector>
#include <set>

bool IsCharWhiteSpace(char c);

void ShortenStr(const std::string& s, size_t max_length, std::string* outstr);
std::string ShortenStr(const std::string& s, int max_length);

std::string TrimLeadingAndTrailingSpaces(const std::string& s);

void JoinStrings(const std::vector<std::string>& strs, const std::string& separator, std::string* outstr);
std::string JoinStrings(const std::vector<std::string>& strs, const std::string& separator);
std::string JoinInts(const std::vector<int>& ints);

char SplitStringFirstUsing(const::std::string& s, const std::set<char> delims, std::vector<std::string>* out);
void SplitStringUsing(const::std::string& s, char delim, std::vector<std::string>* out);

// Parses an integer. Returns the provided def value if the conversion fails.
int ParseInt32WithDefault(const std::string& s, int def);
// Parses an integer. Returns false if parse fails.
bool ParseInt32(const std::string& s, int* value);
bool ParseDouble(const std::string& s, double* value);

// Escapes string such that it does not contain separators such as space, tab, new line, commas, arithmetic expressions, etc.
std::string EscapeStrSeparators(const std::string& s);
std::string UnEscapeStrSeparators(const std::string& s);

#endif /* STRUTIL_H_ */
