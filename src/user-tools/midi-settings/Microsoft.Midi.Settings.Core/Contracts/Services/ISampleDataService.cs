using Microsoft.Midi.Settings.Core.Models;

namespace Microsoft.Midi.Settings.Core.Contracts.Services;

// Remove this class once your pages/features are using your data.
public interface ISampleDataService
{
    Task<IEnumerable<MidiMessageViewModel>> GetUmpMonitorDataAsync();
    Task<IEnumerable<string>> GetUmpEndpointNamesAsync();
    Task<IEnumerable<DisplayFriendlyMidiDevice>> GetDevicesAsync();


}
