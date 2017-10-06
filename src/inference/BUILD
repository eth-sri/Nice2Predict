
cc_library(name = "inference",
           srcs = ["inference.cpp",
                   "graph_inference.cpp",
                   "label_checker.cpp",

                   "inference.h",
                   "graph_inference.h",
                   "label_checker.h",
                   "label_set.h",
                   "lock_free_weight.h",
                   "smt_beam.h",
                  ],
           deps = ["//src/protos:service_cc_proto",
                   "//src/base:base",
                   "//json:json",
                   "@com_github_gflags_gflags//:gflags",
                   "@com_github_sparsehash//:sparsehash",
                  ],
           data = ["//:common_files"],
           visibility = ["//visibility:public"])

