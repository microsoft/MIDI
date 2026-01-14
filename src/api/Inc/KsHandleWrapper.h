// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cfgmgr32.h>
#include <functional>
#include "threadpoolwork.h"

enum class HandleType
{
    Filter,
    Pin
};

class KsHandleWrapper
{
public:
    // Constructor for Filter
    KsHandleWrapper(std::wstring filterName);

    // Constructor for Pin
    KsHandleWrapper(std::wstring filterName, UINT pinId, MidiTransport transport, HANDLE existingFilterHandle = nullptr);

    ~KsHandleWrapper();

    HRESULT Open();
    void Close();

    // Returns a duplicated handle for use, or nullptr if device is removed.
    HANDLE GetHandle();

    // Takes ownership of an existing handle. 
    void SetHandle(wil::unique_handle handle);

    // PnP notification handlers
    void OnDeviceQueryRemove();
    void OnDeviceQueryRemoveCancel();
    void OnDeviceRemove();

    // Register/unregister for notifications
    HRESULT RegisterForNotifications();
    void UnregisterForNotifications();

    bool IsOpen() const { return m_handle != nullptr; }

    // For lambda execution with the handle.
    template <typename Func>
    HRESULT Execute(Func&& func)
    {
        auto sharedLock = m_lock.lock_shared();

        if (!m_handle)
        {
            return E_HANDLE;
        }

        return func(m_handle.get());
    }

    HRESULT RegisterOnRemoveCallback(std::function<void()> OnRemoveCallback)
    {
        auto lock = m_lock.lock_exclusive();

        RETURN_HR_IF(E_INVALIDARG, nullptr == OnRemoveCallback);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), nullptr != m_OnRemoveCallback);

        m_OnRemoveCallback = std::move(OnRemoveCallback);

        return S_OK;
    }

    HRESULT RegisterOnRestoreCallback(std::function<void()> OnRestoreCallback)
    {
        auto lock = m_lock.lock_exclusive();

        RETURN_HR_IF(E_INVALIDARG, nullptr == OnRestoreCallback);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED), nullptr != m_OnRestoreCallback);

        m_OnRestoreCallback = std::move(OnRestoreCallback);

        return S_OK;
    }

private:

    wil::srwlock m_lock;
    wil::unique_handle m_handle;
    std::wstring m_filterName;
    HCMNOTIFICATION m_notifyContext = nullptr;

    // async processing of unregister required if needed during a callback
    std::unique_ptr<ThreadpoolWork> m_ThreadpoolWork;

    // notify the handle wrapper client of the remove/restore if needed
    std::function<void()> m_OnRemoveCallback;
    std::function<void()> m_OnRestoreCallback;

    // Specifies if it's a filter or pin handle.
    HandleType m_handleType;

    // Pin-specific
    UINT m_PinID = 0;
    MidiTransport m_Transport = MidiTransport_Invalid;
    wil::unique_handle m_ParentFilterHandle;

    // Static callback for CM notification
    static DWORD CALLBACK PnPCallback(HCMNOTIFICATION hNotify, PVOID Context, CM_NOTIFY_ACTION Action, PCM_NOTIFY_EVENT_DATA EventData, DWORD EventDataSize);
};
