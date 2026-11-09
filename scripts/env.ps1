# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright 2026 the ScarletNX contributors








param([switch]$Isolated)

$repo = Split-Path -Parent $PSScriptRoot
$tools = Join-Path $repo 'deps\tools'

$list = Join-Path $tools 'path.txt'
if (-not (Test-Path $list)) {
    throw 'Run scripts\bootstrap.ps1 first: deps\tools is not set up.'
}
$paths = @(Get-Content $list | Where-Object { $_ } | ForEach-Object { Join-Path $tools $_ })
if (-not (Test-Path (Join-Path $paths[0] 'clang.exe'))) {
    throw 'Run scripts\bootstrap.ps1 again: deps\tools has no Clang.'
}
$env:CC = 'clang'; $env:CXX = 'clang++'

if ($Isolated) {
    $system = Join-Path $env:SystemRoot 'System32'
    $paths += @($system, $env:SystemRoot, (Join-Path $system 'WindowsPowerShell\v1.0'))
    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) { $paths += Split-Path -Parent $git.Source }
    $env:PATH = $paths -join ';'
} else {
    $env:PATH = ($paths -join ';') + ';' + $env:PATH
}
Write-Host 'YKW4Recomp tools (Clang) are on PATH'
