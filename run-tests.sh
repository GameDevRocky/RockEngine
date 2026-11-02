#!/usr/bin/env bash
set -euo pipefail

if (( $# > 1 )); then
    printf 'Usage: %s [GameObject.test | GameObjectTest | GameObjectTest.TestName]\n' "$0" >&2
    exit 2
fi

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cmake --build --preset local --target RockEngineTests

if (( $# == 0 )); then
    ctest --test-dir build/local --output-on-failure
    exit
fi

selector="$1"
case "$selector" in
    *.test.cpp)
        selector="${selector##*/}"
        filter="${selector%.test.cpp}Test.*"
        ;;
    *.test)
        selector="${selector##*/}"
        filter="${selector%.test}Test.*"
        ;;
    *.*|*'*'*|*'?'*)
        filter="$selector"
        ;;
    *Test)
        filter="$selector.*"
        ;;
    *)
        filter="${selector}Test.*"
        ;;
esac

test_binary="build/local/bin/RockEngineTests"
if [[ -f "${test_binary}.exe" ]]; then
    test_binary="${test_binary}.exe"
fi

"$test_binary" "--gtest_filter=$filter" --gtest_fail_if_no_test_selected
