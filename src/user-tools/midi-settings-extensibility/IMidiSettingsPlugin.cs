namespace Microsoft.Midi.Settings.Extensibility
{
    public interface IMidiSettingsPlugin
    {
        public string Name { get; }
        public string Description { get; }
        public string Version { get; }
        public string Author { get; }


    }
}