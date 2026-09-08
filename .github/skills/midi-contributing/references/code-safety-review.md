# Reviewing your own change: the failure classes that hurt here

Walk your diff against this list before you submit. Each item below has produced a real,
customer-visible defect in this project — a dead service, a lost firmware update, a silent MIDI
port, or an app that will not start.

## 1. Escaping exceptions kill the process

In `midisrv` that means every MIDI application on the machine stops working, and the service
often cannot even be stopped afterward because the wedged threads block `SERVICE_CONTROL_STOP`.

Four places an exception is fatal, and all four appear in this repository's history:

- **An `HRESULT`-returning function that calls throwing code.** Callers use `RETURN_IF_FAILED` /
  `LOG_IF_FAILED` and are entitled to rely on the signature. Wrap the body in `try` +
  `CATCH_RETURN()`. C++/WinRT throws far more readily than it looks: a constructor rejecting a
  malformed string throws `E_INVALIDARG` from deep inside a generated header.
- **A thread body.** `std::thread` and `std::jthread` call `std::terminate` on an escaping
  exception. `LOG_IF_FAILED(Worker())` in the lambda catches HRESULTs, **not exceptions**, and a
  worker started with `std::bind_front(&Class::Worker, this)` has no wrapper at all.
- **A `noexcept` function.**
- **A `fire_and_forget` coroutine.** Its promise type calls `winrt::terminate()`. The process dies
  with nothing logged.

**Put the `try` inside the worker loop, not around the whole function.** A function-level guard
converts "the service crashes loudly" into "the worker thread dies silently and nothing is ever
created again", which is worse. Log and continue per iteration; keep a function-level guard only
as a last resort.

**Do not treat "this function contains a catch" as guarded.** Compare the position of the first
throwing call against the position of the first handler — a handler *below* the throwing call
protects nothing.

### `<wil/cppwinrt.h>` is mandatory in any module using both WIL and C++/WinRT

`winrt::hresult_error` does not derive from `std::exception`. Without `<wil/cppwinrt.h>`, WIL does
not recognize it, treats it as unrecoverable, and **fail-fasts**. Every `CATCH_LOG()`,
`CATCH_RETURN()` and `LOG_CAUGHT_EXCEPTION()` in the module becomes a crash site — the exact
opposite of what the defensive catch was for.

- Include it in `pch.h`/`stdafx.h`, first among the WIL includes. Include order relative to
  C++/WinRT does not fix it; the header is opt-in and installs the translator.
- This repository writes the include with a backslash: `#include <wil\cppwinrt.h>`. Grepping for
  `wil/cppwinrt.h` returns nothing and looks like the include is absent.

### Recognizing `0xC0000409`

Faulting module `ucrtbase.dll` means `abort()`/`terminate` — an exception escaped a thread body or
a `noexcept` function. Faulting module = your own binary or WIL means a WIL fail-fast — usually
the missing `<wil/cppwinrt.h>`. Split those two first, before theorizing.

## 2. Lifetime and use after free

- **Coroutine parameters must be taken by value.** A reference parameter is not stored in the
  coroutine frame, so after the first `co_await` it dangles. This is worst in XAML event handlers,
  where the framework releases the args the moment the handler returns at its first suspend point.
  This shipped as an access violation in a released preview.
- **Event handlers must capture `weak_from_this()`** and re-check a shutdown flag on entry.
  Revoking a token does **not** drain handlers already in flight.
- **A watcher or browser you do not own needs its stop to drain.** Cancel, then spin on an
  in-flight counter, and only then clear the handlers.
- **`[this]` captures are mostly fine** — synchronous callbacks and `jthread` members that join in
  their destructor are safe. Classify them rather than converting them all; the ones that matter
  are event tokens and callbacks into objects owned elsewhere.
- **After moving a member between a base and a derived class, sweep for a redeclared member.** C++
  shadows silently: the base initializes its copy, the derived code reads its own empty one, and
  the forwarding chain looks correct at every step.
- Releasing a callback and tearing down an endpoint often **re-enters synchronously** through the
  bidirectional stream's `Shutdown`. Release the callback before deleting the endpoint.

## 3. Concurrency, locks and TOCTOU

- **SRW locks are not recursive in any combination.** Taking shared after exclusive on the same
  thread deadlocks exactly like exclusive after exclusive. "Shared is safe because it does not
  mutate" is wrong.
- **Never call out while holding a lock** — no callbacks, no COM, no network, no WinRT async, no
  blocking wait. Copy what you need under the lock (a `com_ptr` addref keeps the callee alive),
  release, then call. Almost every deadlock in this project's history is this one mistake.
- **Keep one global lock order** and do not invert it anywhere. Measure nesting by **brace depth**,
  not proximity: two adjacent scoped blocks are sequential, not nested, and a proximity scan
  produces mostly false positives.
- **Nothing on a socket receive callback, a device-watcher callback or a PnP notification callback
  may block on the service.** Queue the work to a background worker and return in microseconds.
  Creating or deleting an endpoint raises PnP notifications that re-enter the same locks.
- **TOCTOU:** a "still pending?" check and the action it guards must be one atomic transition under
  one lock acquisition. Building a connection takes seconds; if the user can remove the definition
  in that window, the object gets registered anyway and becomes invisible and unreachable while
  holding a socket and an endpoint. The fix is an `AddIfStillPending`-style operation, not a
  re-check.
- **Insert and erase must use the same normalized key.** An insert that normalizes and an erase
  that does not is a silent no-op leaving a stale entry that blocks re-creation forever.
- **A new thread must call `winrt::init_apartment()`**, or its COM and WinRT calls fail with
  `CO_E_NOTINITIALIZED` and the work silently stops happening.
- **Declare `jthread` members last** in the class (reverse-order destruction) and join them
  explicitly in `Shutdown`, with a `get_id() != std::this_thread::get_id()` self-join guard.
- **Clear an "in progress" flag on every exit path.** A refresh loop that hands the flag to a
  dispatcher lambda leaks it whenever the dispatcher is null or the enqueue fails, and the page
  silently stops refreshing forever.

### Never `.get()` a WinRT async operation from a callback or a UI thread

Blocking a thread you do not own is not just slow. The failures are qualitatively different from
"the call took a while", and all three have happened here.

- **In a device watcher or DevQuery callback**, the blocking call holds a system callback thread.
  A watcher that exceeds its notification budget is put into `DeviceWatcherStatus::Aborted` by the
  system, and an aborted watcher goes **permanently and silently blind** — it never reports another
  arrival or removal, and it never reports `Stopped` either, so a stop poll waiting for `Stopped`
  hangs. The visible symptom is a device that is never re-enumerated until the service restarts,
  which looks exactly like a driver defect and is not. Recovering needs an explicit
  restart-if-aborted path with backoff; the real fix is to push the work onto a worker thread and
  return from the callback in microseconds.
- **On an STA UI thread**, it trips `winrt::impl::check_sta_blocking_wait` (a debug assert dialog),
  and nested calls such as creating an endpoint connection can hang forever, because the apartment
  cannot service the nested call while it is blocked.
- **Wrapping it in a helper that does `co_await winrt::resume_background()` first does not fix
  it.** C++/WinRT's `wait_get` calls `check_sta_blocking_wait()` unconditionally, before it even
  looks at `Status()`, so converting a raw `.get()` into such a helper moves the assert one frame
  deeper and changes nothing. Wait via `Completed()` plus an event, then `GetResults()` (which
  rethrows on failure exactly like `get()`), and skip the wait entirely when the operation has
  already completed. Do not use `CoWaitForMultipleHandles` — it pumps, which reintroduces the
  reentrancy you were avoiding.

In UI code, prefer not needing the call at all: a watcher's enumerated-devices collection already
hands you the objects, so you rarely have to look one up by id.

## 4. Untrusted input

The configuration file is writable by a standard user, and the service runs as `LOCAL SERVICE`.
Device-supplied names, network packets and configuration values are all hostile input.

- **WinRT JSON "default" accessors only cover a missing key, not a wrong type.**
  `GetNamedString(name, L"")` and `GetNamedObject(name, nullptr)` **throw** when the name is
  present and holds another type. Check `HasKey` plus `ValueType()` first.
- **`entry.try_as<json::JsonObject>()` always returns `nullptr`.** Iterating a `JsonArray` yields
  `IJsonValue` and iterating a `JsonObject` yields `IKeyValuePair`; neither queries to
  `JsonObject`. There is no compile error and no exception, so every element is silently skipped
  and the configuration reads as empty. Use:
  ```cpp
  if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object) { continue; }
  auto const obj = entry.GetObject();
  ```
  (`IJsonValue::GetObject()` collides with the `GetObject` macro from `wingdi.h`; check
  `ValueType()` and then call the one-argument `GetNamedObject(name)` on the parent instead.)
- **Never parse an untrusted string straight into a GUID.** `winrt::guid{ hstring }` throws, and
  `internal::StringToGuid` returns an **uninitialized** GUID on failure. Use
  `internal::TryParseGuidString`. One malformed entry inside a loop wrapped in a single
  `catch (...)` hides every entry after it.
- **Any new configuration field that becomes a path, a file name or a credential name** is what
  turns configuration tampering into privilege escalation. Those are the fields to scrutinize.
  A user-writable directory can be redirected with a junction — junctions have never needed
  admin — so verify the final path, do not trust it.
- **Validate the declared length of network payloads against the datagram up front**, and never let
  a handler read past its own payload. Resynchronize by skipping unconsumed payload after each
  command.
- **Anything that can produce a storm of replies must be rate limited**, with a fixed-size table
  rather than a map keyed by an attacker-chosen value.
- **Reject out-of-spec input rather than absorbing it.** Tolerating malformed device identity is
  what poisoned USB MIDI 1.0 naming for two decades.

## 5. Strings, buffers and identifiers

- **UMP name and identifier limits are UTF-8 byte counts, not UTF-16 code units.** Never measure
  them with `wstring::length()` or `hstring::size()`. Use the helpers in
  `src/in-box/Inc/wstring_util.h`: `Utf8ByteCount`, `ExceedsUtf8ByteCount`,
  `TruncateToUtf8ByteCount`.
- **Truncating UTF-8 correctly:** inspect the **first dropped byte** and walk back only while
  *that* is a continuation byte. Resizing to the cap and then popping trailing continuation bytes
  destroys a character that ends exactly on the cap.
- **Do not round a byte cap down to a word boundary** unless the wire format requires it. Two
  spec-sized identity fields were silently clipped by two bytes that way, so a remote device saw
  the advertised identity and the in-session identity as two different devices.
- **A boundary test must assert that the maximum value survives intact**, not merely that the
  result does not exceed the maximum. A truncating bug passes `length <= max`.
- **Software device instance ids** allow only ASCII alphanumerics plus `-` and `_`, max 200
  characters, normalized to uppercase. Use `internal::RemoveInvalidSWDUniqueIdCharacters`.
- **Verify where a fix belongs before writing it.** A plausibly named helper is not evidence that
  it is on the code path — grep for its callers first. A fix placed in a dead function passes
  review and changes nothing.

## 6. Silent failure and observability

- **Trace in the type-check branch, not in the `catch`.** A key present with the wrong type falling
  back to a default with nothing logged is how a mistyped `"enabled": "yes"` produced a silently
  disabled host. Absence of a key stays silent, because absence is normal.
- **A `catch` that returns a default is fine only if the interesting case was traced before it.**
- **An empty trace means the code never ran.** Look at loading, registration and ACLs before
  looking at what the code does once loaded.
- **Absence of an event may mean the provider was never in the profile.** Check the `.wprp` before
  concluding anything from missing events. There is more than one `.wprp` in this repository.
- **Message payload data may only be traced under `_DEBUG`** — it is a privacy violation in retail.
  Pointers, byte counts, device ids and names are fine.
- **Keep tracing out of the per-message hot path.**
- **A partial list is a failure mode.** An enumeration loop whose per-item work can throw needs the
  `try` *inside* the loop, or one bad item silently truncates everything after it.

## 7. COM, WinRT and IDL

- **`S_FALSE` is not failure.** `RETURN_IF_FAILED` does not catch it. Some interfaces return
  `S_FALSE` without setting their out parameter; check `hr == S_FALSE || out == nullptr`
  explicitly.
- **MIDL silently discards a trailing `const` on a method.** If the implementation declares the
  method `const`, it is a different signature, does not override the pure virtual, and the class
  stays abstract. The real error, `C2259`, is buried behind a `C4373` warning promoted by `/WX` —
  read the full build log, not the first error MSBuild prints.
- **Parameter `const` is preserved, and pointee `const` is the meaningful form.** `UINT32 const*
  buffer` promises the callee will not modify the buffer. `UINT32* const` only makes the pointer
  const, is not part of the function type, and tells a caller nothing.
- **The generated header for a hand-written IDL does not always regenerate**, and never for
  platforms you did not build. Compare timestamps against the `.idl` before believing a mismatch,
  and rebuild the IDL project once per platform and configuration.
- **An IDL change must be mirrored everywhere it is duplicated**: the `.idl`, any committed MIDL
  output that is checked into git, and the published reference documentation page — including the
  numeric values in enum tables. Wrong numbers in shipped docs are worse than missing ones.
- **A runtime class with a constructor or statics must be registered** in the OS manifest, or it
  fails at run time with "Class not registered". Enums are not activatable and need no entry.
- **A transport needs three registrations**, not one: the file, the COM in-process server CLSID,
  and the `Transport Plugins\<Name>` registry key. The service discovers transports by enumerating
  that key, so a transport with only the first two ships and never loads.

## How to audit a diff mechanically

- **Verify your instrument before trusting a negative result.** Run any detector script against a
  file known to contain the defect first. A scan that reports zero findings because it is broken
  looks exactly like a clean codebase. This has produced wrong conclusions here more than once.
- **`git diff -w` is the right check after a pass that reindents bodies** (wrapping functions in
  `try`, for example). Plain `--stat` shows thousands of changed lines and proves nothing;
  whitespace-insensitive diff collapses it, and "zero removed lines" is direct proof that no code
  was lost or truncated.
- **Never build an edit from transformed terminal output.** Take edit text from the file, not from
  something you printed through a formatter — phantom indentation and substituted characters have
  corrupted whitespace-sensitive files here.
- **One edit per pass, re-reading the file each time.** Collecting all offsets up front and
  applying them in a batch produces overlapping ranges and truncated identifiers, and it still
  compiles.
- **Read the result of any scripted edit** rather than trusting the script's own success output,
  then build.
