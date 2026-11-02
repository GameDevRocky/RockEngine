param(
    [string]$SuiteOrTest,
    [string]$TestName
)

$ErrorActionPreference = 'Stop'
$exitCode = 0
$previousPlatform = $env:QT_QPA_PLATFORM

Push-Location $PSScriptRoot
try {
    if ($args.Count -gt 0 -or ($TestName -and $SuiteOrTest -notin @('engine', 'editor'))) {
        throw 'Usage: .\run-tests.ps1 [engine|editor [file.test|SuiteTest|SuiteTest.TestName] | engine test selector]'
    }

    $cachePath = Join-Path $PSScriptRoot 'build/local/CMakeCache.txt'
    if (-not (Test-Path -LiteralPath $cachePath)) {
        throw 'Configure the local CMake preset before running tests.'
    }
    $editorEnabled = [bool](Select-String -LiteralPath $cachePath -Pattern '^ROCKENGINE_BUILD_EDITOR:BOOL=ON$' -Quiet)
    $suite = if ($SuiteOrTest -eq 'editor') { 'editor' } else { 'engine' }
    if ($suite -eq 'editor' -and -not $editorEnabled) {
        throw 'Editor tests are unavailable: ROCKENGINE_BUILD_EDITOR is OFF in build/local.'
    }

    $selection = if ($TestName) {
        $TestName
    } elseif ($SuiteOrTest -in @('engine', 'editor')) {
        ''
    } else {
        $SuiteOrTest
    }

    $targets = if (-not $SuiteOrTest) {
        if ($editorEnabled) { @('RockEngineTests', 'RockEngineEditorTests') }
        else { @('RockEngineTests') }
    } elseif ($suite -eq 'editor') {
        @('RockEngineEditorTests')
    } else {
        @('RockEngineTests')
    }

    & cmake --build --preset local --target $targets
    if ($LASTEXITCODE -ne 0) {
        $exitCode = $LASTEXITCODE
    } elseif (-not $SuiteOrTest) {
        & ctest --test-dir build/local --output-on-failure
        $exitCode = $LASTEXITCODE
    } elseif (-not $selection) {
        $registeredName = if ($suite -eq 'editor') { 'RockEngineEditorTests' } else { 'RockEngineTests' }
        & ctest --test-dir build/local --output-on-failure -R "^$registeredName`$"
        $exitCode = $LASTEXITCODE
    } else {
        if ($selection -match '(?:^|[\\/])([^\\/]+)\.test(?:\.cpp)?$') {
            $filter = "$($Matches[1])Test.*"
        } elseif ($selection -notmatch '[.*?]') {
            $filter = if ($selection.EndsWith('Test')) { "$selection.*" } else { "${selection}Test.*" }
        } else {
            $filter = $selection
        }

        $binaryName = if ($suite -eq 'editor') { 'RockEngineEditorTests.exe' } else { 'RockEngineTests.exe' }
        $testBinary = Join-Path $PSScriptRoot "build/local/bin/$binaryName"
        if ($suite -eq 'editor' -and -not $env:QT_QPA_PLATFORM) { $env:QT_QPA_PLATFORM = 'offscreen' }
        & $testBinary "--gtest_filter=$filter" --gtest_fail_if_no_test_selected
        $exitCode = $LASTEXITCODE
    }
} finally {
    $env:QT_QPA_PLATFORM = $previousPlatform
    Pop-Location
}

exit $exitCode
