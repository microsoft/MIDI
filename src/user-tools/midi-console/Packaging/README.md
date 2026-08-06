# Windows MIDI Console — MSIX packaging

Builds **Windows MIDI Console** as a signed, self-contained, full-trust MSIX bundle containing
both **x64** and **ARM64** packages.

| Property | Value |
| --- | --- |
| Display name | Windows MIDI Console |
| Executable | `midi.exe` |
| Launch alias | `midi` (works from any directory, any console) |
| Trust level | Full trust (`runFullTrust`) |
| Runtime | Self-contained — no .NET install required |
| Architectures | x64, ARM64 (no x86) |
| Minimum OS | Windows 11 25H2 (build 10.0.26200.0) |

## Files

| File | Purpose |
| --- | --- |
| `Package.appxmanifest` | Manifest **template**. `$Version$`, `$Architecture$` and `$Publisher$` are replaced at build time. |
| `Build-Msix.ps1` | Publish → pack → bundle → sign pipeline. |
| `New-PlaceholderImages.ps1` | Generates placeholder tile/logo PNGs into `Images`. |
| `Images/` | Package logo assets. |

## Building

Developer build, signed with a self-signed test certificate:

```powershell
.\Build-Msix.ps1
```

Release build, signed with the real certificate:

```powershell
.\Build-Msix.ps1 -Version 1.2.0.0 -CertificatePath C:\certs\midi.pfx -CertificatePassword $pw
```

CI build where signing is a separate secured step:

```powershell
.\Build-Msix.ps1 -Version 1.2.0.0 -SkipSigning
```

Output lands in `Packaging\output`:

- `WindowsMidiConsole-<version>.msixbundle` — the shippable artifact
- `WindowsMidiConsole-<version>-x64.msix`, `...-arm64.msix` — the individual packages

Requires the **Windows SDK** (for `makeappx.exe` and `signtool.exe`) and the **.NET 10 SDK**.

## Installing a development build

A self-signed certificate is not trusted by default, so the package will not install until the
certificate is placed in **Local Machine → Trusted People**. From an elevated PowerShell prompt:

```powershell
$cert = Get-ChildItem Cert:\CurrentUser\My |
    Where-Object { $_.Subject -like 'CN=Microsoft Corporation*' } |
    Select-Object -First 1

Export-Certificate -Cert $cert -FilePath .\midi-dev.cer
Import-Certificate -FilePath .\midi-dev.cer -CertStoreLocation Cert:\LocalMachine\TrustedPeople

Add-AppxPackage .\output\WindowsMidiConsole-1.0.0.0.msixbundle
```

Then, from any new console window:

```powershell
midi --help
```

## Before shipping — TODO

1. **Replace the package identity.** `Package.appxmanifest` currently uses the placeholder
   `Name="Microsoft.WindowsMidiConsole"`. Substitute the identity assigned by Partner Center.
2. **Confirm the publisher.** The `-Publisher` value passed to `Build-Msix.ps1` must match the
   signing certificate subject *exactly*, and must match the Partner Center publisher ID.
   A mismatch causes install failures.
3. **Replace the placeholder art** in `Images/` with real branding assets.

## Notes

- `AppListEntry="none"` keeps this command-line tool out of the Start menu app list. Remove that
  attribute in `Package.appxmanifest` if you would rather it appear there.
- `uap10:Subsystem="console"` keeps stdin/stdout/stderr attached to the launching console window,
  which is what makes the `midi` alias behave like a normal CLI tool.
- **`runFullTrust` is required for arbitrary file system access.** Several commands accept a
  user-supplied path anywhere on disk:

  | Command | Option | Access |
  | --- | --- | --- |
  | `endpoint send-sysex-file` | input file | read |
  | `endpoint send-message-file` | input file | read |
  | `endpoint monitor` | `-c`, `--capture-to-file` | write |
  | `diagnostics report` | output file | write |

  A full-trust desktop app in an MSIX package runs outside AppContainer, so it reads and writes
  with the user's normal token and these paths keep working. Do **not** add the
  `broadFileSystemAccess` capability — that is a UWP/AppContainer capability, it is unnecessary
  here, and it invites extra Store review.
- MSIX applications always launch at **medium integrity**, and Windows offers no supported way to
  launch a packaged app elevated. For that reason this app intentionally contains no commands
  requiring administrator rights; service start/stop/restart and start-type changes are done with
  the standard Windows tooling (`services.msc`, `sc.exe`, `Start-Service`, `Stop-Service`).
- The package version's fourth field must be `0`; the Store reserves the revision field.
