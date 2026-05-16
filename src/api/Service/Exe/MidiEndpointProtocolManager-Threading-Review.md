# MidiEndpointProtocolManager / MidiEndpointProtocolWorker Threading Review

This document captures a review of the `std::thread` / `detach()` usage in
`MidiEndpointProtocolManager.cpp` and `MidiEndpointProtocolWorker.cpp`, the
shutdown hazards it creates for `midisrv`, and the recommended changes.

## TL;DR

The current pattern is incorrect. `detach()` is called immediately after the
worker thread is constructed, which makes the later `join()` a no-op. As a
result, `RemoveWorkerIfPresent` and `Shutdown` never actually wait for the
worker thread. Combined with iterator-invalidating `erase` during `Shutdown`,
resetting the manager dependencies before the workers are gone, holding
`m_lock` for the entire worker lifetime, and not initializing COM on the
worker thread, this will cause intermittent shutdown crashes in `midisrv`.

Fix summary:

- Remove the `detach()`.
- Switch to `std::jthread` with a `std::stop_token`.
- Fix the iteration in `Shutdown()`.
- Only release the shared managers after all workers have been joined.

---

## The core bug: `detach()` defeats the later `join()`

`MidiEndpointProtocolManager.cpp`, lines 230-234:

Then in `RemoveWorkerIfPresent`, lines 272-282:

Once `detach()` is called, the `std::thread` is no longer joinable.
`joinable()` returns `false`, the `join()` is skipped, and the code happily
proceeds to call `Worker->Shutdown()` and erase the map entry **while the
worker thread is very possibly still inside `Start()`**, sitting on
`m_endProcessing.wait()` or in `m_midiBidiDevice` teardown. The worker object
itself stays alive (because the detached `std::thread` was constructed with a
copy of `shared_ptr<CMidiEndpointProtocolWorker>`, so the thread owns a
reference), but `Shutdown()` runs concurrently with the worker still running.
So you cannot reason about ordering of `Worker->Shutdown()` vs. what `Start()`
is doing.

Net effect: the `EndProcessing()` / `joinable()` / `join()` block in
`RemoveWorkerIfPresent` is effectively dead code — it does the event-set but
never waits.

## Shutdown ordering hazard for the service

`CMidiEndpointProtocolManager::Shutdown()` (lines 326-342) signals the event
and immediately resets `m_clientManager`, `m_deviceManager`, and
`m_sessionTracker`. The detached worker threads can still be:

- Inside `Start()` referencing `m_deviceManager` (e.g.
  `UpdateAllFunctionBlockPropertiesIfComplete` / `SetDiscoveryCompleteProperty`
  both call `m_deviceManager->UpdateEndpointProperties`).
- Holding `wil::com_ptr_nothrow<IMidiBidirectional> m_midiBidiDevice` whose
  backing transport is being torn down elsewhere.
- Emitting `TraceLogging` after the provider has been unregistered.

Because `midisrv` may also be calling `CoUninitialize` / WinRT apartment
teardown on the main thread right after `Shutdown()` returns, you can easily
get AVs or "stowed exception" crashes attributed to service exit. This is
exactly the class of bug that shows up only at service stop or session change
time.

## Iterator invalidation in `Shutdown()`

Lines 332-337:

`RemoveWorkerIfPresent` calls `m_endpointWorkers.erase(cleanEndpointId)`,
which invalidates the iterator the range-for is using and also invalidates the
`key` reference it just bound (the node holding that wstring is freed). This
is UB regardless of the thread issue.

Note also that `Shutdown()` itself does not hold `m_endpointWorkersMapMutex`
while iterating, but `RemoveWorkerIfPresent` does take it — that is fine for
re-entrancy (`scoped_lock` on a non-recursive `std::mutex`), but it does mean
another thread (the device watcher's `OnDeviceRemoved`) could mutate the map
between iterations.

## `m_lock` is held for the worker's entire lifetime

`MidiEndpointProtocolWorker.cpp` line 163:

`Start()` takes `m_lock` at the top and does not release it until the
function returns, which only happens after `m_endProcessing` is set. If
anything else ever calls a method on the worker that takes `m_lock`, it will
deadlock until shutdown. Today none of `Callback`, `EndProcessing`, or
`Shutdown` take `m_lock`, so it appears benign, but it is a footgun — and it
means `m_lock` is doing nothing useful for synchronizing with `Callback`,
which is probably what you want it for.

## COM apartment on the worker thread

`Start()` calls `CoCreateInstance` on the new `std::thread`, but that thread
never calls `CoInitializeEx` / `winrt::init_apartment`. `Initialize()` only
initializes the apartment on the *calling* thread (manager thread). On the
worker thread, `CoCreateInstance` will either fail with `CO_E_NOTINITIALIZED`
or work only by accident depending on whether some other code has implicitly
created the MTA. This is also why detaching is so painful — you have no clean
place to pair `CoUninitialize` with `CoInitializeEx`.

## Recommended changes

In rough order of importance:

1. **Stop calling `detach()`.** Keep ownership of the `std::thread` (or
   better, `std::jthread`) in `ProtocolNegotiationWorkerThreadEntry` and join
   it in `RemoveWorkerIfPresent` and in `Shutdown()`. The existing
   `EndProcessing()` + `joinable()` / `join()` logic is correct — it just
   needs the thread to actually be joinable.

2. **Prefer `std::jthread` with a `std::stop_token`** (the TODO on line 14 of
   the header already notes this). `jthread`'s destructor calls
   `request_stop()` and `join()` for you, which makes the map cleanup and
   `Shutdown()` exception-safe.

3. **Fix the iteration in `Shutdown()`.** Either snapshot keys first, or
   drain the map by repeatedly popping the first element:

4. **Reset `m_clientManager` / `m_deviceManager` / `m_sessionTracker` only
   after all workers have been joined.** Today these are reset right after the
   (no-op) join, which is the worst possible order if `detach` stays.

5. **Don't hold `m_lock` across `m_endProcessing.wait()`** in `Start()`. Take
   the lock only around state the callback shares with `Start()` (the function
   block maps, the booleans). Right now the lock buys nothing because
   `Callback` does not take it, andit locks you out of ever making
   `Shutdown()` synchronize with `Start()` later.

6. **Initialize COM on the worker thread.** Call
   `winrt::init_apartment(winrt::apartment_type::multi_threaded)` at the top
   of `Start()`. Pair with `uninit_apartment` on exit, or call `CoUninitialize`
   yourself before returning.

7. *Minor:* `ProtocolNegotiationWorkerThreadEntry::Thread` does not need to
   be `shared_ptr<std::thread>`. `std::thread` (or `std::jthread`) is movable;
   store it by value. That removes a class of mistakes around the lifetime of
   the handle vs. the OS thread.

## Summary of risks if left as-is

| Risk | Where | Symptom |
|------|-------|---------|
| Detached thread outlives `Shutdown()` | `DiscoverAndNegotiate` line 234 | Use-after-free on managers, AV on service stop |
| `join()` never runs | `RemoveWorkerIfPresent` line 274 | Cleanup races, intermittent crashes |
| Iterator invalidation | `Shutdown()` line 332 | UB, heap corruption |
| Managers reset before workers exit | `Shutdown()` lines 340-342 | Null deref in worker, stowed exceptions |
| `m_lock` held across `wait()` | `Start()` line 163 | Future deadlocks when callers add locking |
| Uninitialized COM apartment on worker | `Start()` line 212 / 346 | `CO_E_NOTINITIALIZED` or undefined behavior |
