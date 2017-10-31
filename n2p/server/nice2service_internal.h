//
// Created by Oleg Ponomarev on 10/10/17.
//

#ifndef NICE2PREDICT_NICE2SERVICEINTERNAL_H
#define NICE2PREDICT_NICE2SERVICEINTERNAL_H

#include "n2p/inference/graph_inference.h"
#include "n2p/protos/service.pb.h"

#include "server_log.h"


class Nice2ServiceInternal {

 public:
  Nice2ServiceInternal(const std::string &model_path, const std::string &logfile_prefix);
  virtual ~Nice2ServiceInternal() = default;

  nice2protos::InferResponse Infer(const nice2protos::Query &request);
  nice2protos::NBestResponse NBest(const nice2protos::NBestQuery &request);
  nice2protos::ShowGraphResponse ShowGraph(const nice2protos::ShowGraphQuery &request);

 private:
  GraphInference inference_;
  std::unique_ptr<Nice2ServerLog> logging_;
};

#endif //NICE2PREDICT_NICE2SERVICEINTERNAL_H
