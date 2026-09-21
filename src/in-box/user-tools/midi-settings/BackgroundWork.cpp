// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "BackgroundWork.h"

namespace midisettings
{
    namespace
    {
        // There is no resume_foreground overload for the Microsoft.UI dispatcher in this
        // projection, so the continuation is marshaled back with TryEnqueue. A queue which
        // refuses the work resumes in place, which is what a closing window looks like.
        struct ResumeOnDispatcher
        {
            winrt::Microsoft::UI::Dispatching::DispatcherQueue Queue{ nullptr };

            bool await_ready() const noexcept
            {
                return Queue == nullptr;
            }

            bool await_suspend(std::coroutine_handle<> handle) const noexcept
            {
                return Queue.TryEnqueue([handle]() { handle(); });
            }

            void await_resume() const noexcept
            {
            }
        };
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept
    {
        // Awaiting this action is not enough on its own to get the caller back onto the UI
        // thread. When the apartment resume is lost, every XAML call in the caller's
        // continuation throws RPC_E_WRONG_THREAD into its catch handler, so the progress ring
        // it was about to switch off spins forever.
        ResumeOnDispatcher const resumeOnCaller{
            winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread() };

        co_await winrt::resume_background();

        try
        {
            if (work)
            {
                work();
            }
        }
        MIDI_SETTINGS_CATCH_AND_LOG(L"Background work failed.")

        co_await resumeOnCaller;
    }
}
