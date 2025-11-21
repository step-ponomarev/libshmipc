#!/bin/bash
set -e

CORE_JAR="$1"
OUTPUT_DIR="$2"
SOURCE_DIR="$3"
shift 3
JAVA_FILES=("$@")

# Check JAVA_HOME
if [[ -z "${JAVA_HOME:-}" ]]; then
    echo "JAVA_HOME is not set; please install JDK and export JAVA_HOME" >&2
    exit 1
fi

# Create temp directories
classes_dir=$(mktemp -d)
temp_header_dir=$(mktemp -d)
trap 'rm -rf "$classes_dir" "$temp_header_dir"' EXIT

# Generate headers
"$JAVA_HOME/bin/javac" -h "$temp_header_dir" -d "$classes_dir" -cp "$CORE_JAR" "${JAVA_FILES[@]}"

# Copy to Bazel output directory
mkdir -p "$OUTPUT_DIR"
find "$temp_header_dir" -name "*.h" -exec cp {} "$OUTPUT_DIR/" \;

# Copy to source directory (for IDE indexing)
if [ -n "$SOURCE_DIR" ] && [ -d "$(dirname "$SOURCE_DIR")" ]; then
    mkdir -p "$SOURCE_DIR/shmipc/jni" 2>/dev/null || true
    find "$temp_header_dir" -name "*.h" -exec cp {} "$SOURCE_DIR/shmipc/jni/" \; 2>/dev/null || true
fi

