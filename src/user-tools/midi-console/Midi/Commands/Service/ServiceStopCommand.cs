// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.ServiceProcess;


namespace Microsoft.Midi.ConsoleApp
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
                return (int)MidiConsoleReturnCode.ErrorServiceNotAvailable;
            }



            try
            {
                if (controller.Status == ServiceControllerStatus.Running)
                {
                    AnsiConsole.Write("Attempting to stop service..");
                    MidiServiceHelper.StopServiceWithConsoleStatusUpdate(controller);
                    AnsiConsole.WriteLine();

                    if (controller.Status == ServiceControllerStatus.Stopped)
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Done.") + " Use [cadetblue_1]midi service status[/] to verify service status.");
                        return (int)MidiConsoleReturnCode.Success;
                    }
                    else
                    {
                        AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to stop service. It may be in-use by other apps, or it may be locked up. Consider killing the midisrv.exe process, or rebooting."));
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
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to stop service. It may be in-use by other apps, or it may be locked up. Consider killing the midisrv.exe process, or rebooting."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }
        }
    }
}
