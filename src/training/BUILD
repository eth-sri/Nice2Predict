
cc_binary(name = "train",
           srcs = ["train.cpp",
                  ],
           deps = ["//src/protos:service_cc_proto",
                   "//src/base:base",
                   "//src/inference:inference",
                  ],
           visibility = ["//visibility:public"])

cc_binary(name = "eval",
           srcs = ["eval.cpp",
                  ],
           deps = ["//src/protos:service_cc_proto",
                   "//src/base:base",
                   "//src/inference:inference",
                  ],
           visibility = ["//visibility:public"])

