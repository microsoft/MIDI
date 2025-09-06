// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using CommunityToolkit.WinUI.Converters;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Helpers;
using Microsoft.UI.Xaml.Media;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Claims;
using System.Windows.Input;
using Windows.Storage;

namespace Microsoft.Midi.Settings.ViewModels;

public partial class ForDevelopersViewModel : ObservableRecipient //, ISettingsSearchTarget
{
    public static IList<string> GetSearchKeywords()
    {
        // TODO: these need to be localized, so should refer to resources instead
        return new[] { "developers", "advanced", "discovery timeout" };
    }

    public static string GetSearchPageTitle()
    {
        return "Developer Settings";
    }

    public static string GetSearchPageDescription()
    {
        return "MIDI configuration options for developers testing their own apps, devices, transports, and message processing plugins.";
    }


    IMidiServiceRegistrySettingsService m_registrySettingsService;

    public bool IsDeveloperModeEnabled => WindowsDeveloperModeHelper.IsDeveloperModeEnabled;

    public bool IsRunningAsAdministrator => UserHelper.CurrentUserHasAdminRights();
    //public bool IsRunningAsAdministrator => true;

    [ObservableProperty]
    public bool enableDiscoveryAndProtocolNegotiation;

    [ObservableProperty]
    public bool enableMmcss;

    [ObservableProperty]
    public bool serviceRestartRequired;

    [ObservableProperty]
    public bool rebootRequired;


    private UInt32 m_currentlySavedDiscoveryTimeoutValue;

    [ObservableProperty]
    public UInt32 discoveryTimeout;

    public UInt32 DiscoveryTimeoutMaximum => 50000;    // TODO: This needs to stay in sync with MidiDefs.h
    public UInt32 DiscoveryTimeoutMinimum => 500;      // TODO: This needs to stay in sync with MidiDefs.h

    [ObservableProperty]
    public bool discoveryTimeoutHasChanged;


    partial void OnDiscoveryTimeoutChanged(UInt32 value)
    {
        if (value != m_currentlySavedDiscoveryTimeoutValue)
        {
            DiscoveryTimeoutHasChanged = true;
        }
        else
        {
            DiscoveryTimeoutHasChanged = false;
        }
    }

    partial void OnEnableDiscoveryAndProtocolNegotiationChanged(bool value)
    {
        // todo: should check against original values
        ServiceRestartRequired = true;
    }

    partial void OnEnableMmcssChanged(bool value)
    {
        // todo: should check against original values
        ServiceRestartRequired = true;
    }


    public ICommand RestartServiceCommand { get; private set; }

    public ICommand CommitDiscoveryTimeoutCommand { get; private set; }

    public ICommand ShowMidiKernelStreamingDevicesCommand { get; private set; }

    public ICommand RebootCommand { get; private set; }


    enum RegLocation
    {
        HKCR,
        HKLM
    }

    private bool GrantAdministratorFullRegKeyPermissions(RegLocation location, string key, uint recurseLevel = 0)
    {
        // call RtlAdjustPrivilege to grant administrator rights
        // SeTakeOwnership, SeBackup, and SeRestore
        // recursive

        var sid = new System.Security.Principal.SecurityIdentifier("S-1-5-32-545");

        Microsoft.Win32.RegistryKey rootKey;

        switch (location)
        {
            case RegLocation.HKCR:
                rootKey = Microsoft.Win32.Registry.ClassesRoot;
                break;
            case RegLocation.HKLM:
                rootKey = Microsoft.Win32.Registry.LocalMachine;
                break;
            default:
                return false;
        }

        var regKey = rootKey.OpenSubKey(
            key,
            Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
            System.Security.AccessControl.RegistryRights.TakeOwnership);

        if (regKey != null)
        {
            var acl = new System.Security.AccessControl.RegistrySecurity();
            acl.SetOwner(sid);

            regKey.SetAccessControl(acl);

            acl.SetAccessRuleProtection(false, false);
            regKey.SetAccessControl(acl);

            // only for top level keys, change permission for key and prop to subkeys
            if (recurseLevel == 0)
            {
                regKey = regKey.OpenSubKey(
                    "",
                    Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
                    System.Security.AccessControl.RegistryRights.TakeOwnership);

                var rule = new System.Security.AccessControl.RegistryAccessRule(
                    sid,
                    System.Security.AccessControl.RegistryRights.FullControl,
                    System.Security.AccessControl.InheritanceFlags.ContainerInherit,
                    System.Security.AccessControl.PropagationFlags.None,
                    System.Security.AccessControl.AccessControlType.Allow);

                acl.ResetAccessRule(rule);

                if (regKey != null)
                {
                    regKey.SetAccessControl(acl);

                    var regkey2 = regKey.OpenSubKey("");

                    if (regkey2 != null)
                    {
                        // recurse
                        foreach (var subKey in regkey2.GetSubKeyNames())
                        {
                            GrantAdministratorFullRegKeyPermissions(location, key + "\\" + subKey, recurseLevel + 1);
                        }
                    }
                }
                else
                {
                    return false;
                }
            }
        }
        else
        {
            return false;
        }

        return true;
    }

    [DllImport("ntdll.dll", SetLastError = true)]
    private static extern int RtlAdjustPrivilege(
           uint Privilege,
           bool Enable,
           bool CurrentThread,
           out bool Enabled
        );

    public bool PrepareForDeveloperInstall()
    {
        string[] hklmRegKeysToOwn =
        {
            @"SOFTWARE\Microsoft\Windows MIDI Services\Transport Plugins",
            @"SOFTWARE\Microsoft\Windows MIDI Services\Endpoint Processing Plugins",
        };

        string[] hkcrRegKeysToOwn =
        {
            @"CLSID\{0f273b18-e372-4d95-87ac-c31c3d22e937}",  // KS Aggregate Transport
            @"CLSID\{26FA740D-469C-4D33-BEB1-3885DE7D6DF1}",  // KS Transport
            @"CLSID\{942BF02D-93C0-4EA8-B03E-D51156CA75E1}",  // Loopback Transport
            @"CLSID\{8FEAAD91-70E1-4A19-997A-377720A719C1}",  // Virtual MIDI Transport
            @"CLSID\{ac9b5417-3fe0-4e62-960f-034ee4235a1a}",  // Diagnostics Transport
            @"CLSID\{2BA15E4E-5417-4A66-85B8-2B2260EFBC84}",  // Main Midisrv Transport

            @"CLSID\{A8798C54-6066-45F0-9ADB-648BC0641ABF}",  // Bytestream 2 UMP Transform
            @"CLSID\{96121591-8D68-479F-9B48-2BF0B90113F7}",  // UMP 2 Bytestream Transform
            @"CLSID\{a42cde44-7fa9-4597-a8ee-b40b96bcddb1}",  // Message Scheduler
            @"CLSID\{dc638b31-cf31-48ed-9e79-02740bf5d013}",  // Protocol Downscaler
        };

        const uint SeTakeOwnership = 9;
        const uint SeBackup = 17;
        const uint SeRestore = 18;

        bool enabled;
        RtlAdjustPrivilege(SeTakeOwnership, true, false, out enabled);
        RtlAdjustPrivilege(SeBackup, true, false, out enabled);
        RtlAdjustPrivilege(SeRestore, true, false, out enabled);


        foreach (var regkey in hkcrRegKeysToOwn)
        {
            GrantAdministratorFullRegKeyPermissions(RegLocation.HKCR, regkey);
        }

        foreach (var regkey in hklmRegKeysToOwn)
        {
            GrantAdministratorFullRegKeyPermissions(RegLocation.HKLM, regkey);
        }

        // stop midisrv if present
        var controller = MidiServiceHelper.GetServiceController();

        if (controller != null)
        {
            if (MidiServiceHelper.StopService(controller))
            {
                // remove midisrv, if present
                MidiServiceHelper.RemoveService(controller);
            }
        }

        return true;
    }


    private bool TakeOwnershipOfFile(string fullFilePath)
    {
        var takeOwnCommand = "takeown /F " + fullFilePath;

        ProcessStartInfo info = new ProcessStartInfo();
        info.FileName = "cmd.exe";
        info.UseShellExecute = true;
        info.Verb = "runas";
        info.Arguments = $"/c \"{takeOwnCommand}\"";

        var proc = Process.Start(info);
        if (proc != null)
        {
            // TODO: Need to wait for completion and then check return code                   
        }

        return true;
    }

    private bool GrantAdministratorFullFilePermissions(string fullFilePath)
    {
        var command = "icacls " + fullFilePath + " /grant administrators:F";

        ProcessStartInfo info = new ProcessStartInfo();
        info.FileName = "cmd.exe";
        info.UseShellExecute = true;
        info.Verb = "runas";
        info.Arguments = $"/c \"{command}\"";

        var proc = Process.Start(info);
        if (proc != null)
        {
            // TODO: Need to wait for completion and then check return code                   
        }

        return true;
    }


    public bool ReplaceWdmaud2(string fileName)
    {
        string systemDriverPath = Environment.ExpandEnvironmentVariables(@"%systemroot%\System32\wdmaud2.drv");
        string systemDriverBakPath = Environment.ExpandEnvironmentVariables(@"%systemroot%\System32\wdmaud2.drv.bak");

        try
        {
            // stop the MIDI service?

            // check for current wdmaud2.drv. We don't do any backup
            // if there's not a current wdmaud2.drv
            if (File.Exists(systemDriverPath))
            {
                // take ownership of wdmaud2.drv
                if (!TakeOwnershipOfFile(systemDriverPath))
                {
                    // todo: set an error that the UI can show
                    return false;
                }

                // give permissions to admin group
                if (!GrantAdministratorFullFilePermissions(systemDriverPath))
                {
                    // todo: set an error that the UI can show
                    return false;
                }

                // rename current version to .bak
                File.Move(systemDriverPath, systemDriverBakPath, true);
            }

            // copy over the new one
            File.Copy(fileName, systemDriverPath);

            // user needs to reboot
            RebootRequired = true;

            return true;
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error replacing wdmaud2.drv", ex);

            // todo: set an error the UI can show
            return false;
        }


    }


    [DllImport("user32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool ExitWindowsEx(
           uint uFlags,
           uint dwReason
        );

    private const UInt32 EWX_REBOOT = 0x00000002;
    private const UInt32 SHTDN_REASON_MAJOR_APPLICATION = 0x00040000;
    private const UInt32 SHTDN_REASON_MINOR_MAINTENANCE = 0x00000001;
    private const UInt32 SHTDN_REASON_FLAG_PLANNED = 0x80000000;

    private const UInt32 ThisRebootReason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED;

    public ForDevelopersViewModel(IMidiServiceRegistrySettingsService registrySettingsService)
    {
        m_registrySettingsService = registrySettingsService;

        // TODO read these values from the registry

        m_currentlySavedDiscoveryTimeoutValue = m_registrySettingsService.GetMidi2DiscoveryTimeoutMS();

        DiscoveryTimeout = m_currentlySavedDiscoveryTimeoutValue;

        EnableMmcss = m_registrySettingsService.GetUseMMCSS();
        EnableDiscoveryAndProtocolNegotiation = m_registrySettingsService.GetMidi2DiscoveryEnabled();

        ServiceRestartRequired = false;


        RestartServiceCommand = new RelayCommand(() =>
        {
            // TODO: Call into service service to restart


        });

        CommitDiscoveryTimeoutCommand = new RelayCommand(() =>
        {
            if (m_registrySettingsService.SetMidi2DiscoveryTimeoutMS(DiscoveryTimeout))
            {
                ServiceRestartRequired = true;
                m_currentlySavedDiscoveryTimeoutValue = DiscoveryTimeout;
            }

        });

        ShowMidiKernelStreamingDevicesCommand = new RelayCommand(() =>
        {
            

            
        });

        RebootCommand = new RelayCommand(() =>
        {
            ExitWindowsEx(EWX_REBOOT, ThisRebootReason);
        });

    }



}
