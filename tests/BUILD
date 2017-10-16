cc_test(
    name = "unittests",
    srcs = ["unit_tests.cpp"],
    #    copts = ["-Iexternal/gtest/include"],
    deps = [
        "//json",
        "//src/base",
        "//src/inference",
        "//src/protos:service_cc_proto",
        "//src/server:json_adapter",
        "@com_google_googletest//:gtest_main",
    ],
)
