def jni_header_name(java_file):
    basename = java_file.split("/")[-1].replace(".java", "")
    package_path = "lib/shm/ipc/jni"
    header_name = package_path.replace("/", "_") + "_" + basename + ".h"
    return "include/shmipc/jni/" + header_name

def jni_header_outs(java_srcs):
    return [jni_header_name(src) for src in java_srcs]

def gen_jni_headers_cmd(java_core_jar_label):
    return """
      set -e
      bazel_outdir="$$(dirname $(OUTS))"
      source_dir="$$(pwd)/java/native/include"
      
      rm -rf "$$bazel_outdir"
      mkdir -p "$$bazel_outdir"
      
      core_jar="$(location """ + java_core_jar_label + """)"
      script="$(location //java:tools/gen_jni_headers.sh)"
      
      "$$script" "$$core_jar" "$$bazel_outdir" "$$source_dir" $(SRCS)
    """

