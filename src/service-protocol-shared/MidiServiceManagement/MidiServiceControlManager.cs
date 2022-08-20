// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System.Linq.Expressions;
using System.ServiceProcess;

namespace Microsoft.Windows.Midi.Internal.ServiceControl
{
    public class MidiServiceControlManager
    {
        // don't change this here unless you also change it in the installer
        private const string ServiceName = "MIDI Service";

        private static readonly TimeSpan DefaultTimeOut = TimeSpan.FromSeconds(30);


        public static bool DoesServiceExist()
        {
            var services = ServiceController.GetServices()
                .Where(service => service.ServiceName == ServiceName);

            if (services != null && services.Count() > 0)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        public static bool IsServiceRunning()
        {
            ServiceController sc = new ServiceController();
            sc.ServiceName = ServiceName;

            if (sc.Status == ServiceControllerStatus.Running)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        public static bool StopService()
        {
            try
            {
                ServiceController sc = new ServiceController();
                sc.ServiceName = ServiceName;

                if (sc.Status != ServiceControllerStatus.Stopped &&
                    sc.Status != ServiceControllerStatus.StopPending)
                {
                    sc.Stop();
                    sc.WaitForStatus(ServiceControllerStatus.Stopped, DefaultTimeOut);
                }

                return true;
            }
            catch (System.ServiceProcess.TimeoutException)
            {
                return false;
            }
            catch (InvalidOperationException)
            {
                // TODO: log this? Service could not be stopped
                throw;
            }
        }


        public static bool StartService()
        {
            try
            {
                ServiceController sc = new ServiceController();
                sc.ServiceName = ServiceName;

                if (sc.Status != ServiceControllerStatus.Running)
                {
                    sc.Start();

                    sc.WaitForStatus(ServiceControllerStatus.Running, DefaultTimeOut);
                }

                return true;
            }
            catch (System.ServiceProcess.TimeoutException)
            {
                return false;
            }
            catch (InvalidOperationException)
            {
                // TODO: log this? Service could not be started
                throw;
            }
        }




    }
}