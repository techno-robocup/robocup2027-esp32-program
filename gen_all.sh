#!/usr/bin/env bash

OUTPUT_FILE="robocup2027-esp32-program.txt"
TARGET_DIR="${1:-.}"

# Exclude directories/files you usually don't want to upload (added libtorch)
EXCLUDE_DIRS="(\.git|\.next|node_modules|dist|build|vendor|\.venv|venv|__pycache__|libtorch|build)"
EXCLUDE_FILES="(\.DS_Store|package-lock\.json|yarn\.lock|pnpm-lock\.yaml|project_context\.txt)"

echo "=== DIRECTORY STRUCTURE ===" >"$OUTPUT_FILE"

# 1. Generate directory tree
if command -v tree &>/dev/null; then
  tree -a "$TARGET_DIR" -I "node_modules|.git|.next|dist|build|vendor|.venv|venv|__pycache__|libtorch" >>"$OUTPUT_FILE"
else
  find "$TARGET_DIR" -maxdepth 4 | grep -vE "$EXCLUDE_DIRS" >>"$OUTPUT_FILE"
fi

echo -e "\n\n=== FILE CONTENTS ===\n" >>"$OUTPUT_FILE"

# 2. Append file contents with clear boundaries
find "$TARGET_DIR" -type f | grep -vE "$EXCLUDE_DIRS" | grep -vE "$EXCLUDE_FILES" | while read -r file; do
  # Only process text files (ignore binary files like images, compiled code, etc.)
  if file --mime-encoding "$file" | grep -q 'binary'; then
    continue
  fi

  echo "==================================================" >>"$OUTPUT_FILE"
  echo "FILE: $file" >>"$OUTPUT_FILE"
  echo "==================================================" >>"$OUTPUT_FILE"
  cat "$file" >>"$OUTPUT_FILE"
  echo -e "\n\n" >>"$OUTPUT_FILE"
done

echo "Done! Output saved to: $OUTPUT_FILE"
