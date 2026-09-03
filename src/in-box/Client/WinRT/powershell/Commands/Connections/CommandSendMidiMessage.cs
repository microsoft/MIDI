// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommunications.Send, "MidiMessage")]
    public class CommandSendMidiMessage : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0)]
        public MidiEndpointConnection? Connection { get; set; }

        [Parameter(Mandatory = true, Position = 1, ValueFromPipeline = true)]
        [ValidateCount(1, 4)]
        public UInt32[] Words { get; set; } = [];

        [Parameter()]
        public UInt64 Timestamp { get; set; } = 0;

        protected override void ProcessRecord()
        {
            var connection = RequireOpenConnection(Connection);

            var result = connection.SendSingleMessageWordArray(Timestamp, 0, (byte)Words.Length, Words);

            if (Windows.Devices.Midi2.MidiEndpointConnection.SendMessageSucceeded(result))
            {
                WriteVerbose($"MIDI message with {Words.Length} UMP words sent with timestamp {Timestamp}.");
                return;
            }

            WriteNonTerminating(
                new InvalidOperationException($"The MIDI message was not sent. Send result: {result}."),
                "MidiMessageSendFailed",
                ErrorCategory.WriteError,
                result);
        }
    }
}
