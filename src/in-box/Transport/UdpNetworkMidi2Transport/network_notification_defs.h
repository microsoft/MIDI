// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The contract between the service-side transport and any user-session app which wants to tell
// the customer that something needs their attention. Shared by the transport and the MIDI
// notifications app.
//
// The service cannot raise a toast: it runs as LocalService in session 0, which has no path to
// a user's notification platform. So it changes a value here instead, and an app in each logged
// on session watches the key with RegNotifyChangeKeyValue, which is the one change primitive
// that supports any number of independent watchers without losing a wakeup.
//
// The value is deliberately a bare counter. It says that something changed, never what, for two
// reasons: the set can change again before the customer reacts, and this key is writable by any
// authenticated user, so nothing read from it can be trusted. A reader treats a change as
// nothing more than "ask the service again", and the service's answer is the only truth.
//
// The key is volatile. This is a signal about what is happening right now, not a setting, so it
// should not survive a restart or cost a disk write.
//
// The root is opened, never created, and only the leaf below it is created. Creating a missing
// parent would create it volatile too, which would quietly turn the installed MIDI registry root
// into something that disappears at reboot. A missing root means someone has been editing the
// registry, and the right answer to that is that notifications do not work, not that anything
// fails.

#define MIDI_NOTIFICATION_SIGNAL_ROOT_REG_KEY \
    L"Software\\Microsoft\\Windows MIDI Services"

#define MIDI_NOTIFICATION_SIGNAL_SUBKEY_NAME \
    L"Notifications"

// Bumped when a remote client starts waiting for a decision on one of this PC's network hosts.
#define MIDI_NOTIFICATION_NETWORK_PENDING_APPROVAL_VALUE \
    L"NetworkPendingApprovalChangeCount"
