// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The contract with the user-session apps is described in the network transport's
// network_notification_defs.h: the service changes a counter in a volatile key, and an app in each
// session watches the key and then asks the service what changed. These two must stay identical to
// the ones there.
#ifndef MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY
#define MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY \
    L"Software\\Microsoft\\Windows MIDI Services"
#endif

#ifndef MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME
#define MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME \
    L"Notifications"
#endif

// Bumped when a loopback of either kind mutes itself because MIDI was feeding back into it.
#define MIDI_NOTIFICATION_LOOPBACK_FEEDBACK_VALUE \
    L"LoopbackFeedbackChangeCount"

namespace WindowsMidiServicesInternal
{
    // Touches the registry, so never call it on a path that sends MIDI.
    inline HRESULT BumpNotificationSignalCounter(_In_ PCWSTR const valueName) noexcept
    {
        wil::unique_hkey rootKey{};

        // Opened, never created: creating a missing parent here would create it volatile.
        auto result = ::RegOpenKeyExW(
            HKEY_LOCAL_MACHINE,
            MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY,
            0,
            KEY_CREATE_SUB_KEY,
            rootKey.put());

        if (result != ERROR_SUCCESS)
        {
            return HRESULT_FROM_WIN32(result);
        }

        wil::unique_hkey key{};

        result = ::RegCreateKeyExW(
            rootKey.get(),
            MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME,
            0,
            nullptr,
            REG_OPTION_VOLATILE,
            KEY_QUERY_VALUE | KEY_SET_VALUE,
            nullptr,
            key.put(),
            nullptr);

        if (result != ERROR_SUCCESS)
        {
            return HRESULT_FROM_WIN32(result);
        }

        DWORD currentValue{ 0 };
        DWORD valueSize{ sizeof(currentValue) };
        DWORD valueType{ 0 };

        if (::RegQueryValueExW(
                key.get(),
                valueName,
                nullptr,
                &valueType,
                reinterpret_cast<LPBYTE>(&currentValue),
                &valueSize) != ERROR_SUCCESS ||
            valueType != REG_DWORD)
        {
            currentValue = 0;
        }

        // Readers only look for a change, so wrapping is harmless.
        DWORD const newValue{ currentValue + 1 };

        return HRESULT_FROM_WIN32(::RegSetValueExW(
            key.get(),
            valueName,
            0,
            REG_DWORD,
            reinterpret_cast<BYTE const*>(&newValue),
            sizeof(newValue)));
    }
}
