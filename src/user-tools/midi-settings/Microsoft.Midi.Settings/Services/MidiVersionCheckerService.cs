using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{

    public enum AvailableRuntimeDownloadVersionType
    {
        StableRelease,
        Preview
    }

    public struct AvailableRuntimeDownload
    {
        public Uri DownloadUri;

        public AvailableRuntimeDownloadVersionType VersionType;



    }

    public class MidiVersionCheckerService
    {
        // https://aka.ms/MidiServicesLatestSdkVersionJson


        public IList<AvailableRuntimeDownload> GetAvailableRuntimeDownloads()
        {
            var results = new List<AvailableRuntimeDownload>();




            return results;
        }
    }
}
