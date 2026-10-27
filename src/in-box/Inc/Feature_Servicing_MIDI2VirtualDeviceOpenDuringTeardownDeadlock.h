// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Two-thread deadlock that hangs the service when a virtual device is opened while another
// virtual device is shutting down.
//
// With Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock enabled, closing the device side of a
// virtual device runs MidiEndpointTable::OnDeviceDisconnectedAlwaysTeardown. It holds the virtual
// transport's m_entriesLock while it deletes the device's endpoints, and deleting an endpoint
// waits on the service's client manager lock. A client that is opening any virtual endpoint holds
// the client manager lock while the transport's connect path waits on m_entriesLock. Each thread
// waits on the lock the other holds, and every later client open and close in the service queues
// behind them.
//
// This fix adds MidiEndpointTable::OnDeviceDisconnectedTeardownOutsideEntriesLock. It holds
// m_entriesLock only to unlink the entry and take it out of the table, and deletes the endpoints
// without it. A new m_teardownLock is held for the whole teardown, and IsUniqueIdInUse takes it
// first, so a check for a unique id still waits until the old device's endpoints are gone.
//
// Rolling this back restores OnDeviceDisconnectedAlwaysTeardown, which deletes the endpoints with
// m_entriesLock held, and IsUniqueIdInUse taking only m_entriesLock.
class Feature_Servicing_MIDI2VirtualDeviceOpenDuringTeardownDeadlock
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2VirtualDeviceOpenDuringTeardownDeadlock_IsEnabled()
{
    return true;
}
