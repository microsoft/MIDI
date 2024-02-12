using Spectre.Console;
using Spectre.Console.Cli;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Management;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

using System.Security.Principal;
using Microsoft.Devices.Midi2.ConsoleApp.Commands.Service;

namespace Microsoft.Devices.Midi2.ConsoleApp
{
    // commands to check the health of Windows MIDI Services on this PC
    // will check for MIDI services
    // will attempt a loopback test


    internal class ServiceStartCommand : Command<ServiceStartCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }

        public override int Execute(CommandContext context, Settings settings)
        {
            // this command requires admin

            if (!UserHelper.CurrentUserHasAdminRights())
            { 
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("This command must be run as Administrator."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }


            ServiceController controller;

            try
            {
                controller = MidiServiceHelper.GetServiceController();
            }
            catch (Exception)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to find service '{MidiServiceHelper.GetServiceName()}'. Is Windows MIDI Services installed?"));
                return (int)MidiConsoleReturnCode.ErrorMidiServicesNotInstalled;
            }


            try
            {
                if (controller.Status == ServiceControllerStatus.Stopped)
                {
                    AnsiConsole.Write("Starting service..");
                    MidiServiceHelper.StartServiceWithConsoleStatusUpdate(controller);
                    AnsiConsole.WriteLine();
                }
                else
                {
                    if (controller.Status == ServiceControllerStatus.Running)
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning($"Service is already running."));
                        return (int)MidiConsoleReturnCode.Success;
                    }
                    else
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to start service. Service is not currently stopped."));
                        return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
                    }
                }

                if (controller.Status == ServiceControllerStatus.Running)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Done.") + " Use [cadetblue_1]midi service status[/] to verify service status.");
                    return (int)MidiConsoleReturnCode.Success;
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to start service."));
                    return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
                }
            }
            catch (Exception)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to start service."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }
    }
}
