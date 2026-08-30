// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Windows.Devices.Midi2.ServiceConfig;
using Windows.Devices.Midi2.Transports.Bluetooth;

namespace Microsoft.Midi.ConsoleApp
{
    internal static class BluetoothClientDecision
    {
        internal const string ScopeOnce = "once";
        internal const string ScopeUntilRestart = "untilrestart";
        internal const string ScopeAlways = "always";

        internal static ValidationResult ValidateScope(string? scope)
        {
            if (string.IsNullOrWhiteSpace(scope))
            {
                return ValidationResult.Success();
            }

            var value = scope.Trim().ToLowerInvariant();

            if (value != ScopeOnce && value != ScopeUntilRestart && value != ScopeAlways)
            {
                return ValidationResult.Error("Scope must be once, untilRestart or always.");
            }

            return ValidationResult.Success();
        }

        internal static MidiBluetoothApprovalScope ResolveScope(string? scope)
        {
            var value = (scope ?? string.Empty).Trim().ToLowerInvariant();

            return value switch
            {
                ScopeAlways => MidiBluetoothApprovalScope.Always,
                ScopeUntilRestart => MidiBluetoothApprovalScope.UntilRestart,
                _ => MidiBluetoothApprovalScope.Once,
            };
        }

        internal static string DescribeScope(MidiBluetoothApprovalScope scope)
        {
            return scope switch
            {
                MidiBluetoothApprovalScope.Always => "permanently",
                MidiBluetoothApprovalScope.UntilRestart => "until the service restarts",
                _ => "for this connection only",
            };
        }

        internal static int Report(MidiBluetoothPeripheralClientDecisionResponse response, bool approved)
        {
            if (response == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("No response from the Bluetooth MIDI transport."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (!response.Success)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, 0);
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var name = string.IsNullOrEmpty(response.Name)
                ? AnsiMarkupFormatter.EscapeString(response.BluetoothAddress)
                : AnsiMarkupFormatter.EscapeString(response.Name);

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(
                $"'{name}' {(approved ? "approved" : "denied")} {DescribeScope(response.AppliedScope)}."));

            if (approved)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                    "A MIDI endpoint for it appears in a moment. Use 'midi bluetooth peripheral status' to see it."));
            }

            // The service applies every scope immediately but never writes the configuration file.
            if (response.PersistRequired)
            {
                if (TryPersistLists(out var persistError))
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                        "Written to the configuration file, so it survives a service restart."));
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                        $"This decision is in effect now, but could not be written to the configuration file, so a service restart will forget it. {persistError}"));
                }
            }

            return (int)MidiConsoleReturnCode.Success;
        }

        // The lists are stored whole rather than merged, so the complete set is read back from the
        // service, which is what holds it, and written out together.
        internal static bool TryPersistLists(out string error)
        {
            error = string.Empty;

            try
            {
                var status = MidiBluetoothTransportManager.GetPeripheralStatus();

                if (status == null)
                {
                    error = "The service did not report its current lists.";
                    return false;
                }

                var config = new MidiBluetoothPeripheralClientListConfig(status);

                var saveResponse = MidiServiceTransportPluginConfigManager.SaveUpdate(config);

                if (saveResponse == null)
                {
                    error = "The configuration file writer did not respond.";
                    return false;
                }

                error = saveResponse.ErrorMessage;

                return saveResponse.Success;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }
    }

    internal class BluetoothPeripheralApproveCommand : Command<BluetoothPeripheralApproveCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothClientAddress")]
            [CommandArgument(0, "<bluetooth address>")]
            public string? BluetoothAddress { get; set; }

            [LocalizedDescription("ParameterBluetoothApprovalScope")]
            [CommandOption("-s|--scope")]
            public string? Scope { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.BluetoothAddress))
            {
                return ValidationResult.Error("Missing Bluetooth address. Use 'midi bluetooth peripheral status' to see what is waiting.");
            }

            return BluetoothClientDecision.ValidateScope(settings.Scope);
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var scope = BluetoothClientDecision.ResolveScope(settings.Scope);

            var response = MidiBluetoothTransportManager.ApprovePeripheralClientAsync(
                settings.BluetoothAddress!, scope).GetAwaiter().GetResult();

            return BluetoothClientDecision.Report(response, true);
        }
    }

    internal class BluetoothPeripheralDenyCommand : Command<BluetoothPeripheralDenyCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothClientAddress")]
            [CommandArgument(0, "<bluetooth address>")]
            public string? BluetoothAddress { get; set; }

            [LocalizedDescription("ParameterBluetoothApprovalScope")]
            [CommandOption("-s|--scope")]
            public string? Scope { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.BluetoothAddress))
            {
                return ValidationResult.Error("Missing Bluetooth address. Use 'midi bluetooth peripheral status' to see what is waiting.");
            }

            return BluetoothClientDecision.ValidateScope(settings.Scope);
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var scope = BluetoothClientDecision.ResolveScope(settings.Scope);

            var response = MidiBluetoothTransportManager.DenyPeripheralClientAsync(
                settings.BluetoothAddress!, scope).GetAwaiter().GetResult();

            return BluetoothClientDecision.Report(response, false);
        }
    }

    internal class BluetoothPeripheralForgetCommand : Command<BluetoothPeripheralForgetCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterBluetoothClientAddress")]
            [CommandArgument(0, "<bluetooth address>")]
            public string? BluetoothAddress { get; set; }
        }

        public override ValidationResult Validate(CommandContext context, Settings settings)
        {
            if (string.IsNullOrWhiteSpace(settings.BluetoothAddress))
            {
                return ValidationResult.Error("Missing Bluetooth address.");
            }

            return ValidationResult.Success();
        }

        public override int Execute(CommandContext context, Settings settings, CancellationToken cancellationToken)
        {
            if (!BluetoothTransport.EnsureTransportAvailable())
            {
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var response = MidiBluetoothTransportManager.ForgetPeripheralClientAsync(
                settings.BluetoothAddress!).GetAwaiter().GetResult();

            if (response == null)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("No response from the Bluetooth MIDI transport."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            if (!response.Success)
            {
                BluetoothTransport.ReportFailure(response.ErrorCode.ToString(), response.ErrorMessage, 0);
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess(
                $"Forgot the remembered decision for {AnsiMarkupFormatter.EscapeString(settings.BluetoothAddress!)}. It will be asked about again."));

            if (response.PersistRequired && !BluetoothClientDecision.TryPersistLists(out var persistError))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning(
                    $"The configuration file could not be updated, so a service restart will bring this decision back. {persistError}"));
            }

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
