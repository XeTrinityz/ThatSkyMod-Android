[CmdletBinding(DefaultParameterSetName = 'Single')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Single', Position=0)]
    [string]$InputFile,     # e.g., animations.json

    [Parameter(Mandatory = $true, ParameterSetName = 'Single', Position=1)]
    [string]$OutputFile,    # e.g., animations_json.h

    # NOT mandatory — a switch. When supplied we run batch mode.
    [Parameter(ParameterSetName = 'Batch')]
    [switch]$ConvertAllTxt   # when present, convert all .txt files in script dir to .h
)

$ErrorActionPreference = 'Stop'

# Prefer PSScriptRoot (works when running a .ps1 file). Fallback to MyInvocation.
if ($PSBoundParameters.ContainsKey('PSScriptRoot') -or $null -ne $PSScriptRoot) {
    $scriptDir = $PSScriptRoot
} else {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
}

# Helper: make absolute path relative to script dir when not rooted
function Make-Absolute([string]$path) {
    if ([System.IO.Path]::IsPathRooted($path)) {
        return (Resolve-Path -LiteralPath $path -ErrorAction Stop).ProviderPath
    } else {
        $joined = Join-Path -Path $scriptDir -ChildPath $path
        return (Resolve-Path -LiteralPath $joined -ErrorAction Stop).ProviderPath
    }
}

function Convert-BytesToHeader {
    param(
        [Parameter(Mandatory=$true)][string]$ResolvedInput,
        [Parameter(Mandatory=$true)][string]$ResolvedOutput
    )

    # Read file bytes
    [byte[]]$bytes = [System.IO.File]::ReadAllBytes($ResolvedInput)

    # Safe identifier
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($ResolvedInput)
    $safeName = $baseName -replace '[^a-zA-Z0-9]', '_'

    # Build header
    $sb = New-Object System.Text.StringBuilder
    $null = $sb.AppendLine('#pragma once')
    $null = $sb.AppendLine('#include <cstddef>')
    $null = $sb.AppendLine('')
    $null = $sb.AppendLine("static const unsigned char k${safeName}Data[] = {")

    for ($i = 0; $i -lt $bytes.Length; $i++) {
        $null = $sb.Append(('0x{0:X2}' -f $bytes[$i]))
        if ($i -lt ($bytes.Length - 1)) { $null = $sb.Append(', ') }
        if ((($i + 1) % 16) -eq 0) { $null = $sb.AppendLine() }
    }

    $null = $sb.AppendLine()
    $null = $sb.AppendLine('};')
    $null = $sb.AppendLine("static const std::size_t k${safeName}Size = sizeof(k${safeName}Data);")

    # Ensure output directory exists (only if there is a parent path)
    $dstDir = Split-Path -Parent $ResolvedOutput
    if (-not [string]::IsNullOrWhiteSpace($dstDir)) {
        if (-not (Test-Path $dstDir)) {
            New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
        }
    }

    # Write output (ASCII)
    [System.IO.File]::WriteAllText($ResolvedOutput, $sb.ToString(), [System.Text.Encoding]::ASCII)

    Write-Host "Wrote $ResolvedOutput (from $ResolvedInput)"
}

# If user passed nothing meaningful, show helpful usage instead of throwing ambiguous parameter-set error
if (-not $PSBoundParameters.Keys.Count) {
    Write-Host "Usage examples:`n  .\EmbedBinary.ps1 -InputFile animations.json -OutputFile animations_json.h`n  .\EmbedBinary.ps1 -ConvertAllTxt"
    return
}

# Main logic: either Batch or Single
if ($ConvertAllTxt) {
    # Find all .txt files in the script directory (non-recursive)
    $txtFiles = Get-ChildItem -Path $scriptDir -Filter '*.txt' -File -ErrorAction Stop

    if ($txtFiles.Count -eq 0) {
        throw "No .txt files found in script directory: $scriptDir"
    }

    foreach ($f in $txtFiles) {
        try {
            $inputPath = $f.FullName
            $outputName = ($f.BaseName) + '.h'
            $outputPath = Join-Path -Path $scriptDir -ChildPath $outputName

            # Resolve to full paths (Join-Path + GetFullPath ensures canonical path)
            $resolvedInput = (Resolve-Path -LiteralPath $inputPath -ErrorAction Stop).ProviderPath
            $resolvedOutput = [System.IO.Path]::GetFullPath($outputPath)

            Convert-BytesToHeader -ResolvedInput $resolvedInput -ResolvedOutput $resolvedOutput
        } catch {
            Write-Error "Failed converting '$($f.Name)': $_"
        }
    }
} else {
    # Single-file mode
    # Resolve input path (must exist)
    try {
        $resolvedInput = if ([System.IO.Path]::IsPathRooted($InputFile)) {
            Resolve-Path -LiteralPath $InputFile -ErrorAction Stop
        } else {
            Resolve-Path -LiteralPath (Join-Path $scriptDir $InputFile) -ErrorAction Stop
        }
    } catch {
        throw "Source file not found: $InputFile (resolved from script dir: $scriptDir)"
    }
    $InputFile = $resolvedInput.ProviderPath

    # Resolve output path (do NOT require it to exist yet)
    if ([System.IO.Path]::IsPathRooted($OutputFile)) {
        $OutputFile = [System.IO.Path]::GetFullPath($OutputFile)
    } else {
        $OutputFile = Join-Path $scriptDir $OutputFile
    }

    # Convert single file
    Convert-BytesToHeader -ResolvedInput $InputFile -ResolvedOutput $OutputFile
}
