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
    public class MessageReceivedEventArgs : EventArgs
    {
        public UInt64 Timestamp { get; }
        public UInt32[] Words { get; }

        public MessageReceivedEventArgs(UInt64 timestamp, UInt32[] words)
        {
            Timestamp = timestamp;
            Words = words;
        }
    }
}
