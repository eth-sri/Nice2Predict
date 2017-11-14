cc_binary(
    name = "train",
    srcs = [
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
        "train_json.cpp",
        "train_internal.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//json",
        "//base",
        "//n2p/inference",
        "//n2p/json_server:json_adapter",
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
	"//base",
	"//n2p/inference",
	"//n2p/json_server:json_adapter",
    ],
)
