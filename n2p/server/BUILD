cc_binary(
    name = "nice2server",
    srcs = [
        "nice2server.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "nice2server_lib",
        "//n2p/protos:service_cc_proto",
    ],
)

cc_library(
    name = "nice2server_lib",
    srcs = [
        "nice2service_internal.cpp",
        "nice2service_internal.h",
        "server_log.cpp",
        "server_log.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/protos:interface_cc_proto",
        "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
    ],
)
