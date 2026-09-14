// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisettings
{
    // The notifications app's settings, written here and read there. The contract is in
    // user-tools\midi-notifications\notification_settings_defs.h.
    //
    // All per-user. Whether to be interrupted is a personal choice, even though the MIDI
    // configuration being reported on is machine wide.
    class NotificationSettings
    {
    public:
        static bool NotificationsEnabled() noexcept;
        static void NotificationsEnabled(_In_ bool const value) noexcept;

        static bool NetworkApprovalEnabled() noexcept;
        static void NetworkApprovalEnabled(_In_ bool const value) noexcept;

        // This user's own Run entry, which needs no administrator.
        static bool StartsAtSignIn() noexcept;
        static bool TrySetStartsAtSignIn(_In_ bool const value) noexcept;

        // True when an administrator set the app to start for everyone on this PC. That entry is
        // in HKLM and cannot be removed from here, so the UI says so rather than offering a
        // switch which would not work.
        static bool StartsForAllUsers() noexcept;

        // Turning notifications off makes the running app exit, so turning them back on has to
        // start it again. Without this the switch appears to do nothing until the next sign in.
        static void EnsureAppRunning() noexcept;

        // Empty when the app is not installed beside this one.
        static std::wstring AppPath() noexcept;

    private:
        static bool ReadFlag(_In_ PCWSTR const valueName, _In_ bool const defaultValue) noexcept;
        static void WriteFlag(_In_ PCWSTR const valueName, _In_ bool const value) noexcept;
    };
}
