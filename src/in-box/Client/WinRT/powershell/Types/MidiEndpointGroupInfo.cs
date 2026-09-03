// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

namespace WindowsMidiServices
{
    public class MidiEndpointGroupInfo
    {
        public byte GroupIndex { get; internal set; }

        // 1-based group as shown to a musician, matching what the rest of the tools display
        public byte GroupNumber { get; internal set; }

        public string Name { get; internal set; } = string.Empty;

        public string Direction { get; internal set; } = string.Empty;

        // Whether the group came from a UMP function block or from a USB group terminal block
        public string BlockKind { get; internal set; } = string.Empty;

        public byte BlockNumber { get; internal set; }

        public bool IsActive { get; internal set; }

        public string EndpointDeviceId { get; internal set; } = string.Empty;
    }
}
