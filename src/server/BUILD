cc_binary(
    name = "nice2serverproto",
    srcs = [
        "nice2serverproto.cpp",
        "nice2service_internal.cpp",
        "nice2service_internal.h",
        "server_log.cpp",
        "server_log.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_glog_glog//:glog",
    ],
)

cc_binary(
    name = "nice2server",
    srcs = [
        "json_adapter.cpp",
        "json_adapter.h",
        "main.cpp",
        "nice2server.cpp",
        "nice2server.h",
        "nice2service_internal.cpp",
        "nice2service_internal.h",
        "server_log.cpp",
        "server_log.h",
    ],
    copts = ["-I/usr/local/include"],
    visibility = ["//visibility:public"],
    deps = [
        "json_adapter",
        "//json:jsonrpc",
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "@com_github_gflags_gflags//:gflags",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "json_adapter",
    srcs = [
        "json_adapter.cpp",
        "json_adapter.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//json",
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "@com_github_glog_glog//:glog",
    ],
)
