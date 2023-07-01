// Copyright(c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once
#include "MidiTransportInformation.g.h"


namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiTransportInformation : MidiTransportInformationT<MidiTransportInformation>
    {
        MidiTransportInformation() = default;

        hstring Id();
        hstring Name();
        hstring ShortName();
        hstring IconPath();
        hstring Author();
        hstring ServicePluginFileName();
        bool IsRuntimeCreatable();


        // constructor only on the implementation type. Not part of WinRT. Used inside the SDK only.
        MidiTransportInformation(
            hstring id,
            hstring name,
            hstring shortName,
            hstring iconPath,
            hstring author,
            hstring servicePluginFileName,
            bool isRuntimeCreatable
        );


    private:
        hstring _id;
        hstring _name;
        hstring _shortName;
        hstring _iconPath;
        hstring _author;
        hstring _servicePluginFileName;
        bool _isRuntimeCreatable;
    };
}
