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
    // Executes an IAsyncOperation<T>-returning lambda on the thread pool and synchronously
    // waits for its result. Two things make this safe to call from an STA:
    //
    // 1. The inner co_await resumes on the thread pool and never on the caller's apartment,
    //    so completion cannot be blocked by the fact that we are not pumping messages.
    // 2. We wait on an event rather than calling IAsyncOperation::get(). cppwinrt's get()
    //    runs check_sta_blocking_wait() unconditionally, which fires an assert dialog in
    //    debug builds on any STA caller. Our synchronous public API is documented as callable
    //    from any thread, so that assert would fire in every app that calls it from its UI
    //    thread even though (1) makes a deadlock impossible.
    //
    // This still blocks the calling thread. Callers on a UI thread should prefer to do this
    // work on a background thread so the UI stays responsive.
    template <typename TFunc>
    auto RunOnBackgroundThreadAndWait(TFunc&& makeAsync)
    {
        using AsyncT = std::invoke_result_t<TFunc>;

        auto op = [](TFunc fn) -> AsyncT
            {
                co_await winrt::resume_background();
                co_return co_await fn();
            }(std::forward<TFunc>(makeAsync));

        if (op.Status() == winrt::Windows::Foundation::AsyncStatus::Started)
        {
            winrt::handle completed{ ::CreateEventW(nullptr, TRUE, FALSE, nullptr) };
            winrt::check_bool(static_cast<bool>(completed));

            op.Completed([signal = completed.get()](AsyncT const&, winrt::Windows::Foundation::AsyncStatus) noexcept
                {
                    ::SetEvent(signal);
                });

            ::WaitForSingleObject(completed.get(), INFINITE);
        }

        // matches get(): a failed operation rethrows here
        return op.GetResults();
    }
}

