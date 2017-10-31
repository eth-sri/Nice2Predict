load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cc_proto_library")

cc_proto_library(
  name = "service_cc_proto",
  protos = ["service.proto"],
  proto_deps = ["interface_cc_proto"],
  with_grpc = True,
  visibility = ["//visibility:public"],
)

cc_proto_library(
  name = "interface_cc_proto",
  protos = ["interface.proto"],
  with_grpc = False,
  visibility = ["//visibility:public"]
)
