// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

#include "KeyboardInstance.h"

#include "AppSettings.h"
#include "SingleInstance.h"

namespace midikeyboard
{
    namespace
    {
        constexpr wchar_t InstanceKey[] = L"Keyboard";

        // Past this many windows they all share the last slot. Nobody plays thirty-two keyboards.
        constexpr uint32_t MaximumWindowSlots = 32;

        wil::unique_handle g_windowSlotEvent{};
        uint32_t g_windowSlot{ 0 };

        // No destination, group, channel or program source named. Help and a bad command line
        // also count as named, so they never get swallowed by a window already open.
        bool IsPlainLaunch(CommandLineOptions const& options) noexcept
        {
            return !options.ShowHelp &&
                !options.HasError &&
                options.EndpointDeviceId.empty() &&
                options.ProgramsEndpointDeviceId.empty() &&
                !options.GroupNumber.has_value() &&
                !options.ChannelNumber.has_value() &&
                !options.ProgramsGroupNumber.has_value();
        }
    }

    _Use_decl_annotations_
    bool KeyboardInstance::AcquireOrActivateExisting(CommandLineOptions const& options) noexcept
    {
        try
        {
            if (IsPlainLaunch(options) && AppSettings::Current().SingleWindow())
            {
                if (!::midiapp::SingleInstance::AcquireOrActivateExisting(InstanceKey))
                {
                    return false;
                }
            }
        }
        catch (...)
        {
            // Opening a second window is better than not opening at all.
        }

        ClaimWindowSlot();

        return true;
    }

    void KeyboardInstance::ClaimWindowSlot() noexcept
    {
        g_windowSlot = 0;

        try
        {
            for (uint32_t slot = 0; slot < MaximumWindowSlots; slot++)
            {
                auto const name = std::wstring{ LR"(Local\Microsoft.WindowsMidiServices.MidiKeyboard.WindowSlot.)" } +
                    std::to_wstring(slot);

                // An event, not a mutex: only whether the name exists matters, and an event has no
                // owning thread to get wrong. Held until the process exits, which frees the slot.
                wil::unique_handle slotEvent{ ::CreateEventW(nullptr, TRUE, FALSE, name.c_str()) };

                if (!slotEvent)
                {
                    // Can't tell which windows are open, so put this one where it was saved.
                    g_windowSlot = 0;
                    return;
                }

                if (::GetLastError() != ERROR_ALREADY_EXISTS)
                {
                    g_windowSlotEvent = std::move(slotEvent);
                    g_windowSlot = slot;
                    return;
                }
            }

            g_windowSlot = MaximumWindowSlots - 1;
        }
        catch (...)
        {
            g_windowSlot = 0;
        }
    }

    uint32_t KeyboardInstance::WindowSlot() noexcept
    {
        return g_windowSlot;
    }

    _Use_decl_annotations_
    void KeyboardInstance::PublishMainWindow(HWND const window) noexcept
    {
        ::midiapp::SingleInstance::PublishMainWindow(window);
    }

    void KeyboardInstance::Release() noexcept
    {
        ::midiapp::SingleInstance::Release();
    }
}
