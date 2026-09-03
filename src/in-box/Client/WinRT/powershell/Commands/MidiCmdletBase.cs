// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.ServiceConfig;

namespace WindowsMidiServices
{
    public abstract class MidiCmdletBase : PSCmdlet
    {
        // Terminating errors go through ThrowTerminatingError rather than a bare throw so the
        // caller gets an ErrorRecord with a stable FullyQualifiedErrorId to trap on.
        protected void ThrowTerminating(
            Exception exception,
            string errorId,
            ErrorCategory category,
            object? targetObject = null)
        {
            ThrowTerminatingError(new ErrorRecord(exception, errorId, category, targetObject));
        }

        protected void WriteNonTerminating(
            Exception exception,
            string errorId,
            ErrorCategory category,
            object? targetObject = null)
        {
            WriteError(new ErrorRecord(exception, errorId, category, targetObject));
        }

        protected void RequireMidiServices()
        {
            if (!MidiApi.EnsureServiceAvailable())
            {
                ThrowTerminating(
                    new InvalidOperationException("Windows MIDI Services is not available on this PC."),
                    "MidiServicesUnavailable",
                    ErrorCategory.ResourceUnavailable);
            }
        }

        protected void RequireTransport(bool isAvailable, string transportName)
        {
            if (!isAvailable)
            {
                ThrowTerminating(
                    new InvalidOperationException($"The {transportName} transport is not available on this PC."),
                    "MidiTransportUnavailable",
                    ErrorCategory.ResourceUnavailable,
                    transportName);
            }
        }

        protected Windows.Devices.Midi2.MidiEndpointConnection RequireOpenConnection(MidiEndpointConnection? connection)
        {
            if (connection?.BackingConnection is null)
            {
                ThrowTerminating(
                    new ArgumentNullException(nameof(connection), "An open MIDI endpoint connection is required. Use Open-MidiEndpointConnection first."),
                    "MidiConnectionRequired",
                    ErrorCategory.InvalidArgument);
            }

            if (!connection!.BackingConnection!.IsOpen)
            {
                ThrowTerminating(
                    new InvalidOperationException("The MIDI endpoint connection is not open."),
                    "MidiConnectionNotOpen",
                    ErrorCategory.InvalidOperation,
                    connection.EndpointDeviceId);
            }

            return connection.BackingConnection!;
        }

        // Sending a configuration to the service and saving it to the configuration file are
        // separate operations, so a cmdlet applies the change first and persists it only when
        // the caller asked for that. A failed save leaves a working, but transient, change.
        protected void SaveToConfigurationFile(IMidiServiceTransportPluginConfig config)
        {
            var response = MidiServiceTransportPluginConfigManager.SaveUpdate(config);

            if (response is not null && response.Success)
            {
                WriteVerbose($"Saved to the configuration file {response.ConfigFilePath}.");

                if (!string.IsNullOrEmpty(response.BackupFilePath))
                {
                    WriteVerbose($"A backup of the previous configuration was written to {response.BackupFilePath}.");
                }

                return;
            }

            WriteWarning(response is null
                ? "The change was applied, but could not be saved, so it will be lost when the service restarts."
                : $"{response.ErrorMessage} The change was applied, but will be lost when the service restarts.");
        }
    }
}
