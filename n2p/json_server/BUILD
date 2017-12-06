cc_binary(
    name = "json_server",
    srcs = [
        "main.cpp",
        "json_server.cpp",
        "json_server.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "json_adapter",
        "//n2p/server:nice2server_lib",
        "@com_github_cinemast_libjson_rpc_cpp//:jsonrpc",
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
        "//base",
        "//n2p/inference",
        "//n2p/protos:interface_cc_proto",
        "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
    ],
)
