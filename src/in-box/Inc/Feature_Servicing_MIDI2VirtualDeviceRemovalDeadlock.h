// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Single-thread self-deadlock that wedges the entire service when a virtual device is
// torn down.
//
// CMidiClientManager::DestroyMidiClient holds m_ClientManagerLock exclusive while it calls
// Shutdown on the device pipes that are no longer in use. For a virtual device, that Shutdown
// is not a leaf: the device-side connection owns the lifetime of the client-side endpoint, so
// CMidi2VirtualMidiBidi::Shutdown calls MidiEndpointTable::OnDeviceDisconnected, which deletes
// the client-visible endpoint through CMidiDeviceManager::RemoveEndpoint and DeactivateEndpoint.
// DeactivateEndpoint then notifies CMidiClientManager::OnDeviceRemoved, which takes
// m_ClientManagerLock shared. SRW locks are not recursive in any combination, so acquiring
// shared on a thread that already holds the lock exclusive blocks forever.
//
// The shared acquire in OnDeviceRemoved was previously commented out precisely because of this
// deadlock. It was reinstated as a shared rather than exclusive acquire on the assumption that
// shared was safe for re-entrancy. It is not.
//
// This fix keeps the lock, and instead removes the re-entrancy: the pipes are detached and
// erased from the maps under the exclusive lock, and Shutdown is called on them after the lock
// has been released. That also stops a single wedged device from blocking every other
// CreateMidiClient and DestroyMidiClient for the duration of its teardown.
//
// The same function also defers its CMidiSessionTracker::RemoveClientEndpointConnection call
// until after the exclusive lock is released. That call takes the session tracker's own lock,
// and the session rundown path takes the two in the opposite order: RemoveClientSessionInternal
// holds the session tracker lock while it calls DestroyMidiClient, which reaches OnDeviceRemoved
// and asks for m_ClientManagerLock. Holding m_ClientManagerLock while waiting on the session
// tracker therefore deadlocks two threads against each other whenever a client teardown overlaps
// a session rundown. That inversion is older than this KIR and exists in the rolled back path
// too; it only became reachable once the single thread self-deadlock above stopped happening
// first. Both deferrals are gated together because the fixed code path only runs when this
// feature is enabled, so a separate gate could never be useful on its own.
//
// Rolling this back restores calling Shutdown on the device and transform pipes, and removing
// the client's endpoint connection from the session tracker, while m_ClientManagerLock is held
// exclusive.
class Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
