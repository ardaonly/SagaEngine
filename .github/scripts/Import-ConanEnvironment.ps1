# SPDX-License-Identifier: MPL-2.0

function Import-ConanEnvironment {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath
    )

    if (-not (Test-Path -LiteralPath $ScriptPath -PathType Leaf)) {
        throw "Missing Conan environment script: $ScriptPath"
    }

    $command = "call `"$ScriptPath`" >nul && set"
    $lines = & $env:ComSpec /d /c $command
    if ($LASTEXITCODE -ne 0) {
        throw "Conan environment script failed with exit code $LASTEXITCODE`: $ScriptPath"
    }

    foreach ($line in $lines) {
        $separator = $line.IndexOf("=")
        # cmd.exe also reports drive-current-directory entries such as '=C:=...'.
        if ($separator -le 0) {
            continue
        }

        $name = $line.Substring(0, $separator)
        $value = $line.Substring($separator + 1)
        Set-Item -Path "Env:$name" -Value $value
    }

    Write-Host "Imported Conan environment: $ScriptPath"
}
