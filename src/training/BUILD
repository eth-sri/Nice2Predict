cc_binary(
    name = "train",
    srcs = [
        "train.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//json",
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "//src/server:json_adapter",
    ],
)

cc_binary(
    name = "eval",
    srcs = [
        "eval.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//json",
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "//src/server:json_adapter",
    ],
)
