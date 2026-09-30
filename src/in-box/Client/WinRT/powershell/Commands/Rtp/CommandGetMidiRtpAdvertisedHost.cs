// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Rtp;

namespace WindowsMidiServices
{
    // The RTP-MIDI devices this PC can currently see advertised on the network. A host on this PC
    // shows up too, marked IsThisPc.
    [Cmdlet(VerbsCommon.Get, "MidiRtpAdvertisedHost")]
    [OutputType(typeof(MidiRtpAdvertisedHost))]
    public class CommandGetMidiRtpAdvertisedHost : MidiCmdletBase
    {
        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiRtpTransportManager.IsTransportAvailable, Strings.TransportNameRtp);

            var hosts = MidiRtpTransportManager.GetAdvertisedHosts();

            if (hosts is null)
            {
                return;
            }

            foreach (var host in hosts)
            {
                WriteObject(host);
            }
        }
    }

}
