// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Shared by the notifications app, which reads these, and the MIDI Settings app, which writes
// them. Per-user, because whether to be interrupted is a per-user choice even though the MIDI
// configuration it reports on is machine-wide.

#define MIDI_NOTIFICATIONS_SETTINGS_REG_KEY \
    L"Software\\Microsoft\\Windows MIDI Services\\Tools\\notifications"

// Master switch. The app exits when this is off, so turning it off also stops it running at
// logon for this user even though the Run entry is machine wide.
#define MIDI_NOTIFICATIONS_VALUE_ENABLED                L"Enabled"

// Per-category switches, so a customer can keep the ones they want.
#define MIDI_NOTIFICATIONS_VALUE_NETWORK_APPROVAL       L"NetworkApprovalEnabled"

// The identity the app publishes toasts under. It has to match the AppUserModelID on the Start
// Menu shortcut the installer writes, or the notification platform will not accept a toast.
#define MIDI_NOTIFICATIONS_AUMID \
    L"Microsoft.WindowsMidiServices.Notifications"

// Where the Run entry lives. The installer writes it machine wide so a new user gets
// notifications without setting anything up; MIDI Settings adds and removes it.
#define MIDI_NOTIFICATIONS_RUN_REG_KEY \
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"

#define MIDI_NOTIFICATIONS_RUN_VALUE_NAME               L"WindowsMidiServicesNotifications"
