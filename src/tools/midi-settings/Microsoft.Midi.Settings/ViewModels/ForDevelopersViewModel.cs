using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Helpers;

namespace Microsoft.Midi.Settings.ViewModels;

public class ForDevelopersViewModel : ObservableRecipient
{

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;


    public ForDevelopersViewModel()
    {
    }



}
