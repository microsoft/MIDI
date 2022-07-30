using MidiService.Protocol.Messages.Management;
using System;
using System.Collections.Generic;
using System.IO.Pipes;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

using MidiConfig;

namespace Microsoft.Windows.Midi
{
    public sealed class MidiService
    {

        public static string ApiVersion
        {
            get { return ClientState.ApiVersion.ToString(); }
        }

        /// <summary>
        /// Checks to see if the MIDI services are installed. If they are not installed, 
        /// the app should provide the download and install URL
        /// </summary>
        /// <returns>True if the MIDI services are installed</returns>
        /// <exception cref="NotImplementedException"></exception>
        public static bool IsMidiServiceInstalled()
        {
            // does a check to see if services are installed
            throw new NotImplementedException();
        }

        /// <summary>
        /// If the MIDI service is installed but running, this will start it
        /// </summary>
        /// <exception cref="NotImplementedException"></exception>
        public static void StartMidiService()
        {
            throw new NotImplementedException();
        }

        /// <summary>
        /// Checks to see if the MIDI service is running
        /// TODO: ***************** This needs a timeout, of course
        /// </summary>
        /// <returns>Version of the running service</returns>
        public static bool PingService()
        {
            return true;


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
                    sessionPipe.Connect();

                }
                catch (InvalidOperationException)
                {
                    // already connected. Weird, but ok
                }

                // send request message
                serializer.Serialize(request);

                // wait for response (reading blocks until data arrives)
                ServicePingResponseMessage response = serializer.Deserialize<ServicePingResponseMessage>();

                Version version = Version.Parse(response.Header.ServerVersion);

                if (version != null)
                {
                   // serverVersionReported = version.ToString();
                    return true;
                }
                else
                {
                   // serverVersionReported = string.Empty;
                    return false;
                }

            }

        }


        /// <summary>
        /// Gets the URI for the service installer for the API version currently in use
        /// </summary>
        /// <returns>URL to the installer package</returns>
        /// <exception cref="NotImplementedException"></exception>
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
