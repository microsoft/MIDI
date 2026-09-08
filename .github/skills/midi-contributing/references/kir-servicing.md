# Servicing gates (KIR) for shipping code

A **KIR** ("known issue rollback") gate makes a behavior change selectable at run time, so that if
the change causes a regression in the field it can be turned back off through servicing without
shipping new binaries. Windows MIDI Services ships in Windows, so any change to code that has
already shipped needs one.

## Check this before you edit, not after

The trigger is the **location of the file**, not the size or risk of the diff. A one-line fix in
`midisrv` is a shipping change. So is swapping a hardcoded English literal for a resource string,
even though the en-US output is byte-identical — what matters is that the changed code lives in
shipping code, not how visible the behavior change is. Do not reason about severity to talk
yourself out of a gate.

## Is this code shipping?

Shipping in Windows today, so a gate is required:

| Area | Path |
|---|---|
| The service | `src/in-box/Service/` |
| Shared libraries | `src/in-box/Libs/*` |
| Shared headers | `src/in-box/Inc/*` |
| Drivers | `src/in-box/Drivers/*` |
| Message transforms | `src/in-box/Transform/*` |
| KS, KS Aggregate, Loopback, Virtual, Diagnostics transports | `src/in-box/Transport/{KSTransport,KSAggregateTransport,LoopbackMidiTransport,VirtualMidiTransport,DiagnosticsTransport}` |
| WinMM client | `src/in-box/Client/WinMM/` |

Not shipping in Windows today, so no gate is needed:

- The `Windows.Devices.Midi2` WinRT SDK (`src/in-box/Client/WinRT/core`)
- Transports still in preview — confirm the current list before relying on this
- User tools under `src/in-box/user-tools/`
- Everything under `src/in-box/Test/`, samples, `build/` and `docs/`

**This list moves.** Transports graduate into Windows release by release. If you are not certain
whether the file you are changing has shipped, ask in the issue or PR rather than guessing — the
cost of an unnecessary gate is small, and the cost of a missing one is a change that cannot be
rolled back.

### The nuance that catches people

When you add a **net-new function to a shipping library** and gate the choice at the call sites,
gate **every** call site — including call sites in projects that do not ship. What is being gated
is the behavior of the shared function, not the caller. Turning the gate off then restores the old
behavior uniformly everywhere.

## What the gating has to achieve

There are only two requirements, and every other rule below exists to serve them:

1. **With the gate disabled, the code behaves exactly as it shipped.**
2. **The result still has a structure a human can read.**

Anything that satisfies both is acceptable gating. Anything that satisfies only the first is a
maintenance problem, and anything that satisfies only the second is not a rollback.

## Where to put the branch

This is a judgment call about the shape of the change, not a fixed rule. Four common shapes:

| The change | Where the gate goes | Why |
|---|---|---|
| Changes a function's signature | The **call site** | A signature cannot be conditional. There is no other option. |
| Would need several `if (Feature_Servicing_...)` branches inside one function | Add a **net-new function** beside the old one, branch once at the call site | Several gate checks threaded through one body fails requirement 2, and makes it hard to see that the disabled path is intact |
| A small piece of code inside an existing function | **In place**, inside the function | Promoting a few lines to a new function to satisfy a rule adds indirection for nothing |
| Rewrites how a whole class behaves | A **net-new class** beside the old one, chosen in the factory that constructs it | Gating a restructured 3,000-line file in place would mean dozens of sites; one check at the factory is far easier to verify |

The last row is not hypothetical — the KS Aggregate endpoint manager rewrite was done exactly that
way, with `...Manager3` alongside `...Manager2` and a single `IsEnabled()` check in
`TransportState::ConstructEndpointManager()`. Carry every pre-existing gate in the old class over
to the new one, and note deliberately dropped ones in the write-up. A change of that size is a
maintainer conversation, not a decision to take alone.

### In-place gating: what makes it safe

When you gate inside a function, the `else` branch must reproduce the original body verbatim — not
"equivalently". A shared JSON command helper used by four shipping transports is gated this way:
both function bodies end in a single return, so one wrap in one header covers roughly ten call
sites, and the disabled path is byte-for-byte what shipped. That is the test to apply.

If you find yourself adding a **second** gate check to the same function, stop and promote it to a
net-new function instead.

## The rules that hold regardless of shape

1. **Never combine another condition with the gate check in one `if`.** Nest instead. A reviewer
   must be able to see the gate without untangling boolean logic.
2. **A net-new function does not gate itself.** What gets gated is the choice between it and the
   original, wherever you made that choice.
3. **Do not rename an existing function.** Add a net-new one alongside it and choose between them.
   Renaming makes the rollback path disappear.
4. **The disabled path must reproduce the original exactly.**
5. **The wrapping must be obvious to a human reviewer.** That is requirement 2, restated.
6. **One gate = one rollback unit = one concern.** Do not reuse a gate for an unrelated fix, or
   rolling back one drags out the other.
7. **A follow-on defect attributable to a gated change ships under that same gate.** Otherwise the
   gate gets rolled back as the apparent cause, and the original defect returns with it.

### An example of the call-site form

```cpp
if (m_isDeviceSide)
{
    if (Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock::IsEnabled())
    {
        RETURN_IF_FAILED(table->OnDeviceDisconnectedAlwaysTeardown(m_endpointId));
    }
    else
    {
        RETURN_IF_FAILED(table->OnDeviceDisconnected(m_endpointId));
    }
}
```

Note the nesting: `m_isDeviceSide` is a separate `if`, not `&&`-ed into the gate check.

### The header

One header per gate, in `src/in-box/Inc/Feature_Servicing_<Name>.h`:

```cpp
#pragma once

class Feature_Servicing_MIDI2MyChangeName
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2MyChangeName_IsEnabled()
{
    return true;
}
```

Copy an existing header verbatim for the file banner and naming style. Names are
`Feature_Servicing_MIDI2` followed by a short description of the fix, not of the bug number.

### When a gate is impossible

Some changes have no place to put a branch at all. Adding `#include <wil/cppwinrt.h>` to a module
installs a process-wide exception translator hook and changes every catch site in the binary at
once. Such a change has to ship ungated and be reported as such. Say so explicitly in the PR; do
not quietly skip the gate.

## Tests must be gated too

For shipping code, the same gate that selects the behavior must gate the test. The test has to
**no-op and pass trivially** when the gate is disabled, so that a rollback does not turn the suite
red:

```cpp
if (!Feature_Servicing_MIDI2MyChangeName::IsEnabled())
{
    Log::Comment(L"KIR disabled, skipping.");
    return;
}
```

Keep any "the old behavior is preserved" test **ungated**: it must pass in both states.

Write the test so it discriminates. A malformed input often already fails before your fix, just
with a different error, so `VERIFY_IS_FALSE(IsSuccess())` passes either way and proves nothing.
Assert the specific error code, and run the test against the unpatched binary to confirm it goes
red.

## Prove the gate, do not assume it

1. Flip `IsEnabled()` to `false`.
2. Rebuild **every** consumer of the header, not just the project you changed.
3. Re-run the suites. Expect: gated tests skip, the "old behavior preserved" test still passes,
   zero failures, everything compiles.
4. Flip it back and rebuild.

If your change introduces two gates, build the full on/off **matrix** (two gates = four builds),
not each one alone.

## The internal write-up

A gated change is accompanied by an internal bug write-up with exactly three parts:

- **Problem** — what is wrong, and the evidence for it. Evidence belongs here, not in its own
  section.
- **Fix** — what changed, and the gate name.
- **Verification** — the repro, and what to observe with the gate on versus off.

`mididiag` prints the list of gates present on the machine, with a short human-readable
description, so give the gate a description that means something to somebody triaging a support
case.
