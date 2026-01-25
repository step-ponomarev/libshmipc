load("@rules_cc//cc:defs.bzl", "cc_test")

def cc_tests(name, srcs, deps = [], copts = [], linkopts = [], **kwargs):
    tests = []
    for src in srcs:
        test_name = src.split("/")[-1].replace(".cpp", "").replace(".cc", "").replace(".cxx", "")

        cc_test(
            name = test_name,
            srcs = [src],
            deps = deps,
            copts = copts,
            linkopts = linkopts,
            **kwargs
        )
        tests.append(":" + test_name)
    
    native.test_suite(
        name = name,
        tests = tests,
        visibility = kwargs.get("visibility", ["//visibility:private"]),
    )
