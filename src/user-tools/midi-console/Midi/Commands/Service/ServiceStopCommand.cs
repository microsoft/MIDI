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


    internal class ServiceStopCommand : Command<ServiceStopCommand.Settings>
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
                if (controller.Status == ServiceControllerStatus.Running)
                {
                    AnsiConsole.Write("Stopping service..");
                    MidiServiceHelper.StopServiceWithConsoleStatusUpdate(controller);
                    AnsiConsole.WriteLine();

                    if (controller.Status == ServiceControllerStatus.Stopped)
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Done.") + " Use [cadetblue_1]midi service status[/] to verify service status.");
                        return (int)MidiConsoleReturnCode.Success;
                    }
                    else
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to stop service."));
                        return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
                    }
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning($"Service is not currently running."));
                    return (int)MidiConsoleReturnCode.Success;
                }
            }
            catch (Exception)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to stop service."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }
    }
}
