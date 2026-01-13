// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include <windows.h>
#include <cguid.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <devioctl.h>
#include <ks.h>
#include <ksmedia.h>
#include <wil\com.h>
#include <wil\resource.h>
#include <wil\result_macros.h>

#include <Devpkey.h>
#include "MidiDefs.h"
#include "MidiKsDef.h"
#include "MidiKsCommon.h"
#include "KsHandleWrapper.h"

// Filter constructor
KsHandleWrapper::KsHandleWrapper(std::wstring pwszFilterName)
{
    m_filterName = pwszFilterName;
    m_notifyContext = nullptr;
    m_handleType = HandleType::Filter;
}

// Pin constructor
KsHandleWrapper::KsHandleWrapper(std::wstring pwszFilterName, UINT pinId, MidiTransport transport, HANDLE existingFilterHandle)
{
    m_filterName = pwszFilterName;
    m_notifyContext = nullptr;
    m_handleType = HandleType::Pin;
    m_PinID = pinId;
    m_Transport = transport;

    if (existingFilterHandle)
    {
        HANDLE dupHandle = nullptr;
        if (DuplicateHandle(GetCurrentProcess(), existingFilterHandle, GetCurrentProcess(), &dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            m_ParentFilterHandle.reset(dupHandle);
        }
    }
}

KsHandleWrapper::~KsHandleWrapper()
{
    // UnregisterForNotifications blocks until all callbacks have completed,
    // do not acquire the lock here, because doing so will block callbacks from completing
    UnregisterForNotifications();
    m_handle.reset();
    m_OnRemoveCallback = nullptr;
    m_OnRestoreCallback = nullptr;
}

HRESULT KsHandleWrapper::Open()
{
    auto lock = m_lock.lock_exclusive();

    if (m_handleType == HandleType::Filter)
    {
        wil::unique_handle filter;
        RETURN_IF_FAILED(FilterInstantiate(m_filterName.c_str(), &filter));

        // Take ownership of the handle
        m_handle = std::move(filter);
    }
    else if (m_handleType == HandleType::Pin)
    {
        wil::unique_handle pin;

        // If the caller didn't pass a filter handle, instantiate one.
        if (!m_ParentFilterHandle)
        {
            wil::unique_handle filter;
            RETURN_IF_FAILED(FilterInstantiate(m_filterName.c_str(), &filter));
            m_ParentFilterHandle = std::move(filter);
        }

        RETURN_IF_FAILED(InstantiateMidiPin(m_ParentFilterHandle.get(), m_PinID, m_Transport, &pin));
        m_handle = std::move(pin);
    }

    RETURN_IF_FAILED(RegisterForNotifications());

    return S_OK;
}

void KsHandleWrapper::Close()
{
    // cancel pending ioctls for midi pin
    {
        auto sharedLock = m_lock.lock_shared();
        if (m_handleType == HandleType::Pin && m_handle)
        {
            CancelIoEx(m_handle.get(), nullptr);
        }
    }

    // acquire exclusive lock to reset handle
    {
        auto lock = m_lock.lock_exclusive();
        m_handle.reset();
    }
}

HANDLE KsHandleWrapper::GetHandle()
{
    auto lock = m_lock.lock_shared();

    if (!m_handle)
    {
        return nullptr;
    }  

    HANDLE dupHandle = nullptr;
    BOOL result = DuplicateHandle(
        GetCurrentProcess(),
        m_handle.get(),
        GetCurrentProcess(),
        &dupHandle,
        0,
        FALSE,
        DUPLICATE_SAME_ACCESS);

    return result ? dupHandle : nullptr;
}

void KsHandleWrapper::SetHandle(wil::unique_handle handle)
{
    auto lock = m_lock.lock_exclusive();

    // Take ownership of the existing handle
    m_handle = std::move(handle);
}

void KsHandleWrapper::OnDeviceQueryRemove()
{
    // If the handle user is registered, notify them that it is about
    // to close
    if (m_OnRemoveCallback)
    {
        m_OnRemoveCallback();
    }

    // Close the handle to allow PnP to remove the device
    Close();
}

void KsHandleWrapper::OnDeviceQueryRemoveCancel()
{
    // Reopen the handle
    Open();

    // If the handle user is registered, notify them that it is has
    // been restored
    if (m_OnRestoreCallback)
    {
        m_OnRestoreCallback();
    }
}

void KsHandleWrapper::OnDeviceRemove()
{
    // If the handle user is registered, notify them that it is about
    // to close
    if (m_OnRemoveCallback)
    {
        m_OnRemoveCallback();
    }

    // Surprise removal: close the handle
    Close();
}

HRESULT KsHandleWrapper::RegisterForNotifications()
{

    if (!m_handle)
    {
        return E_HANDLE;
    }

    // If our threadpool cleanup worker doesn't yet exist, create
    // it now
    if (!m_ThreadpoolWork)
    {
        m_ThreadpoolWork = std::make_unique<ThreadpoolWork>();
    }

    if (m_notifyContext)
    {
        // spin off a worker to unregister the old context, doing this in a callback
        // will deadlock because CM_Unregister_Notification blocks until all callbacks
        // have completed
        m_ThreadpoolWork->Submit([h = m_notifyContext]{ CM_Unregister_Notification(h); });
        m_notifyContext = nullptr;
    }

    // Define the notification filter
    CM_NOTIFY_FILTER cmFilter;
    ZeroMemory(&cmFilter, sizeof(cmFilter));
    cmFilter.cbSize = sizeof(cmFilter);
    cmFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE;
    cmFilter.u.DeviceHandle.hTarget = m_handle.get();

    // Register the notification
    CONFIGRET cr = CM_Register_Notification(&cmFilter, this, &KsHandleWrapper::PnPCallback, &m_notifyContext);

    return (cr == CR_SUCCESS) ? S_OK : HRESULT_FROM_WIN32(cr);
}

void KsHandleWrapper::UnregisterForNotifications()
{

    if (m_notifyContext)
    {
        CM_Unregister_Notification(m_notifyContext);
        m_notifyContext = nullptr;
    }

    if (m_ThreadpoolWork)
    {
        // This will wait until all callbacks have completed before
        // returning, ensuring that all previous calls to CM_Register_Notification have
        // been closed.
        m_ThreadpoolWork.reset();
    }
}

DWORD CALLBACK KsHandleWrapper::PnPCallback(
    _In_ HCMNOTIFICATION hNotify,
    _In_opt_ PVOID Context,
    _In_ CM_NOTIFY_ACTION Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD EventDataSize)
{
    UNREFERENCED_PARAMETER(hNotify);
    UNREFERENCED_PARAMETER(EventDataSize);

    if (EventData->FilterType != CM_NOTIFY_FILTER_TYPE_DEVICEHANDLE)
    {
        return ERROR_INVALID_PARAMETER;
    }

    KsHandleWrapper* wrapper = (KsHandleWrapper*)Context;

    switch (Action)
    {
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVE:
        wrapper->OnDeviceQueryRemove();
        break;
    case CM_NOTIFY_ACTION_DEVICEQUERYREMOVEFAILED:
        wrapper->OnDeviceQueryRemoveCancel();
        break;
    case CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL:
    case CM_NOTIFY_ACTION_DEVICEREMOVEPENDING:
    case CM_NOTIFY_ACTION_DEVICEREMOVECOMPLETE:
        wrapper->OnDeviceRemove();
        break;
    default:
        break;
    }

    return ERROR_SUCCESS;
}
