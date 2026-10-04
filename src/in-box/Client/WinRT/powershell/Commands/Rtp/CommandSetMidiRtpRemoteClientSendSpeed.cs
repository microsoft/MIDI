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
    // Gives one device that connects to a host on this PC its own sending speed, used instead of
    // the host's. A device already connected changes speed straight away.
    [Cmdlet(VerbsCommon.Set, "MidiRtpRemoteClientSendSpeed", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiRtpRemoteClientSettings))]
    public class CommandSetMidiRtpRemoteClientSendSpeed : MidiRtpRemoteClientSpeedCmdletBase
    {
        [Parameter(Mandatory = true)]
        public MidiRtpSendSpeedLimit SendSpeedLimit { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiRtpTransportManager.IsTransportAvailable, Strings.TransportNameRtp);

            var settingsList = GetOtherRemoteClientSettings(out _);

            if (settingsList is null)
            {
                return;
            }

            if (!ShouldProcess(RemoteClientTarget, Strings.RemoteClientSpeedSetAction))
            {
                return;
            }

            var settings = new MidiRtpRemoteClientSettings(RemoteClientTarget)
            {
                SendSpeedLimit = SendSpeedLimit
            };

            settingsList.Add(settings);

            if (SendRemoteClientSettings(settingsList) && PassThru.IsPresent)
            {
                WriteObject(settings);
            }
        }
    }

}
