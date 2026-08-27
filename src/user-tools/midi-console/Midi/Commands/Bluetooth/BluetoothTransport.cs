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
    internal static class BluetoothTransport
    {
        internal static bool EnsureTransportAvailable()
        {
            if (MidiBluetoothTransportManager.IsTransportAvailable)
            {
                return true;
            }

            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("The Bluetooth MIDI transport is not installed."));
            AnsiConsole.WriteLine();
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage(
                "Transports which are installed are listed by 'midi enumerate transport-plugins'."));

            return false;
        }

        // A machine with no Bluetooth, or with a radio which cannot act as a central, otherwise
        // looks exactly like a machine where nothing happens to be switched on.
        internal static void ReportRadioLimitations()
        {
            var radio = MidiBluetoothTransportManager.GetRadioInformation();

            if (radio == null)
            {
                return;
            }

            if (!radio.IsPresent)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC has no Bluetooth radio, so no Bluetooth MIDI device can be found."));
                AnsiConsole.WriteLine();
            }
            else if (!radio.IsLowEnergySupported)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC's Bluetooth radio does not support Bluetooth Low Energy, which Bluetooth MIDI requires."));
                AnsiConsole.WriteLine();
            }
            else if (!radio.IsCentralRoleSupported)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning("This PC's Bluetooth radio cannot connect out to devices, so no Bluetooth MIDI device can be found."));
                AnsiConsole.WriteLine();
            }
        }

        internal static void ReportFailure(string errorCode, string errorMessage, int errorHResult)
        {
            AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(
                $"The Bluetooth MIDI transport reported {AnsiMarkupFormatter.EscapeString(errorCode)}."));

            if (!string.IsNullOrEmpty(errorMessage))
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(AnsiMarkupFormatter.EscapeString(errorMessage)));
            }

            // Some causes share an error code, so the raw value is the only way to tell them apart
            if (errorHResult != 0)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatGeneralDetailMessage($"HRESULT 0x{errorHResult:X8}"));
            }
        }

        // A device only reports its protocol once connected, because reading it means reading the
        // device's characteristics.
        internal static string FormatProtocol(MidiBluetoothProtocol protocol)
        {
            return protocol switch
            {
                MidiBluetoothProtocol.BluetoothLowEnergyMidi1 => "MIDI 1.0",
                MidiBluetoothProtocol.BluetoothLowEnergyMidi2Ump => "MIDI 2.0 UMP",
                _ => "-",
            };
        }

        // The twelve hex digit form is what every Bluetooth command here takes as an identifier.
        internal static string FormatAddress(ulong address)
        {
            return address == 0 ? string.Empty : $"{address:X12}";
        }

        // The image is loaded by name from a known folder, so a path would either escape that
        // folder or simply fail to load.
        internal static ValidationResult ValidateCustomizationOptions(string? name, string? description, string? image, bool clear)
        {
            if (clear && (name != null || description != null || image != null))
            {
                return ValidationResult.Error("--clear removes every customization, so it cannot be combined with --name, --description, or --image.");
            }

            if (!clear && name == null && description == null && image == null)
            {
                return ValidationResult.Error("Nothing to change. Supply --name, --description, or --image, or use --clear to remove the customization.");
            }

            if (!string.IsNullOrEmpty(image) &&
                (image.Contains('\\') || image.Contains('/') || Path.IsPathRooted(image)))
            {
                return ValidationResult.Error("The image must be a file name only, not a path.");
            }

            return ValidationResult.Success();
        }

        // Applying a change and keeping it are separate steps, so a rejected change is never
        // written to the configuration file.
        internal static bool ApplyCustomization(
            string endpointDeviceInstanceId,
            string? name,
            string? description,
            string? image,
            bool clear,
            bool save)
        {
            var matchCriteria = new MidiServiceConfigEndpointMatchCriteria
            {
                DeviceInstanceId = endpointDeviceInstanceId,
            };

            var config = new MidiServiceEndpointCustomizationConfig(MidiBluetoothTransportManager.TransportId)
            {
                MatchCriteria = matchCriteria,
            };

            if (clear)
            {
                config.ClearDisplayProperties = true;
            }
            else
            {
                // A property left alone keeps its stored value, because the save merges
                if (name != null) config.Name = name.Trim();
                if (description != null) config.Description = description.Trim();
                if (image != null) config.ImageFileName = image.Trim();
            }

            var response = MidiServiceTransportPluginConfigManager.SendUpdate(config);

            if (response.Status != MidiServiceConfigResponseStatus.Success)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(
                    $"The Bluetooth MIDI transport rejected the update. Status {response.Status}, code {response.ServiceErrorCode}."));

                if (!string.IsNullOrEmpty(response.ServiceErrorMessage))
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError(response.ServiceErrorMessage));
                }

                return false;
            }

            if (!save)
            {
                return true;
            }

            if (clear)
            {
                // Saving empty values would leave a stored entry which says nothing, so the entry
                // is taken out of the file instead.
                ConfigFileSaver.ReportSave(
                    new MidiServiceEndpointCustomizationRemovalConfig(MidiBluetoothTransportManager.TransportId, matchCriteria));
            }
            else
            {
                ConfigFileSaver.ReportSave(config);
            }

            return true;
        }
    }
}
