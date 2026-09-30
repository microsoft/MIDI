// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.CapabilityInquiry;

namespace WindowsMidiServices
{
    // Asks a device, over MIDI-CI Property Exchange, what each of its channels is called and what
    // it is set to right now.
    [Cmdlet(VerbsCommon.Get, "MidiChannelList", DefaultParameterSetName = EndpointDeviceIdParameterSet)]
    [OutputType(typeof(MidiChannelListEntry))]
    public class CommandGetMidiChannelList : MidiCapabilityInquiryCmdletBase
    {
        protected override void ProcessRecord()
        {
            QueryPropertyExchangeResponders((session, responder) =>
            {
                var channels = session.GetChannelListAsync(responder.Muid).GetAwaiter().GetResult();

                if (channels is null)
                {
                    WriteNonTerminating(
                        new InvalidOperationException(Format(Strings.CiChannelListNotSentFormat, responder.Muid)),
                        "MidiChannelListUnavailable",
                        ErrorCategory.ResourceUnavailable,
                        QueriedEndpointDeviceId);

                    return;
                }

                foreach (var entry in channels.Entries)
                {
                    WriteObject(entry);
                }
            });
        }
    }
}
