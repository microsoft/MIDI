// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Windows.Devices.Midi2.Utilities.RuntimeInformation;
using Microsoft.Windows.Devices.Midi2.Utilities.Update;
using System.Diagnostics;
using System.Runtime.InteropServices;

namespace Microsoft.Midi.Settings.Services;


public class MidiUpdateService : IMidiUpdateService
{
    // https://aka.ms/MidiServicesLatestSdkVersionJson

    private readonly IMidiSdkService _sdkService;
    private readonly ILocalSettingsService _localSettingsService;
    private readonly IMidiServiceRegistrySettingsService _registrySettingsService;
    private readonly IMidiSessionService _sessionService;

    private const string AutoCheckSettingsKey = "AutoCheckForUpdates";


    public MidiRuntimeReleaseTypes GetCurrentInstalledChannel()
    {
        // TODO: If the current in-use release is a preview, then put the user on the preview channel.


        return MidiRuntimeReleaseTypes.Preview;
    }

    public MidiRuntimeReleaseTypes GetCurrentPreferredChannel()
    {
        return _registrySettingsService.GetPreferredSdkRuntimeReleaseType(GetCurrentInstalledChannel());
    }

    public void SetCurrentPreferredChannel(MidiRuntimeReleaseTypes preferredReleaseType)
    {
        _registrySettingsService.SetPreferredSdkRuntimeReleaseType(preferredReleaseType);
    }


    public void SetAutoCheckForUpdatesEnabled(bool newValue)
    {
        _registrySettingsService.SetAutoCheckForUpdatesEnabled(newValue);
    }


    public bool GetAutoCheckForUpdatesEnabled()
    {
        return _registrySettingsService.GetAutoCheckForUpdatesEnabled();
    }





    public IReadOnlyList<MidiRuntimeRelease> GetAllAvailableRuntimeDownloads()
    {
        return MidiRuntimeUpdateUtility.GetAllAvailableReleases();
    }

    public MidiRuntimeRelease? GetHighestAvailableRuntimeRelease(MidiRuntimeReleaseTypes releaseChannelType)
    {

        return MidiRuntimeUpdateUtility.GetHighestAvailableRelease(releaseChannelType);
    }

    public MidiRuntimeRelease? CheckForUpdates()
    {
        return GetHighestAvailableRuntimeRelease(GetCurrentPreferredChannel());
    }



    // this is the general installer page, not the global page. Normally, this is not used from
    // within the settings app, because it's there to install the runtime, including the settings app.
    public Uri GetMidiSdkInstallerPageUri()
    {
        return new Uri(MidiDesktopAppSdkInitializer.LatestMidiAppSdkDownloadUrl);
    }


    [DllImport("shell32.dll", CharSet = CharSet.Unicode)]
    private static extern int SHGetKnownFolderPath([MarshalAs(UnmanagedType.LPStruct)] Guid rfid, uint dwFlags, IntPtr hToken, out string pszPath);
    private static readonly Guid FOLDERID_Downloads = new Guid("374DE290-123F-4565-9164-39C4925E467B");

    public async Task<bool> DownloadAndInstallUpdate(Uri uri)
    {
        // validate internet access
        // download the update (this needs to follow redirects due to how github hosting works)
        // launch the installer
        // close this app

        try
        {

            var handler = new HttpClientHandler()
            {
                AllowAutoRedirect = true,
                MaxAutomaticRedirections = 5
            };

            var client = new HttpClient(handler);

            //string tempFileName = Path.GetTempFileName();


            using (var response = await client.GetAsync(uri))
            {
                if (!response.IsSuccessStatusCode)
                {
                    return false;
                }

                string installerFileName = string.Empty;
                if (response.Content.Headers.ContentDisposition != null && 
                    response.Content.Headers.ContentDisposition.FileName != null)
                {
                    installerFileName = response.Content.Headers.ContentDisposition.FileName;
                }

                string downloadsFolder;
                SHGetKnownFolderPath(FOLDERID_Downloads, 0, IntPtr.Zero, out downloadsFolder);

                string installerFullPath = string.Empty;

                if (installerFileName == null || 
                    installerFileName == string.Empty || 
                    downloadsFolder == null || 
                    downloadsFolder == string.Empty)
                {
                    installerFullPath = Path.GetTempFileName();
                }
                else
                {
                    installerFullPath = Path.Combine(downloadsFolder, installerFileName);
                }


                if (installerFileName != null)
                {
                    FileStreamOptions options = new FileStreamOptions()
                    {
                        Mode = FileMode.Create,
                        Access = FileAccess.Write
                    };

                    using (var installerFileStream = new FileStream(installerFullPath, options))
                    {
                        await response.Content.CopyToAsync(installerFileStream);
                    }

                    System.Diagnostics.Debug.WriteLine("File downloaded to: " + installerFullPath);

                    var process = new Process();
                    process.StartInfo.FileName = installerFullPath;
                    process.StartInfo.UseShellExecute = true;
                    process.StartInfo.LoadUserProfile = true;
                    process.Start();

                    // close MIDI session if open
                    if (_sessionService.Session != null)
                    {
                        _sessionService.Session.Dispose();
                    }

                    Process.GetCurrentProcess().Kill();

                    // never gets called
                    return true;
                }
            }
        }
        catch (Exception ex)
        {
            App.GetService<ILoggingService>().LogError("Error downloading and installing update.", ex);

            System.Diagnostics.Debug.WriteLine("Exception downloading file: " + ex.ToString());

            return false;
        }

        // upon a false return, the calling code should pop up a browser window with the general release page
        return false;
    }


    public bool IsAutoCheckForUpdatesEnabled
    {
        get
        {
            return _localSettingsService.ReadSettingAsync<bool>(AutoCheckSettingsKey, true).GetAwaiter().GetResult();
        }
        set
        {
            _localSettingsService.SaveSettingAsync<bool>(AutoCheckSettingsKey, value).GetAwaiter().GetResult();
        }
    }



    public MidiUpdateService(
        IMidiSdkService sdkService, 
        IMidiSessionService sessionService,
        ILocalSettingsService localSettingsService,
        IMidiServiceRegistrySettingsService registrySettingsService)
    {
        _sessionService = sessionService;
        _sdkService = sdkService;
        _localSettingsService = localSettingsService;
        _registrySettingsService = registrySettingsService;
    }


}
