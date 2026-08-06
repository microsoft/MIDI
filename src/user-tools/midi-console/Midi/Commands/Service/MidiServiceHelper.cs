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
    internal class MidiServiceHelper
    {
        public static string GetServiceName()
        {
            return "MidiSrv";
        }

        public static ServiceController GetServiceController()
        {
            return new System.ServiceProcess.ServiceController(GetServiceName());
        }

        public static bool ServiceIsReallyRunning(ServiceController serviceController)
        {
            return serviceController.Status == ServiceControllerStatus.Running;
        }
    }
}
