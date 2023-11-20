// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

class CMidiTransformManager
{
public:

    CMidiTransformManager() {}
    ~CMidiTransformManager() {}

    HRESULT Initialize(_In_ std::shared_ptr<CMidiPerformanceManager>& performanceManager,
        _In_ std::shared_ptr<CMidiClientManager>& clientManager,
        _In_ std::shared_ptr<CMidiDeviceManager>& deviceManager);

    //HRESULT CreateMidiClient(_In_ handle_t,
    //    _In_ LPCWSTR,
    //    _In_ PMIDISRV_CLIENTCREATION_PARAMS,
    //    _In_ PMIDISRV_CLIENT);

    //HRESULT DestroyMidiClient(_In_ handle_t,
    //    _In_ MidiClientHandle);

    HRESULT Cleanup();

private:
//    wil::critical_section m_ClientManagerLock;

    std::shared_ptr<CMidiPerformanceManager> m_performanceManager;
    std::shared_ptr<CMidiProcessManager> m_processManager;
    std::shared_ptr<CMidiDeviceManager> m_deviceManager;
    std::shared_ptr<CMidiClientManager> m_clientManager;

 //   std::map<MidiClientHandle, wil::com_ptr_nothrow<CMidiClientPipe>> m_clientPipes;
 //   std::map<std::wstring, wil::com_ptr_nothrow<CMidiDevicePipe>> m_devicePipes;

    // mmcss task id that is shared among all midi clients
 //   DWORD m_MmcssTaskId{ 0 };
};