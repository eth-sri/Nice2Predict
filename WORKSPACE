workspace(name = "com_ethz_srl_nice2predict")

http_archive(
    name = "org_pubref_rules_protobuf",
    sha256 = "fb9852446b5ba688cd7178a60ff451623e4112d015c6adfe0e9a06c5d2dedc08",
    strip_prefix = "rules_protobuf-0.8.1/",
    url = "https://github.com/pubref/rules_protobuf/archive/v0.8.1.tar.gz",
)

load("@org_pubref_rules_protobuf//cpp:rules.bzl", "cpp_proto_repositories")
cpp_proto_repositories()

#BTW, @org_pubref_rules_protobuf already contains @com_google_googletest

load("//tools/build_defs:externals.bzl",
     "new_patched_http_archive",
)

# The sparsehash BUILD is copied from https://github.com/livegrep/livegrep
new_patched_http_archive(
  name = "com_github_sparsehash",
  url = "https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz",
  sha256 = "05e986a5c7327796dad742182b2d10805a8d4f511ad090da0490f146c1ff7a8c",
  build_file = "//third_party:BUILD.sparsehash",
  strip_prefix = "sparsehash-sparsehash-2.0.3/",
  patch_file = "//third_party:sparsehash.patch",
)

new_http_archive(
    name = "com_github_open_source_parsers_jsoncpp",
    build_file = "//third_party:BUILD.jsoncpp",
    sha256 = "3671ba6051e0f30849942cc66d1798fdf0362d089343a83f704c09ee7156604f",
    strip_prefix = "jsoncpp-1.8.3/",
    url = "https://github.com/open-source-parsers/jsoncpp/archive/1.8.3.tar.gz",
)

new_http_archive(
    name = "com_github_cinemast_libjson_rpc_cpp",
    build_file = "//third_party:BUILD.jsonrpc",
    sha256 = "888c10f4be145dfe99e007d5298c90764fb73b58effb2c6a3fc522a5b60a18c6",
    strip_prefix = "libjson-rpc-cpp-1.0.0",
    url = "https://github.com/cinemast/libjson-rpc-cpp/archive/v1.0.0.tar.gz",
)
