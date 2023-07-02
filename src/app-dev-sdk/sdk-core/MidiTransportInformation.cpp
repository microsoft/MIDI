// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiTransportInformation.h"
#include "MidiTransportInformation.g.cpp"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    // constructor for use inside the SDK
    MidiTransportInformation::MidiTransportInformation(
        hstring id,
        hstring name,
        hstring shortName,
        hstring iconPath,
        hstring author,
        hstring servicePluginFileName,
        bool isRuntimeCreatable
    )
    {
        _id = id;
        _name = name;
        _shortName = shortName;
        _iconPath = iconPath;
        _author = author;
        _servicePluginFileName = servicePluginFileName;
        _isRuntimeCreatable = isRuntimeCreatable;
    }


    hstring MidiTransportInformation::Id()
    {
        return _id;
    }
    hstring MidiTransportInformation::Name()
    {
        return _name;
    }
    hstring MidiTransportInformation::ShortName()
    {
        return _shortName;
    }
    hstring MidiTransportInformation::IconPath()
    {
        return _iconPath;
    }
    hstring MidiTransportInformation::Author()
    {
        return _author;
    }
    hstring MidiTransportInformation::ServicePluginFileName()
    {
        return _servicePluginFileName;
    }
    bool MidiTransportInformation::IsRuntimeCreatable()
    {
        return _isRuntimeCreatable;
    }
}
