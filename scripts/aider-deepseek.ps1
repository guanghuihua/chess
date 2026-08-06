param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AiderArguments
)

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$aiderExecutable = Join-Path $env:USERPROFILE ".local\bin\aider.exe"
$credentialTarget = "GuanghuiEducationLab.XiangqiTraining.DeepSeekApiKey"

if (-not (Test-Path -LiteralPath $aiderExecutable)) {
    Write-Error "Aider was not found: $aiderExecutable"
    Write-Host "Install it with the official aider-install package before running this task."
    exit 1
}

if (-not ("MindDuet.NativeCredential" -as [type])) {
    Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

namespace MindDuet {
    public static class NativeCredential {
        [StructLayout(LayoutKind.Sequential)]
        public struct Credential {
            public UInt32 Flags;
            public UInt32 Type;
            public IntPtr TargetName;
            public IntPtr Comment;
            public Int64 LastWritten;
            public UInt32 CredentialBlobSize;
            public IntPtr CredentialBlob;
            public UInt32 Persist;
            public UInt32 AttributeCount;
            public IntPtr Attributes;
            public IntPtr TargetAlias;
            public IntPtr UserName;
        }

        [DllImport("advapi32.dll", EntryPoint = "CredReadW", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool CredRead(string target, UInt32 type, UInt32 flags, out IntPtr credential);

        [DllImport("advapi32.dll")]
        public static extern void CredFree(IntPtr credential);
    }
}
"@
}

$credentialPointer = [IntPtr]::Zero
if (-not [MindDuet.NativeCredential]::CredRead($credentialTarget, 1, 0, [ref]$credentialPointer)) {
    Write-Error "DeepSeek credential was not found in Windows Credential Manager. Save it from MindDuet Chess first."
    exit 1
}

$deepSeekApiKey = $null
try {
    $credential = [Runtime.InteropServices.Marshal]::PtrToStructure(
        $credentialPointer,
        [type][MindDuet.NativeCredential+Credential]
    )
    $credentialBytes = New-Object byte[] $credential.CredentialBlobSize
    [Runtime.InteropServices.Marshal]::Copy(
        $credential.CredentialBlob,
        $credentialBytes,
        0,
        $credential.CredentialBlobSize
    )
    $deepSeekApiKey = [Text.Encoding]::UTF8.GetString($credentialBytes).Trim()
} finally {
    [MindDuet.NativeCredential]::CredFree($credentialPointer)
}

if ([string]::IsNullOrWhiteSpace($deepSeekApiKey)) {
    Write-Error "The DeepSeek credential is empty."
    exit 1
}

$previousDeepSeekApiKey = $env:DEEPSEEK_API_KEY
$previousPythonIoEncoding = $env:PYTHONIOENCODING
$previousPythonUtf8 = $env:PYTHONUTF8
$previousOutputEncoding = [Console]::OutputEncoding
try {
    $env:DEEPSEEK_API_KEY = $deepSeekApiKey
    $env:PYTHONIOENCODING = "utf-8"
    $env:PYTHONUTF8 = "1"
    [Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
    & $aiderExecutable `
        --model "deepseek/deepseek-v4-flash" `
        --no-auto-commits `
        --no-analytics `
        --no-check-update `
        --no-gitignore `
        --no-pretty `
        --read (Join-Path $projectRoot "AGENTS.md") `
        @AiderArguments
    $aiderExitCode = $LASTEXITCODE
} finally {
    if ($null -eq $previousDeepSeekApiKey) {
        Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
    } else {
        $env:DEEPSEEK_API_KEY = $previousDeepSeekApiKey
    }
    if ($null -eq $previousPythonIoEncoding) {
        Remove-Item Env:PYTHONIOENCODING -ErrorAction SilentlyContinue
    } else {
        $env:PYTHONIOENCODING = $previousPythonIoEncoding
    }
    if ($null -eq $previousPythonUtf8) {
        Remove-Item Env:PYTHONUTF8 -ErrorAction SilentlyContinue
    } else {
        $env:PYTHONUTF8 = $previousPythonUtf8
    }
    [Console]::OutputEncoding = $previousOutputEncoding
    $deepSeekApiKey = $null
}

exit $aiderExitCode
