// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Watches one registry key for value changes and calls back. RegNotifyChangeKeyValue is used
// rather than anything shared because every logged on session runs its own copy of this app,
// and it is the change primitive which lets any number of watchers register independently
// without one of them consuming another's wakeup.
class RegistryChangeWatcher
{
public:
    ~RegistryChangeWatcher() { Stop(); }

    // Watches a volatile key below a parent which must already exist. The parent is never
    // created, because creating it would create it volatile too and quietly turn an installed
    // key into one that disappears at reboot. A missing parent means notifications do not work.
    HRESULT StartVolatile(
        _In_ HKEY const rootKey,
        _In_ std::wstring const& parentPath,
        _In_ std::wstring const& subKeyName,
        _In_ std::function<void()> onChanged) noexcept;

    // Watches stored settings, creating the path if this is the first time the customer has had
    // any. Nothing here is volatile, so creating parents is what is wanted.
    HRESULT StartPersistent(
        _In_ HKEY const rootKey,
        _In_ std::wstring const& subKeyPath,
        _In_ std::function<void()> onChanged) noexcept;

    void Stop() noexcept;

private:
    HRESULT BeginWatching(_In_ std::function<void()> onChanged) noexcept;

    HRESULT ArmNotification() noexcept;

    static void CALLBACK OnWaitCallback(
        _Inout_ PTP_CALLBACK_INSTANCE instance,
        _Inout_opt_ PVOID context,
        _Inout_ PTP_WAIT wait,
        _In_ TP_WAIT_RESULT waitResult) noexcept;

    wil::unique_hkey m_key;
    wil::unique_event m_changeEvent{ };
    PTP_WAIT m_wait{ nullptr };

    std::function<void()> m_onChanged;

    // Set before the threadpool wait is closed, so a callback which is already running knows not
    // to re-arm into a key that is about to be released.
    std::atomic<bool> m_stopping{ false };
};
