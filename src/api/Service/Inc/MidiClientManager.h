// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidiClientManager
{
public:

    CMidiClientManager() {}
    ~CMidiClientManager() {}
    
    HRESULT Initialize(_In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager,
                            _In_ std::shared_ptr<CMidiProcessManager>& processManager,
                            _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager,
                            _In_ std::shared_ptr<CMidiSessionTracker>& sessionTracker);

    HRESULT CreateMidiClient(_In_ handle_t,
                                _In_ LPCWSTR,
                                _In_ GUID,
                                _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                                _In_ PMIDISRV_CLIENT);

    HRESULT DestroyMidiClient(_In_ handle_t,
                                _In_ MidiClientHandle);

    HRESULT Cleanup();

private:
    HRESULT GetMidiClient(_In_ handle_t,
                                _In_ LPCWSTR,
                                _In_ GUID,
                                _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                                _In_ PMIDISRV_CLIENT,
                                _In_ wil::unique_handle&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ BOOL);

    HRESULT GetMidiDevice(_In_ handle_t,
                                _In_ LPCWSTR,
                                _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&);

    // TODO: These should be made more generic and go by a configuration, rather than have
    // discrete methods for each type of transform. But right now, it's about making the
    // transform process work properly
    HRESULT GetMidiTransform(
                                _In_ handle_t,
                                _In_ MidiFlow,
                                _In_ MidiDataFormat,
                                _In_ MidiDataFormat,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&);

    HRESULT GetMidiScheduler(
                                _In_ handle_t,
                                _In_ MidiFlow,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&);

    HRESULT GetMidiJRTimestampHandler(
                                _In_ handle_t,
                                _In_ MidiFlow,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&);

    HRESULT GetMidiEndpointMetadataHandler(
                                _In_ handle_t,
                                _In_ MidiFlow,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&,
                                _In_ wil::com_ptr_nothrow<CMidiPipe>&);


    wil::critical_section m_ClientManagerLock;

    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;
    std::shared_ptr<CMidiSessionTracker> m_SessionTracker;

    std::map<MidiClientHandle, wil::com_ptr_nothrow<CMidiPipe>> m_ClientPipes;
    std::map<std::wstring, wil::com_ptr_nothrow<CMidiPipe>> m_DevicePipes;
    std::multimap<std::wstring, wil::com_ptr_nothrow<CMidiTransformPipe>> m_TransformPipes; // need this to be CMidiTransformPipe to make it reasonable to get properties

    // mmcss task id that is shared among all midi clients
    DWORD m_MmcssTaskId {0};
};

