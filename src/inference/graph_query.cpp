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

#include "graph_query.h"

GraphQuery::GraphQuery(const StringSet* ss, const LabelChecker* checker) : label_set_(ss, checker) {
  arcs_connecting_node_pair_.set_empty_key(IntPair(-1, -1));
  arcs_connecting_node_pair_.set_deleted_key(IntPair(-2, -2));
}
GraphQuery::~GraphQuery() {
}

void GraphQuery::FromJSON(const Json::Value& query) {
  arcs_.clear();
  factors_.clear();

  CHECK(query.isArray());
  for (const Json::Value& arc : query) {
    if (arc.isMember("f2")) {
      // A factor connecting two facts (an arc).
      Arc a;
      a.node_a = numberer_.ValueToNumber(arc["a"]);
      a.node_b = numberer_.ValueToNumber(arc["b"]);
      a.type = label_set_.ss()->findString(arc["f2"].asCString());
      if (a.type < 0) continue;
      arcs_.push_back(a);
    }
    if (arc.isMember("cn")) {
      // A scope that lists names that cannot be assigned to the same value.
      const Json::Value& v = arc["n"];
      if (v.isArray()) {
        std::vector<int> scope_vars;
        scope_vars.reserve(v.size());
        for (const Json::Value& item : v) {
          scope_vars.push_back(numberer_.ValueToNumber(item));
        }
        std::sort(scope_vars.begin(), scope_vars.end());
        scope_vars.erase(std::unique(scope_vars.begin(), scope_vars.end()), scope_vars.end());
        nodes_in_scope_.push_back(std::move(scope_vars));
      }
    }
    if (arc.isMember("group")) {
      const Json::Value& v = arc["group"];
      if (v.isArray()) {
        Factor factor_vars;
        for (const Json::Value& item : v) {
          factor_vars.insert(numberer_.ValueToNumber(item));
        }
        factors_.push_back(factor_vars);
      }
    }

  }
  std::sort(arcs_.begin(), arcs_.end());
  arcs_adjacent_to_node_.assign(numberer_.size(), std::vector<Arc>());
  for (const Arc& a : arcs_) {
    arcs_adjacent_to_node_[a.node_a].push_back(a);
    arcs_adjacent_to_node_[a.node_b].push_back(a);
  }
  for (std::vector<Arc>& v : arcs_adjacent_to_node_) {
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
  }

  arcs_connecting_node_pair_.clear();
  for (const Arc& a : arcs_) {
    arcs_connecting_node_pair_[IntPair(a.node_a, a.node_b)].push_back(a);
    arcs_connecting_node_pair_[IntPair(a.node_b, a.node_a)].push_back(a);
  }

  scopes_per_nodes_.assign(numberer_.size(), std::vector<int>());
  for (size_t scope = 0; scope < nodes_in_scope_.size(); ++scope) {
    for (int node : nodes_in_scope_[scope]) {
      scopes_per_nodes_[node].push_back(scope);
    }
  }

  factors_of_a_node_.assign(numberer_.size(), std::vector<Factor>());
  for (uint i = 0; i < factors_.size(); i++) {
    for (auto var = factors_[i].begin(); var != factors_[i].end(); var++) {
      Factor f;
      for (auto v = factors_[i].begin(); v != factors_[i].end(); v++) {
        if (*var != *v) {
          f.insert(*v);
        }
      }
      factors_of_a_node_[*var].push_back(f);
    }
  }
}


