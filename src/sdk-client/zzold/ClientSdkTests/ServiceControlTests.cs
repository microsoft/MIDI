// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.


using Microsoft.Windows.Midi;
using System.Security.Principal;
using static System.Net.Mime.MediaTypeNames;

namespace ClientSdkTests
{
    [TestClass]
    public class ServiceControlTests
    {


        [TestMethod]
        public void TestPing()
        {
            string reportedVersion;

            MidiServicePingResponse response = MidiService.Ping(out reportedVersion);

            switch (response)
            {
                case MidiServicePingResponse.Error:
                    Assert.Fail("Ping: Error.");
                    break;
                case MidiServicePingResponse.TimeOut:
                    Assert.Fail("Ping: Time out.");
                    break;
                case MidiServicePingResponse.Pong:
                    // we're good
                    System.Diagnostics.Debug.WriteLine($"Ping: Server version returned: {reportedVersion}");
                    break;
            }
        }

        [TestMethod]
        public void ServiceIsRunning()
        {
            Assert.IsTrue(MidiService.IsRunning());
        }

        [TestMethod]
        public void ServiceIsInstalled()
        {
            Assert.IsTrue(MidiService.IsInstalled()) ;
        }


        [TestMethod]
        public void StopAndRestartService()
        {
            if (MidiService.CallerHasAdministrativeRight)
            {

                if (MidiService.IsRunning())
                {
                    System.Diagnostics.Debug.WriteLine("Service is running. Attempting to stop.");

                    Assert.IsTrue(MidiService.Stop(), "Could not stop service.");
                }
                else
                {
                    // Service is not running.
                    System.Diagnostics.Debug.WriteLine("Service was already in a stopped state. Stop not tested.");
                }

                if (MidiService.IsRunning())
                {
                    Assert.Fail("Service is still running.");
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Service is stopped. Attempting to start.");
                    Assert.IsTrue(MidiService.Start(), "Could not start service.");
                }
            }
            else
            {
                Assert.Inconclusive("Cannot run test under a normal account. Needs admin rights.");
            }


        }






    }
}