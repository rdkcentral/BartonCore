#!/usr/bin/env bash

# Installs git commit hooks.

MY_DIR=$(realpath $(dirname $0))
TOP_DIR=${MY_DIR}/..
HOOKS_DIR=${TOP_DIR}/.git/hooks

HOOKS_SUPPORTED=(pre-commit)

# Copy various hooks, namespaced
cp ${MY_DIR}/clang-format-hooks/apply-format ${HOOKS_DIR}/
cp ${MY_DIR}/clang-format-hooks/git-pre-commit-format ${HOOKS_DIR}/pre-commit-clang-format

# Copy our hook(s) that will call the others.
for hook in "${HOOKS_SUPPORTED[@]}"; do
    echo "Installing ${hook} hook."
    cp ${MY_DIR}/${hook} ${HOOKS_DIR}/${hook}
done
