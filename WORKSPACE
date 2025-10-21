# Bazel WORKSPACE for libshmipc
workspace(name = "libshmipc")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Doctest external dependency
http_archive(
    name = "com_github_doctest",
    urls = ["https://github.com/doctest/doctest/archive/refs/tags/v2.4.12.zip"],
    strip_prefix = "doctest-2.4.12",
    build_file_content = """
cc_library(
    name = "doctest",
    hdrs = glob(["doctest/doctest.h"]),
    includes = ["."],
    visibility = ["//visibility:public"],
)
""",
)
