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
    // Gives one device that connects to a host on this PC settings of its own, used instead of the
    // host's: how fast the host sends to it, and how long its endpoint stays while it is away.
    // Like Connect-MidiRtpHost, this sets all of them: anything left out gets its default. A device
    // already connected changes speed straight away, and its endpoint follows the new offline
    // retention when it next leaves.
    [Cmdlet(VerbsCommon.Set, "MidiRtpRemoteClientSettings", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiRtpRemoteClientSettings))]
    public class CommandSetMidiRtpRemoteClientSettings : MidiRtpRemoteClientSettingsCmdletBase
    {
        // How fast the host sends to the device. A slower speed helps a device that loses data when
        // a lot of it arrives at once, like a long SysEx dump. A lone message is never delayed.
        [Parameter]
        public MidiRtpSendSpeedLimit SendSpeedLimit { get; set; } = MidiRtpSendSpeedLimit.Unlimited;

        // Send every message, however late, rather than skipping some in-between steps of long
        // control change, pressure and pitch bend moves to catch up
        [Parameter]
        public SwitchParameter NoSkippingInBetweenValues { get; set; }

        // How long the endpoint and its MIDI 1.0 ports stay while the device is away: 0 removes them
        // at once (the default), -1 keeps them always, so an app started while the device is off
        // still finds its port, and anything else is a number of seconds.
        [Parameter]
        [ValidateRange(-1, 86400)]
        public int OfflineRetentionSeconds { get; set; }

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

            if (!ShouldProcess(RemoteClientTarget, Strings.RemoteClientSettingsSetAction))
            {
                return;
            }

            var settings = new MidiRtpRemoteClientSettings(RemoteClientTarget)
            {
                SendSpeedLimit = SendSpeedLimit,
                SkipInBetweenValuesToCatchUp = !NoSkippingInBetweenValues.IsPresent,
                OfflineRetentionSeconds = OfflineRetentionSeconds
            };

            settingsList.Add(settings);

            if (SendRemoteClientSettings(settingsList) && PassThru.IsPresent)
            {
                WriteObject(settings);
            }
        }
    }

}
