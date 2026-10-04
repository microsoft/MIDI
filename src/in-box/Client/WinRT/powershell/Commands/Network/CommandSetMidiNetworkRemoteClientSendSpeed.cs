// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.Network;

namespace WindowsMidiServices
{
    // Gives one device that connects to a host on this PC its own sending speed, used instead of
    // the host's. A device already connected changes speed straight away.
    [Cmdlet(VerbsCommon.Set, "MidiNetworkRemoteClientSendSpeed", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiNetworkRemoteClientSettings))]
    public class CommandSetMidiNetworkRemoteClientSendSpeed : MidiNetworkRemoteClientSpeedCmdletBase
    {
        [Parameter(Mandatory = true)]
        public MidiNetworkSendSpeedLimit SendSpeedLimit { get; set; }

        // Send more slowly while the device keeps asking for data again
        [Parameter]
        public SwitchParameter ReduceSendSpeedAutomatically { get; set; }

        [Parameter]
        public SwitchParameter PassThru { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiNetworkTransportManager.IsTransportAvailable, Strings.TransportNameNetwork);

            var settingsList = GetOtherRemoteClientSettings(out _);

            if (settingsList is null)
            {
                return;
            }

            if (!ShouldProcess(RemoteClientTarget, Strings.RemoteClientSpeedSetAction))
            {
                return;
            }

            var settings = new MidiNetworkRemoteClientSettings(RemoteClientName.Trim(), RemoteClientProductInstanceId.Trim())
            {
                SendSpeedLimit = SendSpeedLimit,
                ReduceSendSpeedAutomatically = ReduceSendSpeedAutomatically.IsPresent
            };

            settingsList.Add(settings);

            if (SendRemoteClientSettings(settingsList) && PassThru.IsPresent)
            {
                WriteObject(settings);
            }
        }
    }

}
