// Copyright (c) Microsoft Corporation. All rights reserved.
#pragma once

class CMidiClientManager
{
public:

    CMidiClientManager() {}
    ~CMidiClientManager() {}
    
    HRESULT Initialize(_In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager,
                            _In_ std::shared_ptr<CMidiProcessManager>& processManager,
                            _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager);

    HRESULT CreateMidiClient(_In_ handle_t,
                                _In_ LPCWSTR,
                                _In_ PMIDISRV_CLIENTCREATION_PARAMS,
                                _In_ PMIDISRV_CLIENT);

    HRESULT DestroyMidiClient(_In_ handle_t,
                                _In_ MidiClientHandle);

    HRESULT Cleanup();

private:
    std::shared_ptr<CMidiPerformanceManager> m_PerformanceManager;
    std::shared_ptr<CMidiProcessManager> m_ProcessManager;
    std::shared_ptr<CMidiDeviceManager> m_DeviceManager;

    // TODO: convert this to a list of clients
    wil::com_ptr_nothrow<CMidiClientPipe> m_ClientPipe;

    wil::com_ptr_nothrow<CMidiDevicePipe> m_DevicePipe;

    // mmcss task id that is shared among all midi clients
    DWORD m_MmcssTaskId {0};
};

