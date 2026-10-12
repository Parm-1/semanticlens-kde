#!/usr/bin/env bash
# Fetch/verify the exact CPU runtime source. Model setup is a separate action.
set -euo pipefail
cd "$(dirname "$0")/.."
git_command=${KDE_INDEX_GIT:-/usr/bin/git}
revision=4d756bc72bf00a4aacf410ae15a2d315f3db400d
patch="$PWD/evidence/embedding-v1/source/llama.cpp-no-logits.patch"
mkdir -p vendor
if [[ ! -d vendor/llama.cpp/.git ]]; then
    "$git_command" -c advice.detachedHead=false clone --depth 1 --branch b11461 https://github.com/ggml-org/llama.cpp vendor/llama.cpp
fi
[[ $("$git_command" -C vendor/llama.cpp rev-parse HEAD) == "$revision" ]]
[[ $(sha256sum "$patch" | cut -d' ' -f1) == 33c8c4812a4307aef3f398e5ba622edcc54ef421054edd98ca0f71906b9a2d7e ]]
if "$git_command" -C vendor/llama.cpp apply --check "$patch" 2>/dev/null; then
    "$git_command" -C vendor/llama.cpp apply "$patch"
fi
"$git_command" -C vendor/llama.cpp apply --reverse --check "$patch"
[[ $("$git_command" -C vendor/llama.cpp diff | sha256sum | cut -d' ' -f1) == $(sha256sum "$patch" | cut -d' ' -f1) ]]
[[ $("$git_command" -C vendor/llama.cpp status --porcelain --untracked-files=all) == " M src/llama-context.cpp" ]]
printf '%s\n' "CPU runtime source verified at $revision plus the pinned no-logits patch."
