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

#include <glog/logging.h>

LabelChecker::LabelChecker() : is_loaded_(false) {
  valid_labels_.set_empty_key(-1);
  valid_labels_.set_deleted_key(-2);
}

LabelChecker::~LabelChecker() {
}

void LabelChecker::Load(const std::string& filename, const StringSet* ss) {
  LoadRules(filename);
  LOG(INFO) << "Loaded rules";
  ApplyRulesOnAllValuesInSS(ss);
  is_loaded_ = true;
}

void LabelChecker::LoadRules(const std::string& filename) {
  rules_.clear();
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

    if (line[0] == '+') {  // Positive rule.
      rules_.push_back(CheckingRule(/* valid= */  true, line.c_str() + 1));
    } else if (line[0] == '-') {  // Negative rule.
      rules_.push_back(CheckingRule(/* valid= */  false, line.c_str() + 1));
    } else if (line[0] == '#' || line[0] == '%') {
      // Skip. Comments.
    } else {
      LOG(FATAL) << "Invalid rule: All rules must start with + or a -. Found:" << line;
    }
  }
  f.close();
}

void LabelChecker::ApplyRulesOnAllValuesInSS(const StringSet* ss) {
  std::vector<int> strings;
  ss->getAllStrings(&strings);

  for (const CheckingRule& rule : rules_) {
    if (IsRegEx(rule.re_str_.c_str())) {
      // For each label that matches the RE, set its validity according to the rule.
      for (int label : strings) {
        const char* strbegin = ss->getString(label);
        const char* strend = strbegin + strlen(strbegin);
        if (strend - strbegin > 100) {
          valid_labels_[label] = false;
          continue;
        }
        if (std::regex_match(strbegin, strend, rule.re_)) {
          valid_labels_[label] = rule.valid_;
        }
      }
    } else {
      // Process as a single label.
      int label = ss->findString(rule.re_str_.c_str());
      if (label >= 0) {
        valid_labels_[label] = rule.valid_;
      }
    }
  }
}

bool LabelChecker::IsStringLabelValid(const char* s) const {
  const char* s_end = s + strlen(s);
  bool valid = true;
  for (const CheckingRule& rule : rules_) {
    if (IsRegEx(rule.re_str_.c_str())) {
      if (std::regex_match(s, s_end, rule.re_)) {
        valid = rule.valid_;
      }
    } else {
      if (rule.re_str_ == s) {
        valid = rule.valid_;
      }
    }
  }
  return valid;
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
