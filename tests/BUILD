cc_test(
    name = "unittests",
    srcs = ["unit_tests.cpp"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        "@com_google_googletest//:gtest_main",
        "//src/base:base",
        "//src/inference:inference",
    ],
)
