// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Messages.Management;
using Microsoft.Windows.Midi.Internal.ServiceProtocol.Serialization;

using Microsoft.Windows.Midi.Internal.ServiceControl;
using System.Security.Principal;
using System.Security;

namespace Microsoft.Windows.Midi
{

    public enum MidiServicePingResponse
    {
        Pong,
        TimeOut,
        Error
    }

    public sealed class MidiService
    {
        private const int PingTimeoutMilliseconds = 2000;

        public static bool CallerHasAdministrativeRight
        {
            get
            {
                WindowsPrincipal pricipal = new WindowsPrincipal(WindowsIdentity.GetCurrent());
                bool hasAdministrativeRight = pricipal.IsInRole(WindowsBuiltInRole.Administrator);

                return hasAdministrativeRight;
            }
        }

        public static string ApiVersion
        {
            get { return ClientState.ApiVersion.ToString(); }
        }

        /// <summary>
        /// Checks to see if the MIDI service is installed (by checking to see if it
        /// exists in any state in the registered services. If it is not installed, 
        /// the app should provide the download and install URL.
        /// </summary>
        /// <returns>True if the MIDI service itself is installed</returns>
        public static bool IsInstalled()
        {
            return MidiServiceControlManager.DoesServiceExist();
            
        }

        /// <summary>
        /// Checks with Windows to see if the service is running. This is not a ping
        /// to ensure the service hasn't locked up, but just a basic check.
        /// </summary>
        /// <returns>True if the service control manager reports the service as
        /// being in a running state</returns>
        public static bool IsRunning()
        {
            return MidiServiceControlManager.IsServiceRunning();

        }

        /// <summary>
        /// If the MIDI service is installed but running, this will attempt 
        /// to start it. Requires admin rights for the calling user.
        /// </summary>
        /// <returns>True if it succeeds, false if it times out or otherwise fails</returns>
        public static bool Start()
        {
            if (CallerHasAdministrativeRight)
            {
                return MidiServiceControlManager.StartService();
            }
            else
            {
                throw new SecurityException("Starting the service requires administrative rights.");
            }
        }

        /// <summary>
        /// Attempts to stop the MIDI service
        /// </summary>
        /// <returns>True if it succeeds, false if it times out or otherwise fails</returns>
        public static bool Stop()
        {
            if (CallerHasAdministrativeRight)
            {
                return MidiServiceControlManager.StopService();
            }
            else
            {
                throw new SecurityException("Stopping the service requires administrative rights.");
            }
        }


        /// <summary>
        /// Checks to see if the MIDI service is responding. This is doing an 
        /// actual check against a known endpoint on the service. This doesn't mean
        /// a queue isn't blocked or some other issue, but does indicate that the
        /// service itself is responding.
        /// </summary>
        /// <returns>Version of the running service as well as whether or not
        /// the ping was responded to.</returns>
        public static MidiServicePingResponse Ping(out string serverVersionReported)
        {
            // checks to see if the MIDI service is running

            var request = new ServicePingMessage()
            {
                Header = new RequestMessageHeader()
                {
                    ClientId = ClientState.ClientId,
                    ClientRequestId = Guid.NewGuid(),
                    ClientVersion = ApiVersion,
                },
            };

            using (NamedPipeClientStream sessionPipe = new NamedPipeClientStream(
                ".", // local machine
                MidiServiceConstants.PingPipeName,
                PipeDirection.InOut, PipeOptions.None
                ))
            {
                // get the serializer ready
                MidiServiceProtocolSerializer serializer = new MidiServiceProtocolSerializer(sessionPipe);

                try
                {
                    // connect to pipe. This could take a while if pipe is busy
                    sessionPipe.Connect(PingTimeoutMilliseconds);
                }
                catch (InvalidOperationException)
                {
                    // already connected. Weird, but ok
                }
                catch (TimeoutException)
                {
                    // ping timed out
                    serverVersionReported = string.Empty;
                    return MidiServicePingResponse.TimeOut;
                }

                // send request message
                serializer.Serialize(request);

                // wait for response (reading blocks until data arrives)
                ServicePingResponseMessage response = serializer.Deserialize<ServicePingResponseMessage>();

                // TODO: WinRT doesn't allow O
                Version version = Version.Parse(response.Header.ServerVersion);

                if (version != null)
                {
                    serverVersionReported = version.ToString();
                    return MidiServicePingResponse.Pong;
                }
                else
                {
                    serverVersionReported = string.Empty;
                    return MidiServicePingResponse.Error;
                }
            }
        }


        /// <summary>
        /// Gets the URI for the service installer for the API version currently in use
        /// </summary>
        /// <returns>URL to the installer package</returns>
        public static string GetServicesInstallerUri()
        {
            throw new NotImplementedException();
        }


        public static void InstallCompatibleMidiService()
        {
            throw new NotImplementedException();
        }


        //public static Path ConfigurationFileLocation
        //{
        //    get
        //    {
        //        return MidiServicesConfig.Current.ConfigurationFileFullPath;
        //    }
        //}




        // TODO: Maybe a function to get a collection of versions for all
        // the essential drivers and transport plugins?

    }
}
