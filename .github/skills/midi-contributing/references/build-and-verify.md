# Building, testing and verifying a change

## Solutions

| Solution | What it builds |
|---|---|
| `src/in-box/Midi2.sln` | The service, transports, transforms, WinMM client, shared libraries and their tests |
| `src/in-box/Midi2-AppSDK.sln` | The `Windows.Devices.Midi2` WinRT SDK, the console, the GUI tools and the SDK tests |
| `src/in-box/Drivers/USBMIDI2/Driver/USBMidi2.sln` | The USB MIDI 2.0 class driver |
| `src/installers/*.sln` | The installers and bundles |

## Building a single project

```powershell
& $msbuild <project>.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64 `
    "/p:SolutionDir=<repo>\src\in-box\\" /v:minimal /nologo
```

- **`SolutionDir` is required**, including the trailing double backslash, or the build fails with
  `C1083: Cannot open include file: 'MidiDefs.h'`. This applies to managed projects that reference
  a native project too.
- Solution-level `/t:ProjectName` does not work on these solutions. Build the project file.
- **Static libraries are not rebuilt by their dependents.** Build the library first, then everything
  that links it. Changing a shared static library means rebuilding the service, several transports
  and the WinMM driver.
- Filter output with `": error C|: fatal error|: error MSB|: error LNK"`. Do not grep bare `error`
  — several source files have `Error` in their names and flood the match.
- Redirect long builds to a log file; the output is large enough to be truncated inline.
- When `/WX` promotes a warning, the first error MSBuild prints is often not the useful one. Read
  the full `.log` under the project's intermediate directory.

### Build failures that are not your change

| Symptom | Cause | Fix |
|---|---|---|
| `MSB6003 ... .tlog being used by another process` | Orphaned MSBuild nodes from an interrupted build | Stop the MSBuild processes, rebuild with `/nodeReuse:false`. Do not background long solution builds. |
| `MIDL2011: unresolved type declaration Microsoft.UI.Xaml.*` | A deleted `obj\` took `project.assets.json` with it, so package targets never imported | `msbuild <sln> /t:Restore`, then build |
| `C1301: error accessing program database ... .ipdb, invalid format` | Stale incremental link database | Delete the `.ipdb`/`.iobj` and rebuild |
| `LNK1104: cannot open file ....dll` | Transient file lock | Re-run the build |
| `C2259: cannot instantiate abstract class` after an IDL edit | Stale generated header, or a trailing `const` MIDL discarded | Compare header and `.idl` timestamps; rebuild the IDL project per platform |

## Running tests

Tests are TAEF:

```
"C:\Program Files (x86)\Windows Kits\10\Testing\Runtimes\TAEF\x64\TE.exe" <test>.dll /logOutput:Low
```

- `/logOutput:High` to see `Log::Comment` output.
- `/select:` and `/name:` filters get mangled by PowerShell quoting. Put them in a temporary `.cmd`
  file. Repeating `/name:` does not union the filters — the last one wins.
- **Run both sets.** Service and transport suites live under `src/in-box/VSFiles/<plat>/<cfg>/`;
  SDK suites live under `src/in-box/vsfiles-sdk/out/tests/<plat>/<cfg>/`. A change validated
  against only one set has shipped a break in the other.

### Rebuild the tests, not just the code under test

The SDK binary is resolved from the test folder, and that copy is refreshed **only when a test
project is built**. Building the SDK alone leaves the tests running the old SDK, so the results are
meaningless and look like "my fix did nothing".

If every `Midi2.WinRTClient.*` suite fails with `Class not registered [0x80040154]`, that means
*rebuild the tests*, not "registration is broken on this machine".

After building, confirm the two copies match:

```powershell
(Get-FileHash $sdkBuildOutput).Hash -eq (Get-FileHash $sdkCopyInTestFolder).Hash   # must be True
```

The same class of stale-binary trap applies to tools that consume the SDK. An
`InvalidCastException` from an SDK call means the projection was regenerated against a newer
metadata file than the DLL sitting next to the executable.

## Writing tests

- **Never let a test pick an arbitrary MIDI device.** Target a loopback endpoint by id. "First in
  the list" has landed on a real instrument mid-firmware-update.
- **Clean up anything written to the running service.** A configuration payload goes to the live
  service, not to a file, and survives until restart. Use a `wil::scope_exit` per test *and* sweep
  the same ids again in class cleanup, because a skipped or blocked test never runs its own
  cleanup.
- **Watch cross-test coupling.** A test that leaves a timing value at its floor gives every later
  test an unusually aggressive background scanner.
- **Count what you mean to count.** Callbacks are not messages: the cross-process reader coalesces
  queued messages that share a timestamp into one callback with a larger payload, so counting
  callbacks under-counts. Derive the count from the payload size and make the completion trigger
  `>=`, not `==`.
- **Do not call `uninit_apartment` in a test.** The test host already initialized the apartment;
  tearing it down breaks every later test in the process.
- **Implement `QueryInterface` properly even in test objects.** Returning `S_OK` for everything
  claims `IMarshal` and `IAgileObject` and never writes the out pointer.
- **Assert the specific error code**, not just failure — see the discriminating-test note in
  [kir-servicing.md](kir-servicing.md).
- **Benchmark Release, never Debug.** The same list operation measured 4,731 ms in Debug and 232 ms
  in Release. Do not design around a Debug number.

### Test failures that are environmental

- Repeated test runs accumulate deactivated software device nodes. Thousands of them make raw
  device enumeration take seconds and time out watcher tests. Count the stale nodes and split live
  from deactivated before blaming a change. `build\remove_deactivated_midi_devices.ps1` cleans them
  up; run it more than once.
- Endpoint enumeration speed depends on which build of the service is installed. Poll for a
  non-empty list rather than sleeping for a fixed interval.

## Deploying to test against the running service

Deployment needs elevation. Most binaries go to `System32`; **some transports load from
`C:\Program Files\Windows MIDI Services\Service\` instead**, and there are separate helper scripts
under `build\` for those. A stale copy of one of them makes the service answer
"Unrecognized command." to any newer verb.

Confirm the real load path from the CLSID's `InprocServer32` registry value rather than assuming
`System32`, and hash- or timestamp-check the deployed file before believing any test result.

## Verifying a service-crash fix

"The tests passed" is not sufficient — the RPC returns before the service dies, so tests report
success while the service is crashing underneath them.

Capture the service process id before and after, plus a fault count since a timestamp mark, and
check the Application event log's **faulting module** field.

## Before you hand the change back

```powershell
pwsh -File build\check_en_us_spelling.ps1 -Path <files or folders you touched>
git status
git diff
```

- Running any build target rewrites a tracked generated version header with today's date. Check for
  it in `git status` and revert it, or it rides along in your diff.
- Review the diff. It is what catches an edit that landed at the wrong offset, and it is cheap.
- Confirm the built binary is **newer than the source you changed** before interpreting any test
  result. A passing test against a four-minute-old executable has proved nothing.
- State plainly what you could not verify: a platform you did not build, an elevated step you could
  not run, hardware you do not have.
