cc_binary(
    name = "train",
    srcs = [
        "train.cpp",
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
