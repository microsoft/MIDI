using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.ViewModels;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services;

public class MidiSettingsSearchService : IMidiSettingsSearchService
{
    private readonly IMidiEndpointEnumerationService _endpointEnumerationService;
    private readonly IMidiTransportInfoService _transportInfoService;

    private readonly IPageService _pageService;

    private List<MidiSettingsSearchResult> AllItems = [];


    private const string TransportGlyph = "\uE7C0";
    private const string EndpointGlyph = "\uE8D6";
    private const string SettingsPageGlyph = "\uE713";
    private const string SourcePortGlyph = "\uE896";
    private const string DestinationPortGlyph = "\uE898";

    public void Refresh()
    {
        // build the initial search list
        AllItems.Clear();

        // add all pages. Target is typeof(viewmodel).FullName

        foreach (var t in _pageService.GetAllSearchEnabledViewModels())
        {
            var result = new MidiSettingsSearchResult();

            var ifType = t.GetInterface("ISettingsSearchTarget");

            if (ifType == null)
                continue;

            //var keywordsMethod = ifType.GetMethod("GetSearchKeywords", BindingFlags.Static | BindingFlags.Public);
            var keywordsMethod = t.GetMethod("GetSearchKeywords", BindingFlags.Static | BindingFlags.Public);
            IList<string> keywords = (IList<string>)keywordsMethod?.Invoke(null, null)!;

            //var pageTitleMethod = ifType.GetMethod("GetSearchPageTitle", BindingFlags.Static | BindingFlags.Public);
            var pageTitleMethod = t.GetMethod("GetSearchPageTitle", BindingFlags.Static | BindingFlags.Public);
            var pageTitle = (string)pageTitleMethod?.Invoke(null, null)!;

            result.Glyph = SettingsPageGlyph;
            result.Keywords.AddRange(keywords);
            result.DestinationKey = t.FullName!;
            result.DisplayText = pageTitle;

            result.ResultType = "Settings Page";

            AllItems.Add(result);
        }

        // add all endpoints and their various names

        foreach (var endpoint in _endpointEnumerationService.GetEndpoints())
        {
            var result = new MidiSettingsSearchResult();

            result.Glyph = EndpointGlyph;
            result.DestinationKey = typeof(DeviceDetailViewModel).FullName!;
            result.DisplayText = endpoint.Name;
            result.Parameter = endpoint;

            if (endpoint.TransportCode.ToLower().StartsWith("ks"))
            {
                result.AddKeyword("usb driver");
            }

            result.AddKeyword(endpoint.Description);
            result.AddKeyword(endpoint.TransportCode);
            result.AddKeyword(endpoint.ManufacturerName);

            var parentName = endpoint.DeviceInformation.GetParentDeviceInformation().Name.Trim().ToLower();
            result.AddKeyword(parentName);

            foreach (var name in endpoint.DeviceInformation.GetNameTable())
            {
                result.AddKeyword(name.PinName);
                result.AddKeyword(name.CustomName);
                result.AddKeyword(name.BlockName);
            }

            foreach (var fb in endpoint.DeviceInformation.GetDeclaredFunctionBlocks())
            {
                result.AddKeyword(fb.Name);
            }

            foreach (var gtb in endpoint.DeviceInformation.GetGroupTerminalBlocks())
            {
                result.AddKeyword(gtb.Name);
            }



            // MIDI 1 ports have their own entries

            foreach (var sourcePort in endpoint.Midi1InputPorts)
            {
                var port = new MidiSettingsSearchResult();

                port.Glyph = SourcePortGlyph;
                port.DestinationKey = typeof(DeviceDetailViewModel).FullName!;
                port.DisplayText = sourcePort.PortName + " on " + endpoint.Name;
                port.ResultType = "MIDI 1.0 Source/Input Port";
                port.Parameter = endpoint;

                port.Keywords.AddRange(result.Keywords);
                port.AddKeyword(sourcePort.PortName);
                port.AddKeyword("winmm");
                port.AddKeyword("mma");

                AllItems.Add(port);
            }

            foreach (var destinationPort in endpoint.Midi1OutputPorts)
            {
                var port = new MidiSettingsSearchResult();

                port.Glyph = DestinationPortGlyph;
                port.DestinationKey = typeof(DeviceDetailViewModel).FullName!;
                port.DisplayText = destinationPort.PortName + " on " + endpoint.Name;
                port.ResultType = "MIDI 1.0 Destination/Output Port";
                port.Parameter = endpoint;

                port.Keywords.AddRange(result.Keywords);
                port.AddKeyword(destinationPort.PortName);
                port.AddKeyword("winmm");
                port.AddKeyword("mma");

                AllItems.Add(port);
            }


            // endpoint keywords we don't want to add to the child MIDI 1.0 API ports
            if (endpoint.SupportsMidi2)
            {
                result.ResultType = "MIDI 2.0 UMP Endpoint";
                result.AddKeyword("midi 2.0");
            }
            else
            {
                result.ResultType = "UMP Endpoint";
            }
            result.AddKeyword("ump");
            AllItems.Add(result);


        }


        // add all transports

        foreach (var transport in _transportInfoService.GetAllTransports())
        {
            var result = new MidiSettingsSearchResult();

            result.Glyph = TransportGlyph;
            result.DestinationKey = typeof(PluginsTransportViewModel).FullName!;
            result.DisplayText = transport.Name;
            result.ResultType = "Transport";
            result.Keywords.Add(transport.TransportCode.ToLower());
            result.Keywords.Add(transport.Description.ToLower());

            AllItems.Add(result);
        }


        // maybe add all current session names


    }

    public MidiSettingsSearchService(
        IMidiEndpointEnumerationService endpointEnumerationService,
        IMidiTransportInfoService transportInfoService,
        IPageService pageService)
    {
        _endpointEnumerationService = endpointEnumerationService;
        _transportInfoService = transportInfoService;
        _pageService = pageService;
    }

    public IList<MidiSettingsSearchResult> GetFilteredResults(string filterText)
    {
        IList<MidiSettingsSearchResult> results = [];

        var cleanedSearchtext = filterText.ToLower().Trim();

        foreach (var result in AllItems)
        {
            if (result.DisplayText.Contains(cleanedSearchtext))
            {
                results.Add(result);
            }
            else
            {
                foreach (var keyword in result.Keywords)
                {
                    if (keyword.Contains(cleanedSearchtext))
                    {
                        results.Add(result);
                        break;
                    }
                }
            }
        }

        return results.OrderBy(r=>r.DisplayText).ToList();
    }
}
