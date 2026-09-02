// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "BackgroundWork.h"

namespace midikeyboard
{
    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncAction RunOnBackgroundAsync(std::function<void()> work) noexcept
    {
        co_await winrt::resume_background();

        try
        {
            if (work)
            {
                work();
            }
        }
        MIDI_KEYBOARD_CATCH_AND_LOG(L"Background work failed.")
    }
}
