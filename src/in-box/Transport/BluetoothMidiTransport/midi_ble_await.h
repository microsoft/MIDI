// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_AWAIT_H
#define MIDI_BLE_AWAIT_H

// Waits on a WinRT async operation for a limited time, and gives up early when what it is waiting
// for no longer matters. Needs only WinRT Foundation and wil, so it is unit tested on its own.

// Must precede the other wil headers, or WIL cannot identify winrt::hresult_error
#include <wil/cppwinrt.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <winrt/Windows.Foundation.h>

#include "transport_defs.h"

namespace MidiBleUtilities
{
    inline constexpr uint32_t AwaitPollSliceMilliseconds = MIDI_BLE_AWAIT_POLL_SLICE_MS;

    // Set while the transport is tearing down. Service shutdown joins the threads which make these
    // calls, so a wait in progress abandons rather than running to its full timeout.
    inline std::atomic<bool> g_shuttingDown{ false };

    inline bool IsShuttingDown() noexcept { return g_shuttingDown.load(std::memory_order_relaxed); }
    inline void SetShuttingDown(_In_ bool const value) noexcept { g_shuttingDown.store(value, std::memory_order_relaxed); }

    struct AwaitOutcome
    {
        // A wait which ran out of time and a call the system failed outright both come back empty,
        // but only one of them means the device stopped answering.
        bool TimedOut{ false };
        bool Failed{ false };
        HRESULT ErrorCode{ S_OK };

        // Given up because the transport or the caller's connection is shutting down. TimedOut is
        // set as well, which is how a wait abandoned at service shutdown has always been reported.
        bool Abandoned{ false };
    };

    // shouldAbandon is checked before the wait and after every poll slice. A connection which is
    // shutting down passes one, so its own waits end within a slice instead of holding up the
    // thread join, and with it the removal of the endpoint, for the whole timeout.
    template<typename TResult, typename TShouldAbandon>
    inline TResult AwaitWithTimeoutOrAbandon(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _In_ TShouldAbandon const& shouldAbandon,
        _Out_ AwaitOutcome& outcome)
    {
        outcome = AwaitOutcome{ };

        if (operation == nullptr)
        {
            outcome.Failed = true;
            outcome.ErrorCode = E_POINTER;

            return onTimeout;
        }

        try
        {
            // Completed may only ever be assigned once on a WinRT async operation, which rules out
            // calling wait_for in a loop: the second call throws and the wait collapses to a single
            // slice. Signaling an event from one handler and waiting on that instead is what lets
            // the wait be given up early without touching the operation again.
            //
            // Shared rather than captured by reference, because the handler can still run after
            // this function has abandoned the operation and returned.
            auto completed = std::make_shared<wil::slim_event_manual_reset>();

            operation.Completed([completed](auto&&, auto&&) { completed->SetEvent(); });

            uint32_t remaining = timeoutMilliseconds;

            // Only the caller's condition is checked up front. The service-wide flag keeps its
            // old behavior of allowing one slice, which the courtesy writes at teardown rely on.
            bool abandoned = shouldAbandon();

            while (remaining > 0 && !abandoned)
            {
                uint32_t const slice = remaining < AwaitPollSliceMilliseconds ? remaining : AwaitPollSliceMilliseconds;

                if (completed->wait(slice))
                {
                    break;
                }

                remaining -= slice;

                abandoned = IsShuttingDown() || shouldAbandon();
            }

            auto const status = operation.Status();

            if (status == ::winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                return operation.GetResults();
            }

            if (status == ::winrt::Windows::Foundation::AsyncStatus::Started)
            {
                outcome.TimedOut = true;
                outcome.Abandoned = abandoned;
            }
            else
            {
                // Error and Canceled carry the HRESULT which names the real fault. Reporting these
                // as a timeout discards the only field a "it will not connect" report can act on.
                outcome.Failed = true;
                outcome.ErrorCode = operation.ErrorCode();
            }

            // Best effort. The operation keeps its own references and unwinds on its own, so
            // abandoning it is safe even when the cancel is ignored.
            operation.Cancel();
        }
        CATCH_LOG();

        if (!outcome.TimedOut && !outcome.Failed)
        {
            outcome.Failed = true;
        }

        return onTimeout;
    }

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _Out_ AwaitOutcome& outcome)
    {
        return AwaitWithTimeoutOrAbandon(operation, timeoutMilliseconds, onTimeout, []() noexcept { return false; }, outcome);
    }

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _Out_ bool& timedOut)
    {
        AwaitOutcome outcome{ };

        auto result = AwaitWithTimeout(operation, timeoutMilliseconds, onTimeout, outcome);

        // Existing callers use this flag to mean "did not succeed"
        timedOut = outcome.TimedOut || outcome.Failed;

        return result;
    }

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout)
    {
        AwaitOutcome ignored{ };

        return AwaitWithTimeout(operation, timeoutMilliseconds, onTimeout, ignored);
    }

    template<typename TResult, typename TShouldAbandon>
    inline TResult AwaitWithTimeoutOrAbandon(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout,
        _In_ TShouldAbandon const& shouldAbandon)
    {
        AwaitOutcome ignored{ };

        return AwaitWithTimeoutOrAbandon(operation, timeoutMilliseconds, onTimeout, shouldAbandon, ignored);
    }
}

#endif
