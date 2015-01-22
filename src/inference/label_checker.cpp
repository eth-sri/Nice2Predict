/*
 * label_checker.cpp
 *
 *  Created on: Dec 2, 2014
 *      Author: veselin
 */


#include "label_checker.h"

#include <stdlib.h>
#include <regex>
#include <fstream>

#include "glog/logging.h"

LabelChecker::LabelChecker() : is_loaded_(false) {
  valid_labels_.set_empty_key(-1);
  valid_labels_.set_deleted_key(-2);
}

LabelChecker::~LabelChecker() {
}

void LabelChecker::Load(const std::string& filename, StringSet* ss) {
  is_loaded_ = true;
  std::vector<int> strings;
  ss->getAllStrings(&strings);

  valid_labels_.clear();

  std::ifstream f(filename);
  if (!f.good()) {
    LOG(FATAL) << "Could not open " << filename;
  }
  while (!f.eof()) {
    std::string line;
    std::getline(f, line);
    // Erase training newline symbols (if any).
    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r')) {
      line.erase(line.begin() + line.size() - 1, line.end());
    }
    if (line.empty()) continue;
    if (line[0] == '+') {
      if (IsRegEx(line.c_str() + 1)) {
        // Add regex.
        std::regex re(line.c_str() + 1);
        for (int label : strings) {
          const char* strbegin = ss->getString(label);
          const char* strend = strbegin + strlen(strbegin);
          if (std::regex_match(strbegin, strend, re)) {
            valid_labels_[label] = true;
          }
        }
      } else {
        // Add a single label as valid.
        int label = ss->findString(line.c_str() + 1);
        if (label >= 0) {
          valid_labels_[label] = true;
        }
      }
    } else if (line[0] == '-') {
      if (IsRegEx(line.c_str() + 1)) {
        // Remove a regex.
        std::regex re(line.c_str() + 1);
        for (int label : strings) {
          const char* strbegin = ss->getString(label);
          const char* strend = strbegin + strlen(strbegin);
          if (std::regex_match(strbegin, strend, re)) {
            valid_labels_[label] = false;
          }
        }
      } else {
        // Set a single label as invalid.
        int label = ss->findString(line.c_str() + 1);
        if (label >= 0) {
          valid_labels_[label] = false;
        }
      }
    } else if (line[0] == '#' || line[0] == '%') {
      // Skip. Comments.
    } else {
      LOG(FATAL) << "Invalid rule: All rules must start with + or a -. Found:" << line;
    }
  }
  f.close();
}

bool LabelChecker::IsRegEx(const char* c) const {
  const char* curr = c;
  while (*curr) {
    // Check for metacharacters.
    if (*curr == '.' || *curr == '?' || *curr == '+' || *curr == '*' ||
        *curr == '(' || *curr == ')' ||
        *curr == '[' || *curr == ']' ||
        *curr == '{' || *curr == '}' ||
        *curr == '\\' || *curr == '|' ||
        *curr == '$' || *curr == '^') return true;
    ++curr;
  }
  return false;
}
