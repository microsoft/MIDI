// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License

#pragma once

class CMidi2RtpMidiTransportModule : public ATL::CAtlDllModuleT<CMidi2RtpMidiTransportModule>
{
public:
    DECLARE_LIBID(LIBID_Midi2RtpMidiTransportLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_MIDI2RTPMIDITRANSPORT, "{81a635a5-2573-436c-ae92-21a270672417}")
};

extern class CMidi2RtpMidiTransportModule _AtlModule;
