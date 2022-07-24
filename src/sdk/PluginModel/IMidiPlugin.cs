namespace Microsoft.Windows.Midi.PluginModel
{

    public enum MidiPluginType
    {
        MessageProcessor = 100,
        Transport = 200,
    }

    public interface IMidiPlugin
    {
        MidiPluginType PluginType { get; }

        string PluginAuthor { get; }

        string PluginIconFileName { get; }

        string PluginDescription { get; }

        //string TypeName { get; }

        Version PluginVersion { get; }

    }
}