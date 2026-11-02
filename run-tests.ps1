param([string]$TestName)

$ErrorActionPreference = 'Stop'
$exitCode = 0

Push-Location $PSScriptRoot
try {
    & cmake --build --preset local --target RockEngineTests
    if ($LASTEXITCODE -ne 0) {
        $exitCode = $LASTEXITCODE
    } elseif ([string]::IsNullOrEmpty($TestName)) {
        & ctest --test-dir build/local --output-on-failure
        $exitCode = $LASTEXITCODE
    } else {
        if ($TestName -match '(?:^|[\\/])([^\\/]+)\.test(?:\.cpp)?$') {
            $filter = "$($Matches[1])Test.*"
        } elseif ($TestName -notmatch '[.*?]') {
            $filter = if ($TestName.EndsWith('Test')) { "$TestName.*" } else { "${TestName}Test.*" }
        } else {
            $filter = $TestName
        }

        $testBinary = Join-Path $PSScriptRoot 'build/local/bin/RockEngineTests.exe'
        & $testBinary "--gtest_filter=$filter" --gtest_fail_if_no_test_selected
        $exitCode = $LASTEXITCODE
    }
} finally {
    Pop-Location
}

exit $exitCode
