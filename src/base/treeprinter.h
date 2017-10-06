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

#ifndef BASE_TREEPRINTER_H_
#define BASE_TREEPRINTER_H_

#include <string>
#include <vector>
#include <set>

#include "glog/logging.h"

#include "base/stringprintf.h"
#include "base/strutil.h"
#include "base/termcolor.h"

/* Utility class for pretty printing a tree
 * Sample usage:
 * TreePrinter tp;
 * tp.BeginNode("1");
 *   tp.BeginNode("2"); tp.EndNode();
 *   tp.BeginNode("3"); tp.EndNode();
 * tp.EndNode();
 * tp.Print();
 *
 */

class TreePrinter {
public:
  TreePrinter() : position_(-1) {}

  void BeginNode(std::string header, std::string desc = "") {
    Node n = {header, desc, position_, {}};
    nodes_.emplace_back(n);
    int new_position = static_cast<int>(nodes_.size()) - 1;
    if (position_ != -1){
      nodes_[position_].children_.insert(new_position);
    }
    position_ = new_position;
  }

  void EndNode(std::string footer = "") {
    if (!footer.empty()) {
      StringAppendF(&nodes_[position_].desc_, "%c%s", nodes_[position_].desc_.empty() ? ' ' : '\n' , footer.c_str());
    }
    position_ = nodes_[position_].parent_;
  }

  void UpdateNodeDescription(std::string desc) {
    nodes_[position_].desc_.append("\n" + desc);
  }

  void GoToChild(int child_id) {
    CHECK(child_id >= 0 && child_id < static_cast<int>(nodes_[position_].children_.size()));


    for (int child : nodes_[position_].children_){
      if (child_id == 0) {
        position_ = child;
        return;
      }
      child_id--;
    }
  }

  void Print() const {
    if (nodes_.empty()) return;

    std::string s = NodeToString(0);
    std::vector<std::string> lines;
    SplitStringUsing(s, '\n', &lines);
    for (const auto& line : lines) {
      LOG(INFO) << line;
    }
  }

private:
  struct Node {
    std::string header_;
    std::string desc_;
    int parent_;
    std::set<int> children_;
  };

  std::string NodeToString(int position, int depth = 0) const {
    std::string s;
    const Node& n = nodes_[position];

    if (position == position_) {
      s.append(std::string(HighlightColors::GREEN));
    }

    StringAppendF(&s, "##%7s\n", n.header_.substr(0, 6).c_str());

    if (position == position_) {
      s.append(std::string(HighlightColors::DEFAULT));
    }

    if (!n.desc_.empty()) {
      std::string indent((depth) * 10, ' ');
      std::vector<std::string> parts;
      SplitStringUsing(n.desc_, '\n', &parts);
      for (const auto& p : parts) {
        StringAppendF(&s, "%s  %s\n", indent.c_str(), p.c_str());
      }
    }

    std::string indent((depth + 1) * 10, ' ');
    bool first = true;
    for (int child : n.children_) {
      if (!first) {
        StringAppendF(&s, "%s#\n", indent.c_str());
      }
      StringAppendF(&s, "%s", indent.c_str());
      StringAppendF(&s, "%s\n", NodeToString(child, depth + 1).c_str());
      first = false;
    }

    s.pop_back();
    return s;
  }

  int position_;
  std::vector<Node> nodes_;
};


#endif /* BASE_TREEPRINTER_H_ */
