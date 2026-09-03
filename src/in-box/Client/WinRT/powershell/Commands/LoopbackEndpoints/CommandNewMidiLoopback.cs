// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


using System.Management.Automation;

using global::Windows.Devices.Midi2.Transports.Loopback;

namespace WindowsMidiServices
{

    [Cmdlet(VerbsCommon.New, "MidiLoopback", SupportsShouldProcess = true, DefaultParameterSetName = BaseNameParameterSet)]
    [OutputType(typeof(MidiLoopbackEntry))]
    public class CommandNewMidiLoopback : MidiCmdletBase
    {
        private const string BaseNameParameterSet = "BaseName";
        private const string SeparateNamesParameterSet = "SeparateNames";

        // " (A)" and " (B)" are appended to this, matching what the other Windows MIDI Services
        // tools create.
        [Parameter(Mandatory = true, Position = 0, ParameterSetName = BaseNameParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string BaseName { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 0, ParameterSetName = SeparateNamesParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string NameA { get; set; } = string.Empty;

        [Parameter(Mandatory = true, Position = 1, ParameterSetName = SeparateNamesParameterSet)]
        [ValidateNotNullOrWhiteSpace]
        public string NameB { get; set; } = string.Empty;

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
            RequireTransport(MidiLoopbackManager.IsTransportAvailable, "MIDI 2.0 loopback");

            string nameA;
            string nameB;

            if (ParameterSetName == BaseNameParameterSet)
            {
                var root = LoopbackNaming.Truncate(BaseName, LoopbackNaming.MaxPortNameLength - LoopbackNaming.SuffixA.Length);

                nameA = root + LoopbackNaming.SuffixA;
                nameB = root + LoopbackNaming.SuffixB;
            }
            else
            {
                nameA = LoopbackNaming.Truncate(NameA, LoopbackNaming.MaxPortNameLength);
                nameB = LoopbackNaming.Truncate(NameB, LoopbackNaming.MaxPortNameLength);
            }

            if (string.Equals(nameA, nameB, StringComparison.OrdinalIgnoreCase))
            {
                ThrowTerminating(
                    new ArgumentException($"The two endpoint names must differ within the first {LoopbackNaming.MaxPortNameLength} characters."),
                    "MidiLoopbackDuplicateName",
                    ErrorCategory.InvalidArgument,
                    nameA);
            }

            var uniqueId = string.IsNullOrWhiteSpace(UniqueId)
                ? LoopbackNaming.NewUniqueId()
                : LoopbackNaming.CleanUniqueId(UniqueId);

            if (string.IsNullOrEmpty(uniqueId))
            {
                ThrowTerminating(
                    new ArgumentException("The unique identifier must contain at least one letter or digit."),
                    "MidiLoopbackInvalidUniqueId",
                    ErrorCategory.InvalidArgument,
                    UniqueId);
            }

            if (MidiLoopbackManager.DoesLoopbackAExist(uniqueId) || MidiLoopbackManager.DoesLoopbackBExist(uniqueId))
            {
                ThrowTerminating(
                    new ArgumentException($"A loopback with the unique identifier \"{uniqueId}\" already exists."),
                    "MidiLoopbackAlreadyExists",
                    ErrorCategory.ResourceExists,
                    uniqueId);
            }

            if (!ShouldProcess($"{nameA} / {nameB}", "Create MIDI 2.0 loopback endpoint pair"))
            {
                return;
            }

            var creationConfig = new MidiLoopbackCreationConfig(
                new MidiLoopbackEndpointDefinition(nameA, Description, uniqueId),
                new MidiLoopbackEndpointDefinition(nameB, Description, uniqueId))
            {
                IsMuted = Muted.IsPresent
            };

            var response = MidiLoopbackManager.CreateTransientLoopback(creationConfig);

            if (response is null || !response.Success)
            {
                ThrowTerminating(
                    new InvalidOperationException(response is null ? "Unable to create the loopback." : response.ErrorMessage),
                    "MidiLoopbackCreationFailed",
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
