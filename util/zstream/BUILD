licenses(["notice"])

cc_library(name = "zstream",
           hdrs = [
               "strict_fstream.h",
               "zstream.h",
           ],
           deps = [
               "@com_github_madler_zlib//:zlib",
           ],
           visibility = ["//visibility:public"])

cc_test(name = "zstream_test",
        srcs = ["zstream_test.cpp"],
        deps = [ "//base",
                 ":zstream",
                 "@com_google_googletest//:gtest",
               ])
