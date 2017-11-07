//
// Created by Oleg Ponomarev on 10/10/17.
//

#include <iostream>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "n2p/inference/graph_inference.h"

#include "nice2service_internal.h"

using std::string;

using nice2protos::Query;
using nice2protos::NBestQuery;
using nice2protos::ShowGraphQuery;
using nice2protos::InferResponse;
using nice2protos::NBestResponse;
using nice2protos::ShowGraphResponse;


// Logic and data behind the server's behavior.
Nice2ServiceInternal::Nice2ServiceInternal(const string &model_path, const string &logfile_prefix) {
  inference_.LoadModel(model_path);
  if (!logfile_prefix.empty()) {
    logging_.reset(new Nice2ServerLog(logfile_prefix));
  }
}

InferResponse Nice2ServiceInternal::Infer(const Query &request) {
  std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
  query->FromFeaturesQueryProto(request.features());
  std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
  assignment->FromNodeAssignmentsProto(request.node_assignments());
  inference_.MapInference(query.get(), assignment.get());
  InferResponse response;
  assignment->FillInferResponse(&response);
  return response;
}

nice2protos::NBestResponse Nice2ServiceInternal::NBest(const nice2protos::NBestQuery &request) {
  std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
  query->FromFeaturesQueryProto(request.query().features());
  std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
  assignment->FromNodeAssignmentsProto(request.query().node_assignments());
  if (request.should_infer()) {
    inference_.MapInference(query.get(), assignment.get());
  }
  NBestResponse response;
  assignment->GetNBestCandidates(&inference_, request.n(), &response);
  return response;
}

nice2protos::ShowGraphResponse Nice2ServiceInternal::ShowGraph(const nice2protos::ShowGraphQuery &request) {
  std::unique_ptr<Nice2Query> query(inference_.CreateQuery());
  query->FromFeaturesQueryProto(request.query().features());
  std::unique_ptr<Nice2Assignment> assignment(inference_.CreateAssignment(query.get()));
  assignment->FromNodeAssignmentsProto(request.query().node_assignments());
  if (request.should_infer()) {
    inference_.MapInference(query.get(), assignment.get());
  }
  ShowGraphResponse response;
  inference_.FillGraphProto(query.get(), assignment.get(), &response);
  return response;
}
