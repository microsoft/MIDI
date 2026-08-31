// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Two halves of one deadlock around protocol negotiation locking.
//
// 1. CMidiEndpointProtocolManager: the endpoint worker map mutex was held across worker
//    teardown. Teardown waits on the worker's own lock, which the negotiation thread holds
//    while calling back into the device manager, and that call can block behind a PnP
//    DeviceWatcher callback. The result is a three way cycle in which every DiscoverAndNegotiate
//    caller and the service control thread wait on the map mutex, so the service cannot be
//    stopped.
//
// 2. CMidiEndpointProtocolWorker::Start: that same worker lock was taken on entry and held for
//    the entire life of the worker, spanning both the discovery wait and the unbounded
//    m_endProcessing wait. Shutdown signals EndProcessing before taking the lock, so the
//    handshake was meant to work, but Start was parked on the discovery event alone and could
//    not see the signal until that timeout expired. Every endpoint teardown therefore serialized
//    behind a worker that was only waiting. Measured as repeated multi second stalls of
//    RemoveEndpoint, DeactivateEndpoint and UpdateEndpointProperties.
//
// Rolling this back restores both previous behaviors: holding the map mutex for the whole of
// worker removal, and holding the worker lock for the whole of negotiation.
class Feature_Servicing_MIDI2ProtocolNegotiationDeadlock
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
