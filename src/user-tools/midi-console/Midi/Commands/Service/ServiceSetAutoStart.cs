// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System.Runtime.InteropServices;
using System.ServiceProcess;

namespace Microsoft.Midi.ConsoleApp
{
    internal class ServiceSetAutoStart : Command<ServiceSetAutoStart.Settings>
    {
        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern Boolean ChangeServiceConfig(
            IntPtr hService,
            UInt32 nServiceType,
            UInt32 nStartType,
            UInt32 nErrorControl,
            String lpBinaryPathName,
            String lpLoadOrderGroup,
            IntPtr lpdwTagId,
            [In] char[] lpDependencies,
            String lpServiceStartName,
            String lpPassword,
            String lpDisplayName);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        static extern IntPtr OpenService(
            IntPtr hSCManager, string lpServiceName, uint dwDesiredAccess);

        [DllImport("advapi32.dll", EntryPoint = "OpenSCManagerW", ExactSpelling = true, CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern IntPtr OpenSCManager(
            string machineName, string databaseName, uint dwAccess);

        [DllImport("advapi32.dll", EntryPoint = "CloseServiceHandle")]
        public static extern int CloseServiceHandle(IntPtr hSCObject);

        private const uint SERVICE_NO_CHANGE = 0xFFFFFFFF;
        private const uint SERVICE_QUERY_CONFIG = 0x00000001;
        private const uint SERVICE_CHANGE_CONFIG = 0x00000002;
        private const uint SC_MANAGER_ALL_ACCESS = 0x000F003F;


        public sealed class Settings : CommandSettings
        {
            // TODO: Consider changing this to a command argument instead of an option
            [LocalizedDescription("ParameterServiceRestart")]
            [CommandOption("-r|--restart-service|--restart")]
            [DefaultValue(false)]
            public bool Restart { get; set; }

        }

        public override int Execute(CommandContext context, Settings settings)
        {
            // this command requires admin

            if (!UserHelper.CurrentUserHasAdminRights())
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("This command must be run as Administrator."));
                return (int)MidiConsoleReturnCode.ErrorInsufficientPermissions;
            }

            AnsiConsole.MarkupLine("Setting service to auto-start...");

            var scManagerHandle = OpenSCManager(null, null, SC_MANAGER_ALL_ACCESS);
            if (scManagerHandle == IntPtr.Zero)
            {
                //throw new ExternalException("Open Service Manager Error");
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Error opening service manager."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var serviceHandle = OpenService(
                scManagerHandle,
                MidiServiceHelper.GetServiceName(),
                SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG);

            if (serviceHandle == IntPtr.Zero)
            {
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Error opening service."));
                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            var result = ChangeServiceConfig(
                serviceHandle,
                SERVICE_NO_CHANGE,
                (uint)ServiceStartMode.Automatic,
                SERVICE_NO_CHANGE,
                null,
                null,
                IntPtr.Zero,
                null,
                null,
                null,
                null);

            if (!result)
            {
                int nError = Marshal.GetLastWin32Error();
                var win32Exception = new Win32Exception(nError);
                
                AnsiConsole.MarkupLine(AnsiMarkupFormatter.FormatError("Could not change service start type: " + win32Exception.Message));

                return (int)MidiConsoleReturnCode.ErrorGeneralFailure;
            }

            CloseServiceHandle(serviceHandle);
            CloseServiceHandle(scManagerHandle);

            var controller = MidiServiceHelper.GetServiceController();

            if (settings.Restart)
            {
                AnsiConsole.MarkupLine("Stopping service.");
                MidiServiceHelper.StopServiceWithConsoleStatusUpdate(controller);

                AnsiConsole.MarkupLine("Restarting service.");
                MidiServiceHelper.StartServiceWithConsoleStatusUpdate(controller);
            }

            AnsiConsole.MarkupLine("Succeeded");


            return (int)MidiConsoleReturnCode.Success;
        }

    }
}
