// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Globalization;
using System.Management.Automation;

using Windows.Devices.Midi2;
using Windows.Devices.Midi2.ServiceConfig;

namespace WindowsMidiServices
{
    public abstract class MidiCmdletBase : PSCmdlet
    {
        // Fills in a Format string from Resources\Strings.resx, with numbers written the way the
        // reader's culture writes them.
        protected static string Format(string format, params object?[] args)
        {
            return string.Format(CultureInfo.CurrentCulture, format, args);
        }

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
                    new InvalidOperationException(Strings.ServiceUnavailable),
                    "MidiServicesUnavailable",
                    ErrorCategory.ResourceUnavailable);
            }
        }

        protected void RequireTransport(bool isAvailable, string transportName)
        {
            if (!isAvailable)
            {
                ThrowTerminating(
                    new InvalidOperationException(Format(Strings.TransportUnavailableFormat, transportName)),
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
                    new ArgumentNullException(nameof(connection), Strings.ConnectionRequired),
                    "MidiConnectionRequired",
                    ErrorCategory.InvalidArgument);
            }

            if (!connection!.BackingConnection!.IsOpen)
            {
                ThrowTerminating(
                    new InvalidOperationException(Strings.ConnectionNotOpen),
                    "MidiConnectionNotOpen",
                    ErrorCategory.InvalidOperation,
                    connection.EndpointDeviceId);
            }

            return connection.BackingConnection!;
        }

        // For a cmdlet which takes an endpoint device id instead of an open connection, so a single
        // operation needs no Start-MidiSession or Open-MidiEndpointConnection first. The session is
        // named after the cmdlet, which is what shows in the list of sessions using the endpoint.
        internal MidiTemporaryConnection OpenTemporaryConnection(string endpointDeviceId)
        {
            var session = Windows.Devices.Midi2.MidiSession.Create(
                Format(Strings.SessionTemporaryNameFormat, MyInvocation.MyCommand.Name));

            if (session is null)
            {
                ThrowTerminating(
                    new InvalidOperationException(Strings.SessionCreationFailed),
                    "MidiSessionFailed",
                    ErrorCategory.ResourceUnavailable);
            }

            var connection = session!.CreateEndpointConnection(endpointDeviceId);

            if (connection is null)
            {
                session.Dispose();

                ThrowTerminating(
                    new InvalidOperationException(Format(Strings.ConnectionCreationFailedFormat, endpointDeviceId)),
                    "MidiConnectionCreationFailed",
                    ErrorCategory.ResourceUnavailable,
                    endpointDeviceId);
            }

            if (!connection!.Open())
            {
                session.Dispose();

                ThrowTerminating(
                    new InvalidOperationException(Format(Strings.ConnectionOpenFailedFormat, endpointDeviceId)),
                    "MidiConnectionOpenFailed",
                    ErrorCategory.OpenError,
                    endpointDeviceId);
            }

            return new MidiTemporaryConnection(session, connection);
        }

        // Sending a configuration to the service and saving it to the configuration file are
        // separate operations, so a cmdlet applies the change first and persists it only when
        // the caller asked for that. A failed save leaves a working, but transient, change.
        protected void SaveToConfigurationFile(IMidiServiceTransportPluginConfig config)
        {
            var response = MidiServiceTransportPluginConfigManager.SaveUpdate(config);

            if (response is not null && response.Success)
            {
                WriteVerbose(Format(Strings.ConfigurationSavedFormat, response.ConfigFilePath));

                if (!string.IsNullOrEmpty(response.BackupFilePath))
                {
                    WriteVerbose(Format(Strings.ConfigurationBackupWrittenFormat, response.BackupFilePath));
                }

                return;
            }

            WriteWarning(response is null
                ? Strings.ConfigurationNotSaved
                : Format(Strings.ConfigurationNotSavedReasonFormat, response.ErrorMessage));
        }
    }
}
