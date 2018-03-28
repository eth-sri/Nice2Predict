cc_binary(
    name = "train",
    srcs = [
        "process_data.h",
        "train.cpp",
        "train_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/protos:interface_cc_proto",
    ],
)

cc_binary(
    name = "train_json",
    srcs = [
        "process_data.h",
        "train_json.cpp",
        "train_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/json_server:json_adapter",
        "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
    ],
)

cc_binary(
    name = "eval",
    srcs = [
        "process_data.h",
        "eval.cpp",
        "eval_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/protos:interface_cc_proto",
    ],
)

cc_binary(
    name = "eval_json",
    srcs = [
        "process_data.h",
        "eval_json.cpp",
        "eval_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/json_server:json_adapter",
        "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
    ],
)
