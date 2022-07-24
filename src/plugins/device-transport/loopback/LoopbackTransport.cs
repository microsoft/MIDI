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



        // TODO: These need to use a type that C++ plugin devs will ahve access
        // to, like am IBuffer or something. .NET Streams aren't likely the
        // best choice here. Also need to eval how the USB driver will work before
        // settling on this interface. Don't want to use C++/CLI, but WinRT types
        // and other COM-known types are an option.

        ///// <summary>
        ///// MIDI Out (from apps, to device)
        ///// </summary>
        //public void WriteDataFromStream(Stream sourceStream, Guid endpointId)
        //{
        //    throw new NotImplementedException();
        //}

        ///// <summary>
        ///// MIDI In (from device, to apps)
        ///// </summary>
        //public void ReadDataFromStream(Stream sourceStream, Guid endpointId)
        //{
        //    throw new NotImplementedException();
        //}


        // TODO: Test performance and move to one of a number of shared memory approaches
        // if performance here isn't what it needs to be
        public void WriteToDevice(Guid endpointId, IEnumerable<Ump> messageBuffer)
        {
            throw new NotImplementedException();
        }

        public void ReadFromDevice(Guid endpointId, IEnumerable<Ump> messageBuffer)
        {
            throw new NotImplementedException();
        }


    }
}