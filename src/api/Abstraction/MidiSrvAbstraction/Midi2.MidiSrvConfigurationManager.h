// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidi2MidiSrvConfigurationManager :
    public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMidiAbstractionConfigurationManager>
{
public:
    STDMETHOD(Initialize(_In_ GUID abstractionGuid, _In_ IUnknown* deviceManagerInterface));
    STDMETHOD(UpdateConfiguration(_In_ LPCWSTR configurationJson, _Out_ BSTR* response));
    STDMETHOD(Cleanup)();

private:
    std::unique_ptr<CMidi2MidiSrv> m_MidiSrv;

    GUID m_abstractionGuid;
};


