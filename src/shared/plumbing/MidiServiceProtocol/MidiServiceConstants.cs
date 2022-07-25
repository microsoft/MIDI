using System.Diagnostics;
using System.Text.Json;

namespace MidiService.Protocol
{
    public class MidiServiceConstants
    {
        private const string PipePrefix = "midi.";

        // May want to make this something which can be set in the config file

        public const string InitialConnectionPipeName = PipePrefix + "Connect";
        public const string EnumerationServicePipePrefix = PipePrefix + "Enumerator";

        public const string SessionServicePipePrefix = PipePrefix + "Session";  // this gets an ID added to it




    }
}