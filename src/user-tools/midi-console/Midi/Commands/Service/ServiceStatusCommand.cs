// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Management;
using System.ServiceProcess;


namespace Microsoft.Midi.ConsoleApp
{
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class ServiceStatusCommand : Command<ServiceStatusCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
            [LocalizedDescription("ParameterServiceStatusVerbose")]
            [CommandOption("-v|--verbose")]
            [DefaultValue(false)]
            public bool Verbose { get; set; }
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            // check to see if the service is running. 
            // NOTE: Equivalent code can't be moved to the SDK due to Desktop/WinRT limitations.

            string serviceName = "MidiSrv";

            ServiceController controller;

            try
            {
                controller = new System.ServiceProcess.ServiceController(serviceName);

                if (controller.Status == System.ServiceProcess.ServiceControllerStatus.Running)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess($"Service {serviceName} is running."));
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Service '{serviceName}' status is {controller.Status.ToString()}. You may want to restart the service or reboot."));
                }
            }
            catch (InvalidOperationException)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to find service '{serviceName}'. Is Windows MIDI Services installed?\n"));
                return (int)MidiConsoleReturnCode.ErrorMidiServicesNotInstalled;
            }


            // report more details about the service

            if (settings.Verbose)
            {
                AnsiConsole.WriteLine();

                var table = new Table();

                table.AddColumn("Property");
                table.AddColumn("Value");

                table.Rows.Add(new[] { new Markup("Name"), new Markup(controller.ServiceName) });
                table.Rows.Add(new[] { new Markup("Display Name"), new Markup(controller.DisplayName) });

                using (var serviceObject = new ManagementObject(new ManagementPath(string.Format("Win32_Service.Name='{0}'", serviceName))))
                {
                    if (serviceObject != null) 
                    {
                        var desc = (string?)serviceObject.GetPropertyValue("Description");

                        if (desc != null)
                        {
                            string description = desc.ToString();

                            table.Rows.Add(new[] { new Markup("Description"), new Markup(description) });
                        }
                    }
                }

                table.Rows.Add(new[] { new Markup("Type"), new Markup(controller.ServiceType.ToString()) });
                table.Rows.Add(new[] { new Markup("Start Type"), new Markup(controller.StartType.ToString()) });
                table.Rows.Add(new[] { new Markup("Status"), new Markup(controller.Status.ToString()) });

                AnsiConsole.Write(table);

            }
            AnsiConsole.WriteLine();

            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
