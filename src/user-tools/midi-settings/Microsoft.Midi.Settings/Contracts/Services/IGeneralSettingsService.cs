using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Microsoft.UI.Xaml;

namespace Microsoft.Midi.Settings.Contracts.Services;

public interface IGeneralSettingsService
{
    bool ShowDeveloperOptions
    {
        get; set;
    }

    Task InitializeAsync();

    event EventHandler SettingsChanged;
}
