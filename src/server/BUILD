
cc_binary(name = "nice2serverproto",
           srcs = ["nice2serverproto.cpp",
                   "server_log.cpp",

                   "server_log.h",
                  ],
           deps = ["//src/protos:service_cc_proto",
                   "//src/base:base",
                   "//src/inference:inference",
                   "//json:json"
                  ],
           visibility = ["//visibility:public"])

cc_binary(name = "nice2server",
           srcs = ["nice2server.cpp",
                   "server_log.cpp",
                   "main.cpp",

                   "nice2server.h",
                   "server_log.h",
                  ],
           deps = ["//src/protos:service_cc_proto",
                   "//src/base:base",
                   "//src/inference:inference",
                   "//json:jsonrpc"
                  ],
           copts = ["-I/usr/local/include"],
           visibility = ["//visibility:public"])

