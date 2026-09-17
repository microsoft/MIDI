// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.ServiceConfig;
using Windows.Devices.Midi2.Transports.Synth;

namespace WindowsMidiServices
{
    // Changes one or more synthesizer settings. The configuration is sent as a complete set, so
    // this starts from the current status and changes only what was asked for.
    [Cmdlet(VerbsCommon.Set, "MidiSynth", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiSynthStatus))]
    public class CommandSetMidiSynth : MidiCmdletBase
    {
        [Parameter()]
        public SwitchParameter Enabled { get; set; }

        [Parameter()]
        public SwitchParameter Disabled { get; set; }

        [Parameter()]
        public MidiSynthRenderMode? RenderMode { get; set; }

        [Parameter()]
        public MidiSynthAudioOutputMode? AudioOutputMode { get; set; }

        [Parameter()]
        public MidiSynthBankSelectMode? BankSelectMode { get; set; }

        // Zero is the calibrated level, chosen so existing files sound as loud as they always did.
        [Parameter()]
        public double? VolumeDecibels { get; set; }

        [Parameter()]
        public bool? EffectsEnabled { get; set; }

        // Without this the change lasts only until the service restarts, which is a useful way to
        // offer something the customer can undo by rebooting.
        [Parameter()]
        public SwitchParameter Persist { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiSynthManager.IsTransportAvailable, "General MIDI synthesizer");

            if (Enabled.IsPresent && Disabled.IsPresent)
            {
                ThrowTerminating(
                    new ArgumentException("Specify either -Enabled or -Disabled, not both."),
                    "MidiSynthConflictingState",
                    ErrorCategory.InvalidArgument);
            }

            var status = MidiSynthManager.GetStatus();

            if (status is null)
            {
                ThrowTerminating(
                    new InvalidOperationException("The synthesizer did not report its status, so there is nothing to change."),
                    "MidiSynthStatusUnavailable",
                    ErrorCategory.ResourceUnavailable);

                return;
            }

            // Every property is written when the configuration is sent, so it has to start from
            // what the synthesizer is set to now.
            var config = new MidiSynthConfig(status);

            if (Enabled.IsPresent) { config.IsEnabled = true; }
            if (Disabled.IsPresent) { config.IsEnabled = false; }

            if (RenderMode.HasValue) { config.RenderMode = RenderMode.Value; }
            if (AudioOutputMode.HasValue) { config.AudioOutputMode = AudioOutputMode.Value; }
            if (BankSelectMode.HasValue) { config.BankSelectMode = BankSelectMode.Value; }
            if (VolumeDecibels.HasValue) { config.VolumeDecibels = VolumeDecibels.Value; }
            if (EffectsEnabled.HasValue) { config.AreEffectsEnabled = EffectsEnabled.Value; }

            var action = Persist.IsPresent ? "Update and save the settings" : "Update the settings";

            if (!ShouldProcess("General MIDI synthesizer", action))
            {
                return;
            }

            var response = MidiServiceTransportPluginConfigManager.SendUpdate(config);

            if (response is null || response.Status != MidiServiceConfigResponseStatus.Success)
            {
                WriteNonTerminating(
                    new InvalidOperationException(
                        $"The synthesizer settings were not applied: {response?.Status.ToString() ?? "no response"}"),
                    "MidiSynthUpdateFailed",
                    ErrorCategory.WriteError,
                    response);

                return;
            }

            if (Persist.IsPresent)
            {
                SaveToConfigurationFile(config);
            }

            WriteObject(MidiSynthManager.GetStatus());
        }
    }
}
