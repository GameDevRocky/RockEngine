#!/usr/bin/env bash
set -euo pipefail

if (( $# > 2 )); then
    printf 'Usage: %s [engine|editor [file.test|SuiteTest|SuiteTest.TestName] | engine test selector]\n' "$0" >&2
    exit 2
fi

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if [[ ! -f build/local/CMakeCache.txt ]]; then
    printf 'Configure the local CMake preset before running tests.\n' >&2
    exit 2
fi

editor_enabled=false
if grep -q '^ROCKENGINE_BUILD_EDITOR:BOOL=ON$' build/local/CMakeCache.txt; then
    editor_enabled=true
fi

suite=engine
selection=""
if (( $# >= 1 )); then
    case "$1" in
        editor)
            suite=editor
            ;;
        engine)
            suite=engine
            ;;
        *)
            if (( $# == 2 )); then
                printf 'The first argument must be engine or editor when using two arguments.\n' >&2
                exit 2
            fi
            selection="$1"
            ;;
    esac
fi
if (( $# == 2 )); then
    selection="$2"
fi

if [[ "$suite" == editor && "$editor_enabled" == false ]]; then
    printf 'Editor tests are unavailable: ROCKENGINE_BUILD_EDITOR is OFF in build/local.\n' >&2
    exit 2
fi

targets=(RockEngineTests)
if (( $# == 0 )) && [[ "$editor_enabled" == true ]]; then
    targets+=(RockEngineEditorTests)
elif [[ "$suite" == editor ]]; then
    targets=(RockEngineEditorTests)
fi
cmake --build --preset local --target "${targets[@]}"

if (( $# == 0 )); then
    ctest --test-dir build/local --output-on-failure
    exit
fi

if [[ -z "$selection" ]]; then
    registered_name=RockEngineTests
    if [[ "$suite" == editor ]]; then
        registered_name=RockEngineEditorTests
    fi
    ctest --test-dir build/local --output-on-failure -R "^${registered_name}$"
    exit
fi

case "$selection" in
    *.test.cpp)
        selection="${selection##*/}"
        filter="${selection%.test.cpp}Test.*"
        ;;
    *.test)
        selection="${selection##*/}"
        filter="${selection%.test}Test.*"
        ;;
    *.*|*'*'*|*'?'*)
        filter="$selection"
        ;;
    *Test)
        filter="$selection.*"
        ;;
    *)
        filter="${selection}Test.*"
        ;;
esac

binary_name=RockEngineTests
if [[ "$suite" == editor ]]; then
    binary_name=RockEngineEditorTests
fi
test_binary="build/local/bin/${binary_name}"
if [[ -f "${test_binary}.exe" ]]; then
    test_binary="${test_binary}.exe"
fi

if [[ "$suite" == editor ]]; then
    QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" "$test_binary" "--gtest_filter=$filter" --gtest_fail_if_no_test_selected
else
    "$test_binary" "--gtest_filter=$filter" --gtest_fail_if_no_test_selected
fi
