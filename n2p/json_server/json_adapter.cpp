//
// Created by Oleg Ponomarev on 10/6/17.
//

#include <glog/logging.h>

#include "base/stringprintf.h"
#include "n2p/inference/graph_inference.h"

#include "json_adapter.h"

using nice2protos::Query;
using nice2protos::NBestQuery;
using nice2protos::ShowGraphQuery;
using nice2protos::InferResponse;
using nice2protos::NBestResponse;
using nice2protos::ShowGraphResponse;
using nice2protos::Feature;

namespace std {
  size_t hash<Json::Value>::operator()(const Json::Value& v) const {
    if (v.isInt()) {
      return hash<int>()(v.asInt());
    }
    if (v.isString()) {
      const char* str = v.asCString();
      size_t r = 1;
      while (*str) {
        r = (r * 17) + (*str);
        ++str;
      }
      return r;
    }
    LOG(INFO) << v;
    return 0;
  }
}

int JsonValueNumberer::ValueToNumber(const Json::Value& val) {
  auto ins = data_.insert(std::pair<Json::Value, int>(val, data_.size()));
  if (ins.second) {
    number_to_value_.push_back(val);
  }
  return ins.first->second;
}

int JsonValueNumberer::ValueToNumberOrDie(const Json::Value& val) const {
  auto it = data_.find(val);
  CHECK(it != data_.end());
  return it->second;
}

int JsonValueNumberer::ValueToNumberWithDefault(const Json::Value& val, int default_number) const {
  auto it = data_.find(val);
  if(it == data_.end()) return default_number;
  return it->second;
}

const Json::Value& JsonValueNumberer::NumberToValue(int number) const {
  return number_to_value_[number];
}

int JsonValueNumberer::size() const {
  return data_.size();
}

Query JsonAdapter::JsonToQuery(const Json::Value &json_query) {
  Query query;
  CHECK(json_query["query"].isArray());
  for (const Json::Value& arc : json_query["query"]) {
    if (arc.isMember("f2")) {
      // A factor connecting two facts (an arc).
      auto *feature = query.add_features();
      auto *bin_relation = new Feature::BinaryRelation;
      bin_relation->set_first_node(numberer_.ValueToNumber(arc["a"]));
      bin_relation->set_second_node(numberer_.ValueToNumber(arc["b"]));
      bin_relation->set_relation(arc["f2"].asCString());
      feature->set_allocated_binary_relation(bin_relation);
    }
    if (arc.isMember("cn")) {
      // A scope that lists names that cannot be assigned to the same value.
      auto *feature = query.add_features();
      auto *constraint = new Feature::InequalityConstraint;
      const Json::Value& v = arc["n"];
      if (v.isArray()) {
        std::vector<int> scope_vars;
        scope_vars.reserve(v.size());
        for (const Json::Value& item : v) {
          scope_vars.push_back(numberer_.ValueToNumber(item));
        }
        std::sort(scope_vars.begin(), scope_vars.end());
        scope_vars.erase(std::unique(scope_vars.begin(), scope_vars.end()), scope_vars.end());
        for (const auto &scope_var : scope_vars) {
          constraint->add_nodes(scope_var);
        }
      }
      feature->set_allocated_constraint(constraint);
    }
    if (arc.isMember("group")) {
      const Json::Value& v = arc["group"];
      if (v.isArray()) {
        auto *feature = query.add_features();
        auto *factor_var = new Feature::FactorVariable;
        for (const Json::Value &item : v) {
          factor_var->add_nodes(numberer_.ValueToNumber(item));
        }
        feature->set_allocated_factor_variables(factor_var);
      }
    }
  }

  for (const Json::Value& a : json_query["assign"]) {
    auto *assignment = query.add_node_assignments();
    if (a.isMember("inf")) {
      assignment->set_label(a["inf"].asCString());
      assignment->set_given(false);
    } else {
      CHECK(a.isMember("giv"));
      assignment->set_label(a["giv"].asCString());
      assignment->set_given(true);
    }
    int number = numberer_.ValueToNumberWithDefault(a.get("v", Json::Value::null), -1);
    if (number != -1) {
      assignment->set_node_index(static_cast<uint32_t>(number));
    }
  }
  return query;
}

Json::Value JsonAdapter::InferResponseToJson(const InferResponse &response) {
  Json::Value assignments = Json::Value(Json::arrayValue);
  for (const auto &assignment : response.node_assignments()) {
    Json::Value obj(Json::objectValue);
    obj["v"] = numberer_.NumberToValue(assignment.node_index());
    if (!assignment.given()) {
      obj["inf"] = Json::Value(assignment.label());
    } else {
      obj["giv"] = Json::Value(assignment.label());
    }
    assignments.append(obj);
  }
  return assignments;
}

NBestQuery JsonAdapter::JsonToNBestQuery(const Json::Value &json_query) {
  NBestQuery query;
  query.set_n(json_query["n"].asInt());
  query.set_should_infer(json_query.isMember("infer") && json_query["infer"].asBool());
  query.set_allocated_query(new Query(JsonToQuery(json_query)));
  return query;
  }

Json::Value JsonAdapter::NBestResponseToJson(const NBestResponse &response) {
  Json::Value assignments = Json::Value(Json::arrayValue);
  for (const auto &distribution : response.candidates_distributions()) {
    Json::Value node_results(Json::objectValue);
    node_results["v"] = numberer_.NumberToValue(distribution.node());
    Json::Value node_candidate(Json::arrayValue);
    for (const auto &candidate : distribution.candidates()) {
      Json::Value obj(Json::objectValue);
      obj["label"] = candidate.node_assignment().label();
      obj["score"] = candidate.score();
      node_candidate.append(obj);
    }
    node_results["candidates"] = node_candidate;
    assignments.append(node_results);
  }
  return assignments;
}

ShowGraphQuery JsonAdapter::JsonToShowGraphQuery(const Json::Value &json_query) {
  ShowGraphQuery query;
  query.set_allocated_query(new Query(JsonToQuery(json_query)));
  query.set_should_infer(json_query.isMember("infer") && json_query["infer"].asBool());
  return query;
}

Json::Value JsonAdapter::ShowGraphResponseToJson(const ShowGraphResponse &response) {
  Json::Value json_response;
  json_response["nodes"] = Json::Value(Json::arrayValue);
  Json::Value& nodes = json_response["nodes"];
  for (const auto &node : response.nodes()) {
    Json::Value json_node;
    json_node["id"] = Json::Value(StringPrintf("N%d", static_cast<int>(node.id())));
    json_node["label"] = Json::Value(node.label());
    json_node["color"] = Json::Value(node.color());
    nodes.append(json_node);
  }

  json_response["edges"] = Json::Value(Json::arrayValue);
  Json::Value& edges = json_response["edges"];
  for (const auto &edge : response.edges()) {
    Json::Value json_edge;
    json_edge["id"] = Json::Value(StringPrintf("Edge%d", edge.id()));
    json_edge["label"] = Json::Value(edge.label());
    json_edge["source"] = Json::Value(StringPrintf("N%d", edge.source()));
    json_edge["target"] = Json::Value(StringPrintf("N%d", edge.target()));
    edges.append(json_edge);
  }
  return json_response;
}
