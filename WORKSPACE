load("//tools/build_defs:externals.bzl", "new_patched_http_archive")
load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

new_patched_http_archive(
  name = "com_github_sparsehash",
  url = "https://github.com/sparsehash/sparsehash/archive/sparsehash-2.0.3.tar.gz",
  sha256 = "05e986a5c7327796dad742182b2d10805a8d4f511ad090da0490f146c1ff7a8c",
  build_file = "//third_party:BUILD.sparsehash",
  strip_prefix = "sparsehash-sparsehash-2.0.3/",
  patch_file = "//third_party:sparsehash.patch",
)

http_archive(
  name = "com_google_googletest",
  url = "https://github.com/google/googletest/archive/refs/tags/release-1.11.0.tar.gz",
  strip_prefix = "googletest-release-1.11.0",
  sha256 = "b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5",
)

git_repository(
  name = "com_google_googletest",
  remote = "https://github.com/google/googletest.git",
  tag = "release-1.11.0",
)

http_archive(
  name = "com_google_protobuf",
  url = "https://github.com/protocolbuffers/protobuf/releases/download/v3.17.3/protobuf-cpp-3.17.3.tar.gz",
  strip_prefix = "protobuf-3.17.3",
  sha256 = "51cec99f108b83422b7af1170afd7aeb2dd77d2bcbb7b6bad1f92509e9ccf8cb",
)

load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
protobuf_deps()

http_archive(
  name = "com_github_madler_zlib",
  url = "https://github.com/madler/zlib/archive/refs/tags/v1.2.11.tar.gz",
  strip_prefix = "zlib-1.2.11",
  build_file = "//third_party:BUILD.zlib",
  sha256 = "629380c90a77b964d896ed37163f5c3a34f6e6d897311f1df2a7016355c45eff",
)

http_archive(
  name = "com_github_grpc_grpc",
  url = "https://github.com/grpc/grpc/archive/refs/tags/v1.38.1.tar.gz",
  strip_prefix = "grpc-1.38.1",
  sha256 = "f60e5b112913bf776a22c16a3053cc02cf55e60bf27a959fd54d7aaf8e2da6e8",
)
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")
grpc_extra_deps()
