using Microsoft.Windows.Midi.PluginModel;

namespace LoopbackPlugin
{
    public class LoopbackTransport : IMidiPlugin, IMidiTransport
    {
        public MidiPluginType PluginType => throw new NotImplementedException();

        public string PluginAuthor => throw new NotImplementedException();

        public string PluginIconFileName => throw new NotImplementedException();

        public string PluginDescription => throw new NotImplementedException();

        public Version PluginVersion => throw new NotImplementedException();

        public Guid Id { get => throw new NotImplementedException(); set => throw new NotImplementedException(); }

        public string ShortName => throw new NotImplementedException();

        public string VerboseName => throw new NotImplementedException();

        public string IconFileName => throw new NotImplementedException();

        public string Description => throw new NotImplementedException();





        public void WriteDataFromStream(Stream sourceStream, Guid endpointId)
        {
            throw new NotImplementedException();
        }



    }
}