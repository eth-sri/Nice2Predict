load("@com_github_grpc_grpc//bazel:cc_grpc_library.bzl", "cc_grpc_library")
load("@com_github_grpc_grpc//bazel:generate_cc.bzl", "generate_cc")

proto_library(
  name = "service",
  srcs = ["service.proto"],
  deps = [
    ":interface",
    "@com_google_protobuf//:any_proto",
  ],
)

proto_library(
  name = "interface",
  srcs = ["interface.proto"],
)

cc_proto_library(
  name = "service_cc",
  deps = [":service"],
)

cc_grpc_library(
  name = "service_cc_proto",
  srcs = [":service"],
  deps = [
    ":service_cc",
    "@com_github_grpc_grpc//:grpc++",
  ],
  grpc_only = True,
  visibility = ["//visibility:public"],
)

cc_proto_library(
  name = "interface_cc_proto",
  deps = [":interface"],
  visibility = ["//visibility:public"]
)
