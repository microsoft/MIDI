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

    internal class ServiceStartCommand : Command<ServiceStartCommand.Settings>
    {
        public sealed class Settings : CommandSettings
        {
        }


        // this does a forced start of the service using SCM. It doesn't do a triggered start
        // like the SDK Initializer does

        public override int Execute(CommandContext context, Settings settings)
        {
            // this command requires admin

            if (!UserHelper.CurrentUserHasAdminRights())
            { 
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("This command must be run as Administrator."));
                return (int)MidiConsoleReturnCode.ErrorInsufficientPermissions;
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
