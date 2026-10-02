// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// How the transport tells a user-session app that a remote is waiting for a decision. Same key
// and same rules as Network MIDI 2.0, with a value of its own.
//
// The service runs in session 0 and cannot raise a toast, so it bumps a counter in a volatile
// key that apps watch with RegNotifyChangeKeyValue. The value only says that something changed.
// Any authenticated user can write the key, so a reader treats a change as "ask the service
// again" and never trusts what it reads there.
//
// The root is opened, never created: creating it here would make the installed MIDI registry
// root volatile. A missing root means notifications do not work, and nothing else fails.
// ============================================================================

#pragma once

#define MIDI_RTP_NOTIFICATION_SIGNAL_ROOT_REG_KEY \
    L"Software\\Microsoft\\Windows MIDI Services"

#define MIDI_RTP_NOTIFICATION_SIGNAL_SUBKEY_NAME \
    L"Notifications"

// Bumped when a remote starts or stops waiting for a decision on one of this PC's RTP-MIDI hosts
#define MIDI_RTP_NOTIFICATION_PENDING_APPROVAL_VALUE \
    L"RtpMidiPendingApprovalChangeCount"

// Bumped when one of this PC's RTP-MIDI hosts starts or stops waiting for its network adapter,
// because the adapter is missing and the host may not fall back to the others
#define MIDI_RTP_NOTIFICATION_HOST_ADAPTER_VALUE \
    L"RtpMidiHostAdapterChangeCount"
