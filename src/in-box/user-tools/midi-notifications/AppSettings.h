// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The customer's notification choices, read fresh rather than cached at startup so a change made
// in MIDI Settings takes effect without restarting anything.
class AppSettings
{
public:
    static bool NotificationsEnabled() noexcept;
    static bool NetworkApprovalNotificationsEnabled() noexcept;

private:
    static bool ReadFlag(_In_ PCWSTR const valueName, _In_ bool const defaultValue) noexcept;
};
