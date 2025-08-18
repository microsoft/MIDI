// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;


namespace Microsoft.Midi.Settings.Services;

class MidiTransportInfoService : IMidiTransportInfoService
{
    private IList<MidiServiceTransportPluginInfo>? _transports = null;
    public MidiServiceTransportPluginInfo GetTransportForCode(string transportCode)
    {
        if (_transports == null)
        {
            _transports = MidiReporting.GetInstalledTransportPlugins();
        }

        return _transports.Where(p => p.TransportCode.ToUpper() == transportCode.ToUpper()).FirstOrDefault<MidiServiceTransportPluginInfo>();
    }


    public MidiServiceTransportPluginInfo GetTransportForId(Guid transportId)
    {
        if (_transports == null)
        {
            _transports = MidiReporting.GetInstalledTransportPlugins();
        }

        return _transports.Where(p => p.Id == transportId).FirstOrDefault<MidiServiceTransportPluginInfo>();
    }


    public IList<MidiServiceTransportPluginInfo> GetAllTransports()
    {
        return _transports.OrderBy(t=>t.TransportCode).ToList();
    }


}
