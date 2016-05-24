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

#ifndef INFERENCE_GRAPH_QUERY_H_
#define INFERENCE_GRAPH_QUERY_H_

#include <google/dense_hash_map>

#include "gflags/gflags.h"

#include "base.h"
#include "label_checker.h"
#include "label_set.h"
#include "inference.h"
#include "jsonutil.h"
#include "graph_inference.h"

struct Arc {
  int node_a, node_b, type;
  bool operator==(const Arc& o) const {
    return node_a == o.node_a && node_b == o.node_b && type == o.type;
  }
  bool operator<(const Arc& o) const {
    if (node_a != o.node_a) return node_a < o.node_a;
    if (node_b != o.node_b) return node_b < o.node_b;
    return type < o.type;
  }
};

class GraphQuery : public Nice2Query {
public:
  explicit GraphQuery(const StringSet* ss, const LabelChecker* checker);
  virtual ~GraphQuery();

  virtual void FromJSON(const Json::Value& query) override;

private:
  std::vector<std::vector<Arc> > arcs_adjacent_to_node_;
  std::vector<std::vector<Factor> > factors_of_a_node_;
  std::vector<Arc> arcs_;
  std::vector<Factor> factors_;
  google::dense_hash_map<IntPair, std::vector<Arc> > arcs_connecting_node_pair_;
  JsonValueNumberer numberer_;

  LabelSet label_set_;

  std::vector<std::vector<int> > nodes_in_scope_;
  std::vector<std::vector<int> > scopes_per_nodes_;

  friend class GraphNodeAssignment;
  friend class LoopyBPInference;
  friend class GraphInference;
};

#endif /* INFERENCE_GRAPH_QUERY_H_ */
