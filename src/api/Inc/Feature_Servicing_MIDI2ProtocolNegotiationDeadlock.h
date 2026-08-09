// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The endpoint worker map mutex was held across worker teardown. Teardown waits on the worker's
// own lock, which the negotiation thread holds while calling back into the device manager, and
// that call can block behind a PnP DeviceWatcher callback. The result is a three way cycle in
// which every DiscoverAndNegotiate caller and the service control thread wait on the map mutex,
// so the service cannot be stopped.
//
// Rolling this back restores the previous behaviour of holding the map mutex for the whole of
// worker removal.
class Feature_Servicing_MIDI2ProtocolNegotiationDeadlock
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
