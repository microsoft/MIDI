---
name: midi-contributing
description: >
  Write, review or submit a code change to Windows MIDI Services (microsoft/MIDI).

  USE FOR: implementing a fix or feature in this repository; deciding whether a change needs a
  servicing gate (KIR); reviewing a diff or a pull request; adding or changing user-visible
  strings; adding a transport, tool, test or sample; preparing a change for submission.

  DO NOT USE FOR: scoping or filing a defect before it is understood (use midi-bug-reports);
  documentation-only edits under docs/ that touch no code; using the shipped SDK in your own app.
---

# Contributing code to Windows MIDI Services

This code runs inside a Windows service, inside a kernel-mode-adjacent driver stack, and inside
every music app on the machine. A crash here takes out MIDI for the whole PC, and much of it
ships in Windows, where a bad change has to be serviced rather than simply patched.

Read [CONTRIBUTING.md](../../../CONTRIBUTING.md) for process, licensing and the CLA.
This skill is about the change itself.

## Before you write anything

1. **Find out whether the code you are about to touch ships in Windows.** That single fact
   determines whether you need a servicing gate, and it is the most commonly missed requirement in
   this repository. See [references/kir-servicing.md](references/kir-servicing.md).
2. **Do not change public API surface without asking first.** The `Windows.Devices.Midi2` WinRT
   API ships in box and shipping applications have taken a dependency on it. Adding a method,
   changing a signature, renaming a parameter or renumbering an enum can break an already-shipped
   application. Propose it in the issue or PR and let the maintainers decide.
3. **New external dependencies must be declared explicitly** and must be available as a vcpkg
   port, because the internal build system that signs and ships this code requires it. For JSON,
   the only supported library is `Windows.Data.Json`.
4. **Match the surrounding code.** This repository has strong local conventions per project
   (naming, tracing, error codes, resource ids). Copy the neighboring file rather than importing a
   style from elsewhere.

## The five rules that get broken most often

### 1. Changes to shipping code must be wrapped in a servicing gate (KIR)

Any change to code that has already shipped in Windows must be selectable at run time, so it can
be rolled back in servicing without rebuilding. The trigger is the **location of the file**, not
the size of the diff: a one-line fix in `midisrv` still needs a gate.

Only two things decide whether the gating is right:

1. **With the gate disabled, the behavior and the code is exactly what shipped.**
2. **The result still reads sensibly to a human.**

Where the branch goes follows from those, and depends on the shape of the change:

| The change | Where the gate goes |
|---|---|
| Changes a function's signature | The call site. There is no other option — a signature cannot be conditional. |
| Would need several `if (Feature_Servicing_...)` branches inside one function | Add a net-new function beside the old one and branch once at the call site |
| A small piece of code inside an existing function | Branch in place, inside the function. That is fine. |
| Rewrites how a whole class behaves | Build a net-new class beside the old one and choose between them in the factory that constructs it |

The call-site form, where that is the right choice:

```cpp
if (Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock::IsEnabled())
{
    RETURN_IF_FAILED(table->OnDeviceDisconnectedAlwaysTeardown(m_endpointId));
}
else
{
    RETURN_IF_FAILED(table->OnDeviceDisconnected(m_endpointId));
}
```

Whichever form you pick: never combine another condition with the gate check in one `if` — nest
instead. Never rename an existing function; add a net-new one beside it, or the rollback path
disappears. The `else` branch must be the original code, unchanged, byte-for-byte, except for the new indent inside the braces.

Full rules, the shipping/not-shipping list, test gating and how to prove the gate works:
[references/kir-servicing.md](references/kir-servicing.md).

### 2. SAL-annotate every function parameter

Every named parameter on every function declaration gets a SAL annotation: `_In_`, `_In_opt_`,
`_Out_`, `_Out_opt_`, `_Inout_`, `_In_reads_(n)`, `_Out_writes_(n)` and so on. This is what lets
static analysis find the buffer and null-pointer defects that reviewers will not.

When a function is declared in a header and defined in a `.cpp`, the **definition uses
`_Use_decl_annotations_` and bare parameters** — do not repeat the annotations:

```cpp
// MidiEndpointTable.h
HRESULT OnDeviceDisconnectedAlwaysTeardown(_In_ std::wstring const deviceEndpointInterfaceId) noexcept;

// MidiEndpointTable.cpp
_Use_decl_annotations_
HRESULT MidiEndpointTable::OnDeviceDisconnectedAlwaysTeardown(std::wstring const deviceEndpointInterfaceId) noexcept
{
```

For a function defined inline in the header, annotate the parameters there directly. Annotate COM
method declarations too, including inside `STDMETHOD(...)`. Lambda parameters are not annotated.

### 3. en-US spelling everywhere, including identifiers

en-GB spellings are defects, not style preferences — in UI strings, `.resw` and `.rc` values,
`#define` and variable names, comments, documentation and commit messages alike. The forms that
actually get through are the `-our`, `-ise`/`-isation`, `-re`, `-ce` and doubled-`l` groups; the
full do-not-write table is in the instruction file linked below.

```powershell
pwsh -File build\check_en_us_spelling.ps1 -Path <file or folder>
```

Run it before handing work back; it exits non-zero on a hit and understands `SNAKE_CASE` and
`camelCase`, so it catches API-shaped identifiers that a plain word search misses. Rules and the
word list:
[.github/instructions/en-us-spelling.instructions.md](../../instructions/en-us-spelling.instructions.md).

### 4. User-facing strings live in resource files

Never hardcode a string a human will read. The project is localized, and a literal in code cannot
be translated.

| Project type | Where the strings go | How to read them |
|---|---|---|
| WinUI 3 tools | `Strings\en-US\Resources.resw`, keyed by `x:Uid` | `x:Uid` in XAML, `StringResources::GetString` / `FormatString` in code |
| Console tools, service, transports | `resource.h` (`IDS_*`) + `Resources.rc` `STRINGTABLE` | `internal::ResourceGetWString(IDS_X)` / `ResourceGetHString` |

- Format with `std::format`-style `{0}` `{1}` placeholders, not `%1`. Keep any trailing space
  inside the resource string.
- A `.resw` key prefix matching an `x:Uid` is not enough: every localizable property needs its own
  key (`MyBox.Header`, `MyBox.PlaceholderText`, ...), or the literal in the XAML ships
  untranslatable.
- Renaming a resource key means changing it on **both** sides. A one-sided rename compiles and
  then fails at run time with a missing (empty) string. If a string renders blank, the id is
  missing from the `.rc`/`.resw`, not from your logic.
- Do **not** localize machine-parseable output: `mididiag` field and section labels, field values,
  device ids, and anything an app or script parses stay as constants.
- Adding entries to a `.rc` or `.resw` needs no servicing gate — resources are inert data. The
  code that *reads* them is the change, and in shipping code that does need one.
- Do **not** localize messages in a Trace Logging statement

### 5. Review your own change for the failure classes that hurt here

Before submitting, walk your diff against this list. Every one of these has caused a real,
customer-visible defect in this project.

- **Use after free** — a `[this]` capture in an event handler, a coroutine parameter taken by
  reference, or a callback invoked after the owner was torn down.
- **Time of check to time of use** — a lock released between validating state and acting on it; a
  path validated and then re-opened; a "still pending?" check that is not part of the same
  atomic transition.
- **Escaping exceptions** — an exception leaving an `HRESULT`-returning function, a thread body, a
  `noexcept` function or a `fire_and_forget` coroutine terminates the process. In `midisrv` that
  is every MIDI app on the machine.
- **A module missing `<wil\cppwinrt.h>`** — WIL does not recognize `winrt::hresult_error`, treats
  it as unrecoverable and **fail-fasts**, so every `CATCH_LOG()` and `CATCH_RETURN()` in the binary
  becomes a crash site instead of a handler. Any module using both WIL and C++/WinRT needs the
  include in its precompiled header, first among the WIL includes.
- **Locks** — SRW locks are not recursive in any combination, including taking shared after
  exclusive on the same thread. Never call out (callback, COM, network, WinRT async) while holding
  a lock. Keep one global lock order.
- **Blocking inside an event handler or callback** — calling `.get()` on a WinRT async operation
  from a device watcher, DevQuery or socket callback holds a system callback thread. The watcher
  can be aborted for exceeding its notification budget and then goes permanently and silently
  blind; on an STA the same call trips a blocking-wait check or deadlocks outright. Queue the work
  to your own worker and return in microseconds.
- **Untrusted input** — the configuration file is writable by a standard user and the service runs
  as `LOCAL SERVICE`. Treat config JSON, device-supplied names and network packets as hostile.
- **Narrow/wide string conversion by iterator range** — `std::wstring(s.begin(), s.end())` on a
  UTF-8 string makes one `wchar_t` per *byte*, so every character outside ASCII becomes several
  garbage ones; `std::string(ws.begin(), ws.end())` truncates each `wchar_t` to a byte and loses
  data silently. Names reach us from devices, from the network and from users, so ASCII is not a
  safe assumption. Use `winrt::to_hstring` / `winrt::to_string`, `MultiByteToWideChar(CP_UTF8, …)`
  or the helpers in `Inc/wstring_util.h`. This shipped in the WinMM port names and reached
  customers. Beware that it hides in test helpers too: a helper that mangles the string on the way
  in makes the assertion compare mangled to mangled and pass.
- **Silent failure** — a `catch` that returns a default, a fallback that logs nothing, a wrong-type
  JSON value that quietly disappears. A defect you cannot see in a trace is worse than a crash.

The concrete patterns, the idioms that look correct and are not, and what to grep for:
[references/code-safety-review.md](references/code-safety-review.md).

## Verify before you hand it back

A change is not done because it compiles. In order:

1. It builds for **every** platform and configuration the project supports, not just x64 Release.
2. The relevant test suites pass, **and you rebuilt the tests**, not only the code under test.
3. The binary you tested is the binary you built — check timestamps or hashes, do not assume.
4. If you gated the change, you proved the gate by turning it off, rebuilding and re-running.
5. `build\check_en_us_spelling.ps1` is clean over what you touched.
6. `git status` is clean of incidental files, and `git diff` shows only what you meant to change.

Build commands, test runners, the stale-binary traps and how to prove a deployment is live:
[references/build-and-verify.md](references/build-and-verify.md).

## What to say in the pull request

- Which files ship in Windows, and the servicing gate name covering them — or why none is needed.
- What you built and what you ran, including platforms and configurations, and the pass counts.
- **What you did not verify.** Missing hardware, an elevated step you could not perform, a
  platform you could not build. An honest gap is more useful than a confident summary.
- Any new external dependency, any change to public API surface, and any new file that has to be
  installed or registered.
- Tag AI-generated analysis as **"AI Generated Content"**, as the issue forms require.

## Things not to do

- Do not add features, refactors or "improvements" beyond the change being asked for. Churn in
  shipping code costs a servicing gate for no benefit.
- Do not fix a defect you have not reproduced. Scope it first with the
  **midi-bug-reports** skill.
- Do not delete or rename shipped API members to fix them. Public surface is additive only.
- Do not put message payload data in retail tracing. Pointers, byte counts, device ids and names
  are fine; the contents of a MIDI message are a privacy violation outside `_DEBUG`.
- Do not add tracing to the per-message hot path.
