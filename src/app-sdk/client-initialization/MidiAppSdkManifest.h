// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#define MIDI_SDK_ROOT_NAMESPACE     L"Microsoft.Windows.Devices.Midi2"
#define MIDI_SDK_ROOT_NAMESPACE_LENGTH 31


//inline std::wstring GetMidiSdkMetadataFullFileName()
//{
//    auto path = GetMidiSdkPath();
//
//    if (path.has_value())
//    {
//        return path.value() + L"\\" + MIDI_SDK_METADATA_NAME;
//    }
//    else
//    {
//        return L"";
//    }
//}
//
//inline std::wstring GetMidiSdkImplementationFullFileName()
//{
//    auto path = GetMidiSdkPath();
//
//    if (path.has_value())
//    {
//        return path.value() + L"\\" + MIDI_SDK_IMPL_DLL_NAME;
//    }
//    else
//    {
//        return L"";
//    }
//}


