#!/usr/bin/env bash
# Run every test-*.sh in this directory and report an aggregate result.
# Runs under the Homebrew bash (the runtime needs Bash 4+).
set -o pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BASH_BIN="/opt/homebrew/opt/bash/bin/bash"
[[ -x "${BASH_BIN}" ]] || BASH_BIN="$(command -v bash)"

fails=0
total=0
for t in "${HERE}"/test-*.sh; do
   total=$(( total + 1 ))
   name="$(basename "${t}")"
   printf '\n=== %s ===\n' "${name}"
   if "${BASH_BIN}" "${t}"; then
      :
   else
      fails=$(( fails + 1 ))
   fi
done

printf '\n========================================\n'
if (( fails > 0 )); then
   printf 'SUITE: %d/%d test files FAILED\n' "${fails}" "${total}"
   exit 1
fi
printf 'SUITE: all %d test files passed\n' "${total}"
