// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2.Transports.BasicLoopback;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.New, "MidiBasicLoopback", SupportsShouldProcess = true)]
    [OutputType(typeof(MidiBasicLoopbackEntry))]
    public class CommandNewMidiBasicLoopback : MidiCmdletBase
    {
        [Parameter(Mandatory = true, Position = 0)]
        [ValidateNotNullOrWhiteSpace]
        public string Name { get; set; } = string.Empty;

        [Parameter]
        public string Description { get; set; } = string.Empty;

        [Parameter]
        public string UniqueId { get; set; } = string.Empty;

        [Parameter]
        public SwitchParameter Muted { get; set; }

        [Parameter]
        public SwitchParameter SaveToConfiguration { get; set; }

        protected override void ProcessRecord()
        {
            RequireMidiServices();
            RequireTransport(MidiBasicLoopbackManager.IsTransportAvailable, "MIDI 1.0 basic loopback");

            var name = LoopbackNaming.Truncate(Name, LoopbackNaming.MaxPortNameLength);

            var uniqueId = string.IsNullOrWhiteSpace(UniqueId)
                ? LoopbackNaming.NewUniqueId()
                : LoopbackNaming.CleanUniqueId(UniqueId);

            if (string.IsNullOrEmpty(uniqueId))
            {
                ThrowTerminating(
                    new ArgumentException("The unique identifier must contain at least one letter or digit."),
                    "MidiBasicLoopbackInvalidUniqueId",
                    ErrorCategory.InvalidArgument,
                    UniqueId);
            }

            if (MidiBasicLoopbackManager.DoesLoopbackExist(uniqueId))
            {
                ThrowTerminating(
                    new ArgumentException($"A basic loopback with the unique identifier \"{uniqueId}\" already exists."),
                    "MidiBasicLoopbackAlreadyExists",
                    ErrorCategory.ResourceExists,
                    uniqueId);
            }

            if (!ShouldProcess(name, "Create MIDI 1.0 basic loopback endpoint"))
            {
                return;
            }

            var creationConfig = new MidiBasicLoopbackCreationConfig(
                new MidiBasicLoopbackEndpointDefinition(name, Description, uniqueId))
            {
                IsMuted = Muted.IsPresent
            };

            var response = MidiBasicLoopbackManager.CreateTransientLoopback(creationConfig);

            if (response is null || !response.Success)
            {
                ThrowTerminating(
                    new InvalidOperationException(response is null ? "Unable to create the basic loopback." : response.ErrorMessage),
                    "MidiBasicLoopbackCreationFailed",
                    ErrorCategory.InvalidOperation,
                    creationConfig);

                return;
            }

            if (SaveToConfiguration.IsPresent)
            {
                SaveToConfigurationFile(creationConfig);
            }
            else
            {
                WriteVerbose("This loopback is transient and will disappear when the service restarts. Use -SaveToConfiguration to keep it.");
            }

            WriteObject(response.CreatedLoopbackEntry);
        }
    }

}
