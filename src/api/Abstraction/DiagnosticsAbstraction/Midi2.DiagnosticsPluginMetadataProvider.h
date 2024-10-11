// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once


class CMidi2DiagnosticsPluginMetadataProvider :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiServiceAbstractionPluginMetadataProvider>

{
public:
    STDMETHOD(Initialize());
    STDMETHOD(GetMetadata(_Out_ PABSTRACTIONMETADATA metadata));
    STDMETHOD(Shutdown)();

private:

    GUID m_abstractionId;   // kept for convenience
};

