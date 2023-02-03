using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.ViewModels;

namespace Microsoft.Midi.Settings.ViewModels;

public class ForDevelopersViewModel : ObservableRecipient
{
    bool _isDeveloperModeEnabled = false;

    public ForDevelopersViewModel()
    {
        CheckDeveloperModeRegistryKey();
    }

    public bool IsDeveloperModeEnabled
    {
        get => _isDeveloperModeEnabled;
        set => SetProperty(ref _isDeveloperModeEnabled, value);
    }

    private void CheckDeveloperModeRegistryKey()
    {
        const string keyName = @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock";
        const string valueName = "AllowDevelopmentWithoutDevLicense";

        var value = Microsoft.Win32.Registry.GetValue(keyName, valueName, 0);

        if (value == null || (int)value != 1)
        {
            IsDeveloperModeEnabled = false;
        }
        else        
        {
            IsDeveloperModeEnabled = true;
        }
    }

    // TODO: implement reg watcher for the dev mode value so user doesn't need to close app to refresh
    // http://www.pinvoke.net/default.aspx/advapi32.regnotifychangekeyvalue
    // https://stackoverflow.com/questions/41231586/how-to-detect-if-developer-mode-is-active-on-windows-10


}
