cc_test(
    name = "unittests",
    srcs = ["unit_tests.cpp"],
    deps = [
        "//base",
        "//n2p/inference",
        "//n2p/protos:service_cc_proto",
        "//n2p/json_server:json_adapter",
        "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
        "@com_google_googletest//:gtest_main",
    ],
    size = "small",
)
