# This directory is entirely composed of the open source projects
#
#   https://github.com/open-source-parsers/jsoncpp  (MIT license) included as amalgamated source.
# and
#   https://github.com/cinemast/libjson-rpc-cpp  (MIT license) included by calling pull_json_rpc.py

cc_library(name = "jsoncpp",
           srcs = ["jsoncpp.cpp",
                   "json.h",
                   "json-forwards.h"],
           visibility = ["//visibility:public"])

cc_library(name = "json",
           deps = ["jsoncpp"],
           visibility = ["//visibility:public"])

cc_library(name = "jsonrpc",
           srcs = [
               "client.h",
               "server.h",
               "client_batchcall.cpp",
               "client_client.h",
               "client_client.cpp",
               "client_batchresponse.cpp",
               "client_batchresponse.h",
               "client_rpcprotocolclient.h",
               "client_rpcprotocolclient.cpp",
               "client_batchcall.h",
               "client_iclientconnector.h",
               "client_connectors_unixdomainsocketclient.h",
               "client_connectors_httpclient.cpp",
               "client_connectors_httpclient.h",
               "client_connectors_unixdomainsocketclient.cpp",
               "server_abstractprotocolhandler.cpp",
               "server_rpcprotocolserverv2.cpp",
               "server_requesthandlerfactory.cpp",
               "server_iprocedureinvokationhandler.h",
               "server_abstractprotocolhandler.h",
               "server_rpcprotocolserverv1.h",
               "server_rpcprotocolserverv1.cpp",
               "server_abstractserverconnector.h",
               "server_abstractserver.h",
               "server_requesthandlerfactory.h",
               "server_abstractserverconnector.cpp",
               "server_rpcprotocolserverv2.h",
               "server_rpcprotocolserver12.h",
               "server_rpcprotocolserver12.cpp",
               "server_iclientconnectionhandler.h",
               "server_connectors_httpserver.cpp",
               "server_connectors_httpserver.h",
               "server_connectors_unixdomainsocketserver.cpp",
               "server_connectors_unixdomainsocketserver.h",
               "common_exception.cpp",
               "common_specificationparser.cpp",
               "common_exception.h",
               "common_specificationwriter.cpp",
               "common_specification.h",
               "common_errors.h",
               "common_specificationwriter.h",
               "common_specificationparser.h",
               "common_errors.cpp",
               "common_procedure.cpp",
               "common_procedure.h",
           ],
           deps = [":jsoncpp"],
           linkopts = ["-lcurl", "-lmicrohttpd"],
           visibility = ["//visibility:public"])


cc_binary(name = "testrpcclient",
          srcs = ["simpleclient.cpp"],
          deps = [":jsonrpc"])

cc_binary(name = "testrpcserver",
          srcs = ["simpleserver.cpp"],
          copts = ["-I/usr/local/include"],
          deps = [":jsonrpc"])
