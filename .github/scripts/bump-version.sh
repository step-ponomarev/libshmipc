#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:?Usage: $0 <version>}"
FILE="MODULE.bazel"

if [[ ! -f "$FILE" ]]; then
  echo "Error: $FILE not found" >&2
  exit 1
fi

# check version existance
if ! awk '/^module\(/,/^\)/' "$FILE" | grep -q 'version[[:space:]]*='; then
  echo "Error: no version field in module() block" >&2
  exit 1
fi

# bump version
awk -v ver="$VERSION" '
  /^module\(/ { in_module=1 }
  in_module && /version[[:space:]]*=/ { 
    sub(/version[[:space:]]*=[[:space:]]*"[^"]*"/, "version = \"" ver "\"")
  }
  /^\)/ && in_module { in_module=0 }
  { print }
' "$FILE" >"${FILE}.tmp" && mv "${FILE}.tmp" "$FILE"

echo "Set MODULE.bazel version to ${VERSION}"
