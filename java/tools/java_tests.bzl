def java_tests(name, srcs, runtime_deps = [], jvm_flags = [], deps = [],  **kwargs):
    tests = []
    
    for src in srcs:
        test_path = src.replace("src/test/java/", "").replace(".java", "")
        test_class = test_path.replace("/", ".")
        test_name = test_path.split("/")[-1]
        
        native.java_test(
            name = test_name,
            srcs = [src],
            test_class = test_class,
            deps = deps,
            runtime_deps = runtime_deps,
            jvm_flags = jvm_flags,
            **kwargs
        )
        tests.append(":" + test_name)
    
    native.test_suite(
        name = name,
        tests = tests,
        visibility = kwargs.get("visibility", ["//visibility:private"]),
    )

