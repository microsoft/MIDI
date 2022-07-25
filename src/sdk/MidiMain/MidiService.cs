using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Windows.Midi
{
    public sealed class MidiService
    {

        public static Version? ApiVersion
        {
            get { return Assembly.GetExecutingAssembly().GetName().Version; }
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
        /// </summary>
        /// <returns>Version of the running service</returns>
        /// <exception cref="NotImplementedException"></exception>
        public static Version PingService()
        {
            // checks to see if the MIDI service is running
            throw new NotImplementedException();
        }


        /// <summary>
        /// Gets the URI for the service installer for the API version currently in use
        /// </summary>
        /// <returns>URL to the installer package</returns>
        /// <exception cref="NotImplementedException"></exception>
        public static Uri GetServicesInstallerUri()
        {
            throw new NotImplementedException();
        }


        public static void InstallCompatibleMidiService()
        {

        }


        // TODO: Maybe a function to get a collection of versions for all
        // the essential drivers and transport plugins?

    }
}
