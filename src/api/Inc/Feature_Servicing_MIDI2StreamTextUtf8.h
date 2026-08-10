// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// UMP stream text (Endpoint Name, Product Instance Id, Function Block Name) is UTF-8, but
// CMidiEndpointProtocolWorker::ParseStreamTextMessage widened each byte into its own character.
// Any name outside ASCII was therefore stored mangled: a right single quotation mark, U+2019,
// arrives as E2 80 99 and became three characters, one visible and two invisible C1 controls.
//
// The fix decodes the text once it has been fully accumulated. It cannot be decoded per message,
// because a multi-byte sequence can straddle two stream messages.
//
// Rolling this back restores the previous behaviour of storing one UTF-8 byte per character.
class Feature_Servicing_MIDI2StreamTextUtf8
{
public:
    static bool IsEnabled()
    {
        return true;
    }
};
