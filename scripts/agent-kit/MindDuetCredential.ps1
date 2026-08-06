function Initialize-MindDuetCredentialType {
    if ("MindDuet.AgentKit.NativeCredential" -as [type]) {
        return
    }

    Add-Type -TypeDefinition @"
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text;

namespace MindDuet.AgentKit {
    public static class NativeCredential {
        private const UInt32 GenericCredential = 1;
        private const UInt32 PersistLocalMachine = 2;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct Credential {
            public UInt32 Flags;
            public UInt32 Type;
            public string TargetName;
            public string Comment;
            public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
            public UInt32 CredentialBlobSize;
            public IntPtr CredentialBlob;
            public UInt32 Persist;
            public UInt32 AttributeCount;
            public IntPtr Attributes;
            public string TargetAlias;
            public string UserName;
        }

        [DllImport("advapi32.dll", EntryPoint = "CredReadW", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CredRead(string target, UInt32 type, UInt32 flags, out IntPtr credential);

        [DllImport("advapi32.dll", EntryPoint = "CredWriteW", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool CredWrite(ref Credential credential, UInt32 flags);

        [DllImport("advapi32.dll")]
        private static extern void CredFree(IntPtr credential);

        public static string Read(string target) {
            IntPtr pointer;
            if (!CredRead(target, GenericCredential, 0, out pointer)) {
                int error = Marshal.GetLastWin32Error();
                if (error == 1168) {
                    return null;
                }
                throw new Win32Exception(error);
            }

            try {
                Credential credential = (Credential)Marshal.PtrToStructure(pointer, typeof(Credential));
                byte[] bytes = new byte[credential.CredentialBlobSize];
                if (bytes.Length > 0) {
                    Marshal.Copy(credential.CredentialBlob, bytes, 0, bytes.Length);
                }
                return Encoding.UTF8.GetString(bytes);
            } finally {
                CredFree(pointer);
            }
        }

        public static void Write(string target, string userName, string secret) {
            byte[] bytes = Encoding.UTF8.GetBytes(secret ?? "");
            if (bytes.Length > 2560) {
                throw new ArgumentException("Credential is too large for Windows Credential Manager.");
            }

            IntPtr blob = Marshal.AllocHGlobal(bytes.Length == 0 ? 1 : bytes.Length);
            try {
                if (bytes.Length > 0) {
                    Marshal.Copy(bytes, 0, blob, bytes.Length);
                }

                Credential credential = new Credential();
                credential.Type = GenericCredential;
                credential.TargetName = target;
                credential.CredentialBlobSize = (UInt32)bytes.Length;
                credential.CredentialBlob = blob;
                credential.Persist = PersistLocalMachine;
                credential.UserName = userName;

                if (!CredWrite(ref credential, 0)) {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            } finally {
                Marshal.FreeHGlobal(blob);
            }
        }
    }
}
"@
}

function Get-MindDuetCredential {
    param([Parameter(Mandatory = $true)][string]$Target)

    Initialize-MindDuetCredentialType
    return [MindDuet.AgentKit.NativeCredential]::Read($Target)
}

function Set-MindDuetCredential {
    param(
        [Parameter(Mandatory = $true)][string]$Target,
        [Parameter(Mandatory = $true)][string]$UserName,
        [Parameter(Mandatory = $true)][string]$Secret
    )

    Initialize-MindDuetCredentialType
    [MindDuet.AgentKit.NativeCredential]::Write($Target, $UserName, $Secret)
}
