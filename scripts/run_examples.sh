#!/bin/sh
set -eu

if [ "$#" -gt 1 ]; then
  echo "Usage: $0 [path-to-repl]" >&2
  exit 2
fi

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "${script_dir}/.." && pwd)
repl_bin=${1:-"${repo_root}/build/ci-build/repl/repl"}
examples_dir="${repo_root}/examples"

if [ ! -x "${repl_bin}" ]; then
  echo "repl binary not found or not executable: ${repl_bin}" >&2
  exit 2
fi

if [ ! -d "${examples_dir}" ]; then
  echo "examples directory not found: ${examples_dir}" >&2
  exit 2
fi

found_any=0
pass_count=0
fail_count=0

for example_script in "${examples_dir}"/example*.pips; do
  if [ ! -e "${example_script}" ]; then
    continue
  fi

  found_any=1
  example_name=$(basename "${example_script}")
  echo "Running ${example_name}"

  if "${repl_bin}" -i "${example_script}" >/dev/null; then
    echo "PASS ${example_name}"
    pass_count=$((pass_count + 1))
  else
    echo "FAIL ${example_name}" >&2
    fail_count=$((fail_count + 1))
  fi
done

if [ "${found_any}" -eq 0 ]; then
  echo "No example scripts found in ${examples_dir}" >&2
  exit 2
fi

echo "Summary: ${pass_count} passed, ${fail_count} failed"

if [ "${fail_count}" -ne 0 ]; then
  exit 1
fi
