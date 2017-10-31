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
        "//json:jsonrpc",
        "//n2p/server:nice2server_lib",
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
        "//base",
        "//n2p/inference",
        "//n2p/protos:interface_cc_proto",
    ],
)
