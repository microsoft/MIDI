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
// Rolling this back restores calling Shutdown on the device and transform pipes while
// m_ClientManagerLock is held exclusive.
class Feature_Servicing_MIDI2VirtualDeviceRemovalDeadlock
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
