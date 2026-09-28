// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Describes the transport to the service and the settings app.
// ============================================================================

#pragma once

class CMidi2RtpMidiPluginMetadataProvider :
    public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
        IMidiServiceTransportPluginMetadataProvider>
{
public:
    STDMETHOD(Initialize)();
    STDMETHOD(GetMetadata)(_Out_ PTRANSPORTMETADATA metadata);
    STDMETHOD(Shutdown)();
};
