// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Endpoint properties recorded what the service asked for during protocol negotiation, not what
// the endpoint agreed to.
//
// In CMidiEndpointProtocolWorker::ProcessStreamConfigurationRequest, when the endpoint's Stream
// Configuration Notification still did not match after the one Stream Configuration Request, the
// service built a notification from its own preferences, recorded that in the endpoint
// properties, and sent it to the endpoint. UMP 1.1.2 section 7.1.6 says an endpoint which cannot
// make a change replies with its current configuration, and that reply is final. So the
// "configured protocol" and "configured to send / receive JR timestamps" properties could claim a
// protocol or JR setting the endpoint had turned down.
//
// When the endpoint did agree, the service also recorded a notification it built itself rather
// than the endpoint's. That one carried the two JR flags from the service's side of the link, so
// whenever JR was on in only one direction, "configured to send" and "configured to receive" were
// reversed.
//
// The fix records the endpoint's own notification in both cases, and in the first case no longer
// sends the service's notification to the endpoint. When the endpoint agrees, the service still
// sends its notification exactly as before.
//
// Rolling this back restores recording the service's own notification in both cases, and sending
// it to an endpoint which declined.
class Feature_Servicing_MIDI2StreamConfigurationFromEndpoint
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};

inline bool Feature_Servicing_MIDI2StreamConfigurationFromEndpoint_IsEnabled()
{
    return true;
}
