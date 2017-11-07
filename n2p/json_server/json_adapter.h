#ifndef NICE2PREDICT_JSON_ADAPTER_H
#define NICE2PREDICT_JSON_ADAPTER_H

#include <memory>
#include <unordered_map>

#include "json/json.h"
#include "n2p/protos/service.pb.h"

namespace std {
  template <> struct hash<Json::Value> {
      size_t operator()(const Json::Value& v) const;
  };
}

class JsonValueNumberer {
 public:
  int ValueToNumber(const Json::Value& val);
  int ValueToNumberOrDie(const Json::Value& val) const;
  int ValueToNumberWithDefault(const Json::Value& val, int default_number) const;
  const Json::Value& NumberToValue(int number) const;
  int size() const;

 private:
  std::unordered_map<Json::Value, int> data_;
  std::vector<Json::Value> number_to_value_;
};

class JsonAdapter {
 public:
  nice2protos::Query JsonToQuery(const Json::Value &json_query);
  Json::Value InferResponseToJson(const nice2protos::InferResponse &response);

  nice2protos::NBestQuery JsonToNBestQuery(const Json::Value &json_query);
  Json::Value NBestResponseToJson(const nice2protos::NBestResponse &response);

  nice2protos::ShowGraphQuery JsonToShowGraphQuery(const Json::Value &json_query);
  Json::Value ShowGraphResponseToJson(const nice2protos::ShowGraphResponse &response);

 private:
  JsonValueNumberer numberer_;
};

#endif //NICE2PREDICT_JSON_ADAPTER_H
