
cc_library(name = "inference",
           srcs = ["inference.cpp",
                   "graph_inference.cpp",
                   "label_checker.cpp",

                   "inference.h",
                   "graph_inference.h",
                   "label_checker.h",
                   "label_set.h",
                   "lock_free_weight.h",
                  ],
           deps = ["//n2p/protos:service_cc_proto",
                   "//base",
                   "@com_github_open_source_parsers_jsoncpp//:jsoncpp",
                   "@com_github_sparsehash//:sparsehash",
                  ],
           linkopts = ["-lgflags"],
           data = ["//:common_files"],
           visibility = ["//visibility:public"])

