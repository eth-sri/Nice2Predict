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

#include "strutil.h"

#include <utility>
#include <stdlib.h>

bool IsCharWhiteSpace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

void ShortenStr(const std::string& s, size_t max_length, std::string* outstr) {
  if (s.size() > max_length) {
    if (max_length > 3) {
      outstr->append(s.substr(0, max_length - 3));
    }
    outstr->append("...");
  } else {
    *outstr = s;
  }
}

std::string TrimLeadingAndTrailingSpaces(const std::string& s) {
  std::string r;
  size_t start = 0;
  for (; start < s.length(); ++start) {
    if (!IsCharWhiteSpace(s[start]))
      break;
  }
  size_t end = s.size();
  while (end > 0 && end > start) {
    if (!IsCharWhiteSpace(s[end - 1]))
      break;
    --end;
  }
  return s.substr(start, end - start);
}

std::string ShortenStr(const std::string& s, int max_length) {
  std::string r;
  ShortenStr(s, max_length, &r);
  return r;
}

void JoinStrings(const std::vector<std::string>& strs,
    const std::string& separator, std::string* outstr) {
  for (size_t i = 0; i < strs.size(); ++i) {
    if (i != 0)
      outstr->append(separator);
    outstr->append(strs[i]);
  }
}

std::string JoinStrings(const std::vector<std::string>& strs,
    const std::string& separator) {
  std::string r;
  JoinStrings(strs, separator, &r);
  return r;
}

std::string JoinInts(const std::vector<int>& ints) {
  std::string result;
  char intv[16];
  for (size_t i = 0; i < ints.size(); ++i) {
    if (i != 0)
      result += ' ';
    sprintf(intv, "%d", ints[i]);
    result += intv;
  }
  return result;
}

// Keep the delimiter as first character of second string
char SplitStringFirstUsing(const ::std::string& s, const std::set<char> delims,
    std::vector<std::string>* out) {
  std::vector<std::pair<int, int> > pieces;
  char matched_char = 0;
  int start = 0, len = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (delims.find(s[i]) != delims.end()) {
      pieces.push_back(std::pair<int, int>(start, len));
      matched_char = s[i];
      start = i + 1;
      len = s.size() - i - 1;
      break;
    } else {
      ++len;
    }
  }
  pieces.push_back(std::pair<int, int>(start, len));
  out->assign(pieces.size(), std::string());
  for (size_t i = 0; i < pieces.size(); ++i) {
    (*out)[i].assign(s.c_str() + pieces[i].first, pieces[i].second);
  }
  return matched_char;
}

void SplitStringUsing(const ::std::string& s, char delim,
    std::vector<std::string>* out) {
  std::vector<std::pair<int, int> > pieces;
  int start = 0, len = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == delim) {
      pieces.push_back(std::pair<int, int>(start, len));
      start = i + 1;
      len = 0;
    } else {
      ++len;
    }
  }
  pieces.push_back(std::pair<int, int>(start, len));
  out->assign(pieces.size(), std::string());
  for (size_t i = 0; i < pieces.size(); ++i) {
    (*out)[i].assign(s.c_str() + pieces[i].first, pieces[i].second);
  }
}

int ParseInt32WithDefault(const std::string& s, int def) {
  if (s.size() == 0)
    return def;
  char* endptr;
  int r = strtol(s.c_str(), &endptr, 0);
  if (endptr == s.c_str() + s.size())
    return r;
  return def;
}

bool ParseInt32(const std::string& s, int* value) {
  if (s.size() == 0)
    return false;
  char* endptr;
  int r = strtol(s.c_str(), &endptr, 0);
  if (endptr == s.c_str() + s.size()) {
    *value = r;
    return true;
  }
  return false;
}

bool ParseDouble(const std::string& s, double* value) {
  if (s.size() == 0) return false;
  char* endptr;
  double r = strtod(s.c_str(), &endptr);
  if (endptr == s.c_str() + s.size()) {
      *value = r;
      return true;
  }
  return false;
}


std::string EscapeStrSeparators(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    switch (s[i]) {
    case ',': result += "\\c"; break;
    case ' ': result += "\\s"; break;
    case '\n': result += "\\n"; break;
    case '\t': result += "\\t"; break;
    case '\\': result += "\\\\"; break;
    case '+': result += "\\p"; break;
    case '-': result += "\\m"; break;
    case '=': result += "\\e"; break;
    case '|': result += "\\o"; break;
    case '&': result += "\\a"; break;
    case '@': result += "\\x"; break;
    case ':': result += "\\f"; break;
    case ';': result += "\\r"; break;
    case '\"': result += "\\d"; break;
    case '\'': result += "\\q"; break;
    default: result += s[i];
    }
  }
  return result;
}

std::string UnEscapeStrSeparators(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\\') {
      ++i; if (i >= s.size()) break;
      switch (s[i]) {
      case 'c': result += ','; break;
      case 's': result += ' '; break;
      case 'n': result += '\n'; break;
      case 't': result += '\t'; break;
      case '\\': result += '\\'; break;
      case 'p': result += '+'; break;
      case 'm': result += '-'; break;
      case 'e': result += '='; break;
      case 'o': result += '|'; break;
      case 'a': result += '&'; break;
      case 'x': result += '@'; break;
      case 'f': result += ':'; break;
      case 'r': result += ';'; break;
      case 'd': result += '\"'; break;
      case 'q': result += '\''; break;
      }
    } else result += s[i];
  }
  return result;
}
