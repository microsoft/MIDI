// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.ServiceProcess;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Helpers
{
    public class MidiServiceHelper
    {
        private const int MAX_TRIES = 50;
        private const int SLEEP_MS_PER_ATTEMPT = 1000;

        public static string GetServiceName()
        {
            return "MidiSrv";
        }

        public static ServiceController? GetServiceController()
        {
            try
            {
                return new System.ServiceProcess.ServiceController(GetServiceName());
            }
            catch (Exception)
            {
                return null;
            }
        }

        public static bool ServiceIsStopped(ServiceController serviceController)
        {
            return serviceController.Status == ServiceControllerStatus.Stopped || serviceController.Status == ServiceControllerStatus.StopPending;
        }

        public static bool ServiceIsRunning(ServiceController serviceController)
        {
            return serviceController.Status == ServiceControllerStatus.Running || serviceController.Status == ServiceControllerStatus.StartPending;
        }

        public static bool ServiceIsReallyRunning(ServiceController serviceController)
        {
            return serviceController.Status == ServiceControllerStatus.Running;
        }


        public static bool StopService(ServiceController controller)
        {
            try
            {
                controller.Stop();

                int i = 0;
                while (i < MAX_TRIES && controller.Status != ServiceControllerStatus.Stopped)
                {
                    Thread.Sleep(SLEEP_MS_PER_ATTEMPT);
                    i++;
                    controller.Refresh();
                }

                return controller.Status == ServiceControllerStatus.Stopped;
            }
            catch (Exception)
            {
                return false;
            }
        }

        public static bool StartService(ServiceController controller)
        {
            try
            {
                controller.Start();

                int i = 0;
                while (i < MAX_TRIES && controller.Status != System.ServiceProcess.ServiceControllerStatus.Running)
                {
                    Thread.Sleep(SLEEP_MS_PER_ATTEMPT);
                    controller.Refresh();

                    i++;
                }

                return controller.Status == ServiceControllerStatus.Running;
            }
            catch (Exception)
            {
                return false;
            }
        }


        public static bool RemoveService(ServiceController controller)
        {
            try
            {
                const int timeoutMilliseconds = 20000;
                string machineName = "";

                ProcessStartInfo psi = new ProcessStartInfo("sc");
                psi.Arguments = string.Format("{0} delete \"{1}\"", machineName, GetServiceName()).Trim();
                psi.RedirectStandardOutput = true;
                psi.UseShellExecute = false;

                var process = Process.Start(psi);

                process.WaitForExit(timeoutMilliseconds);

                var output = process.StandardOutput.ReadToEnd();

                if (process.ExitCode != 0)
                {
                    return false;
                    //throw new Exception(string.Format("Service delete for Windows Service {0} failed.", serviceName));
                }

                return true;
            }
            catch (Exception)
            {
                return false;
            }
        }


    }
}
