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


    internal class ServiceRestartCommand : Command<ServiceRestartCommand.Settings>
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
                    AnsiConsole.Write("Stopping service..");

                    MidiServiceHelper.StopServiceWithConsoleStatusUpdate(controller);
                }
                else
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatWarning($"Service is not currently running."));
                }

                // need to wait some amount of time for status to change

                if (controller.Status == ServiceControllerStatus.Stopped)
                {
                    AnsiConsole.Write("Restarting service..");

                    MidiServiceHelper.StartServiceWithConsoleStatusUpdate(controller);
                }

                if (controller.Status != ServiceControllerStatus.Running)
                {
                    AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to restart service. Service is not currently running. It may be locked in a tight loop due to an error."));
                    return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
                }

                AnsiConsole.WriteLine();
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatSuccess("Done.") + " Use [cadetblue_1]midi service status[/] to verify service status.");

            }
            catch (Exception)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError($"Unable to restart service."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }



            return (int)MidiConsoleReturnCode.Success;
        }
    }
}
