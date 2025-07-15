// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Extensions.Options;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Models;
using Microsoft.Windows.Devices.Midi2.Common;
using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2.Utilities.Update;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{

    public class MidiUpdateService : IMidiUpdateService
    {
        // https://aka.ms/MidiServicesLatestSdkVersionJson


        public MidiRuntimeUpdateReleaseTypes GetCurrentInstalledChannel()
        {
            // TODO: If the current in-use release is a preview, then put the user on the preview channel.

            //MidiNuGetBuildInformation.

            return MidiRuntimeUpdateReleaseTypes.Preview;
        }

        public MidiRuntimeUpdateReleaseTypes GetCurrentPreferredChannel()
        {
            // TODO: read from registry.

            return MidiRuntimeUpdateReleaseTypes.Stable;
        }


        public IReadOnlyList<MidiRuntimeRelease> GetAllAvailableRuntimeDownloads()
        {
            return MidiRuntimeUpdateUtility.GetAllAvailableReleases();
        }

        public MidiRuntimeRelease? GetHighestAvailableRuntimeRelease(MidiRuntimeUpdateReleaseTypes releaseChannelType)
        {

            // TODO


            return MidiRuntimeUpdateUtility.GetHighestAvailableRelease(MidiRuntimeUpdateReleaseTypes.Stable);
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

                    string installerFileName = response.Content.Headers.ContentDisposition.FileName;

                    string downloadsFolder;
                    SHGetKnownFolderPath(FOLDERID_Downloads, 0, IntPtr.Zero, out downloadsFolder);

                    string installerFullPath = string.Empty;

                    if (installerFileName == null || installerFileName == string.Empty || downloadsFolder == null || downloadsFolder == string.Empty)
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

                        // TODO: Should we shut down midisrv connection first, just to be nice?
                        // close MIDI session

                        Process.GetCurrentProcess().Kill();

                        // never gets called
                        return true;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.Debug.WriteLine("Exception downloading file: " + ex.ToString());

                return false;
            }

            // upon a false return, the calling code should pop up a browser window with the general release page
            return false;
        }
    }
}
