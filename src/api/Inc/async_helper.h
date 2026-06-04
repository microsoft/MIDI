// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#pragma once

namespace WindowsMidiServicesInternal
{
    // Executes an IAsyncOperation<T>-returning lambda on the thread pool and
    // synchronously waits for its result. Because the inner co_await resumes
    // on the background thread (never on the caller's apartment), this is
    // safe to call from an STA without deadlocking.
    template <typename TFunc>
    auto RunOnBackgroundThreadAndWait(TFunc&& makeAsync)
    {
        using AsyncT = std::invoke_result_t<TFunc>;

        auto op = [](TFunc fn) -> AsyncT
            {
                co_await winrt::resume_background();
                co_return co_await fn();
            }(std::forward<TFunc>(makeAsync));

        return op.get();
    }
}

