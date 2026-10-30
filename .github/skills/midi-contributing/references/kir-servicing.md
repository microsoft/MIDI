# Servicing gates (KIR)

This is the one place the servicing gate rules live. Other documents link here instead of repeating them.

A **KIR** ("known issue rollback") gate makes a change selectable at run time. If the change causes a regression in the field, servicing turns it off without shipping new binaries. A few months later the gate is retired: an automated cleanup keeps the gated code, deletes the original, and deletes the gate header.

## Every gate has to do three things

1. **With the gate off, the code behaves exactly as it shipped.**
2. **A person can read it.** The gate and the original code are easy to see.
3. **It cleans up by itself.** The cleanup understands one shape only: `if (gate) { code to keep } else { code to delete }`. Anything else is left behind as dead code, or as comments that no longer make sense, and someone has to remove it by hand.

Every rule below serves one of these three.

## Does the change need a gate?

The trigger is **where the file lives**, not how big or risky the change is. A one-line fix in `midisrv` needs a gate. So does swapping an English literal for a resource string with the same text.

| Ships in Windows: gate required | Path |
|---|---|
| The service | `src/in-box/Service/` |
| Shared libraries | `src/in-box/Libs/*` |
| Shared headers | `src/in-box/Inc/*` |
| Drivers | `src/in-box/Drivers/*` |
| Message transforms | `src/in-box/Transform/*` |
| KS, KS Aggregate, Loopback, Virtual, Diagnostics transports | `src/in-box/Transport/{KSTransport,KSAggregateTransport,LoopbackMidiTransport,VirtualMidiTransport,DiagnosticsTransport}` |
| WinMM client | `src/in-box/Client/WinMM/` |

No gate needed:

- The `Windows.Devices.Midi2` SDK (`src/in-box/Client/WinRT/core`), transports still in preview, `src/in-box/user-tools/`, `src/in-box/Test/`, samples, `build/` and `docs/`.
- **Brand-new code** in a shipping area, unless it calls gated code. There's no shipped behavior to go back to. Writing a property that gated code happens to read is not a dependency.
- **A new `#define`**, or new `.rc`/`.resw` entries. They do nothing at run time. Changing the value of an existing `#define` does need a gate; add a new one beside it instead. The code that reads a new resource is a change and needs a gate.

This list moves as transports graduate into Windows. If you aren't sure a file has shipped, ask. A gate you didn't need costs little; a missing one can't be rolled back.

A few changes have nowhere to put a branch. Adding `#include <wil/cppwinrt.h>` to a module changes every catch site in the binary at once. Ship those ungated and say so in the PR.

## Shape every gate the same way

```cpp
if (Feature_Servicing_MIDI2MyChange::IsEnabled())
{
    // the new code
}
else
{
    // the original code, unchanged except for the indent
}
```

- **Always write the `else`**, even when the new branch ends in `return`. `if (gate) { return NewThing(); }` followed by the original code leaves a function that calls another function and then a pile of dead code once the gate is retired. Put the original in the `else`.
- **The `else` is the original code, byte for byte.** Not "equivalent". Don't let it depend on a variable the gated code may have changed; restore the original expression literally.
- **The gate check stands alone in its `if`.** Never `&&` or `||` it with anything. Nest other conditions inside or outside it, and put the gate on the outer `if` when you can.
- **No `else if` after a gate.** Neither `if (gate) { } else if (otherGate) { }` nor `if (gate) { } else if (someCondition) { }` cleans up. Put the other condition inside the `else` block instead.
- **`if (!gate)` is fine.** The cleanup understands it, so the test skip below works.
- **Kernel-mode drivers use the C form**, `Feature_Servicing_MIDI2MyChange_IsEnabled()`. Everything else uses `Feature_Servicing_MIDI2MyChange::IsEnabled()`. The cleanup understands both.

## Gate in place by default

Put the branch inside the existing function, around the code that changes. Several gate checks in one function are fine if each has this shape. The cleanup removes them all.

Adding a new function or class beside the old one leaves the old one behind as dead code, and calls through to it as needless layers. **Don't do it unless a maintainer agrees for this change.** The exceptions:

- **A changed signature** can only be gated at the call site, by calling a new function.
- **A rewrite of a whole class** may be a new class chosen once in the factory that builds it, as the KS Aggregate `...Manager3` beside `...Manager2` was. This is the right call for a very large change, but the old class has to be removed by hand, so mark it. Carry every existing gate in the old class into the new one.

When you do add a replacement:

- **Mark everything that dies with the gate**, so the person removing the gate can find it by searching for the gate name. That means the old function's declaration and definition, and any members, locks and includes only the old code uses:

  ```cpp
  // BEGIN removal of Feature_Servicing_MIDI2MyChange
  HRESULT DoWork(_In_ std::wstring const id) noexcept;
  // END removal of Feature_Servicing_MIDI2MyChange
  ```

  If code dies only when a different gate is retired, name that gate.
- **Don't rename the existing function.** Add the new one and choose between them at the call site.
- **Name the new function for what it does**, not for how it differs from the old one. Once the old one is gone, names like `Hardened`, `V2` or `OutsideLock` mean nothing.
- **Gate every call site** when the new function is in a shipping library, including callers that don't ship, so turning the gate off restores the old behavior everywhere.

## Write comments that still make sense after cleanup

Comments in the code you keep stay forever, so they describe only that code.

- No gate names, "net-new", "gate on/off", or "the original code".
- No mention of a function that will be deleted. "Same as `DoWork` except…" is wrong as soon as `DoWork` is gone.
- Explain what changed and why in the gate header, the PR and the write-up. The header is deleted with the gate. A note about the old behavior can go inside the `else`, which is also deleted.

## One gate per concern

- **Every new change gets a new gate**, including a fix to code that is already gated. Never add code to an existing gate: it may already be in a Windows build.
- **Related fixes made together share one new gate** and one write-up. Fewer gates is better.
- **Never reuse a gate for unrelated fixes.** Rolling one back would take the other with it.
- **Fix gated code in place.** When a fix lands in a function an earlier gate added, gate it inside that function instead of adding yet another function beside it.

## Apps and tools branch on capability, not on the gate

A gate can take weeks to turn on for a given PC, so a tool that ships with a serviced transport has to work with what is already in Windows. The transport reports each capability, off when its gate is off, and clients branch on that report. A client never tests a `Feature_Servicing_*` symbol.

## The gate header and the mididiag listing

One header per gate, in `src/in-box/Inc/Feature_Servicing_MIDI2<Name>.h`. Copy an existing header for the banner. Name the gate for the fix, not the bug number. The comment in the header explains the problem, the fix, and what rolling it back restores.

```cpp
#pragma once

class Feature_Servicing_MIDI2MyChange
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2MyChange_IsEnabled()
{
    return true;
}
```

Add the gate to `DoSectionFeatureEnablement` in `src/in-box/user-tools/mididiag/mididiag_main.cpp`: an `#include` and an `OutputSingleFeatureEnablement` line with a short description that means something to whoever is triaging a support case. Remove both when a gate is retired. `Test-ServicingGateListing` in `build/build-all.ps1` fails the build when the headers and the listing don't match.

## Tests

The gate that selects the behavior also gates its test. With the gate off, the test does nothing and passes, so a rollback doesn't turn the suite red:

```cpp
if (!Feature_Servicing_MIDI2MyChange::IsEnabled())
{
    Log::Comment(L"Feature_Servicing_MIDI2MyChange is disabled. Skipping.");
    return;
}
```

A test that checks the old behavior still works is not gated; it must pass both ways. Make each test discriminate: assert the specific error code, and confirm it fails against the unfixed binary.

## Check your diff

Run these before you hand the change over:

1. **Every removed line comes back in an `else`.** `git diff -U0 -- src/in-box | Select-String '^-[^-]'`. A removed line that isn't in a gate-off branch is a change the gate can't roll back.
2. **Every gate check stands alone and has a plain `else`.** Search the diff for `Feature_Servicing_` and look for `&&`, `||`, `else if`, and an `if (gate)` block that returns with no `else` after it.
3. **No comment in kept code mentions a gate, "net-new", or a function that will be deleted.**

## Prove the gate works

1. Change `IsEnabled()` to `false`.
2. Rebuild every project that includes the header, not just the one you changed.
3. Run the tests. Gated tests skip, the old-behavior tests pass, nothing fails.
4. Change it back and rebuild.

With two gates, build all four on/off combinations.

## The internal write-up

Each gate comes with an internal bug write-up in three parts, delivered with the code:

- **Problem**: what's wrong, with the evidence.
- **Fix**: what changed, and the gate name.
- **Verification**: the repro, and what you see with the gate on and off.

Include the mididiag description. Don't hard-wrap it: one line per paragraph, bullet and table row.
