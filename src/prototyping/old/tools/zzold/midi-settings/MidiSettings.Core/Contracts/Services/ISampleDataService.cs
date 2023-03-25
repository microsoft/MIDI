using System.Collections.Generic;
using System.Threading.Tasks;

using MidiSettings.Core.Models;

namespace MidiSettings.Core.Contracts.Services;

// Remove this class once your pages/features are using your data.
public interface ISampleDataService
{
    Task<IEnumerable<SampleMidiDevice>> GetDeviceListDetailsDataAsync();
}
