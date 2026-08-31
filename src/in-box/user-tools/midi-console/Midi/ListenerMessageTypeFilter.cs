// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================



namespace Microsoft.Midi.ConsoleApp
{
    public enum ListenerMessageTypeFilter
    {
        All = 0,

        SysEx7,
        SysEx8,

        Midi1ChannelVoice,
        Midi2ChannelVoice,

        //FlexDataMessages,

        Stream,
    }

}
