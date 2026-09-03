// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;

namespace WindowsMidiServices
{
    public class MidiSystemExclusiveMessage
    {
        public byte GroupIndex { get; internal set; }

        // 1-based group as shown to a musician, matching what the rest of the tools display
        public byte GroupNumber { get; internal set; }

        // Includes the F0 and F7 framing, so this can be written straight to a .syx file
        public byte[] Bytes { get; internal set; } = [];

        public int ByteCount => Bytes.Length;

        // True when this block is a fragment of a larger message rather than one or more
        // complete messages
        public bool IsPartial { get; internal set; }

        public string BytesHex => Convert.ToHexString(Bytes);
    }
}
