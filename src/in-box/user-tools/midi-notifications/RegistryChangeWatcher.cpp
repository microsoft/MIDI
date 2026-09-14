// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"

_Use_decl_annotations_
HRESULT RegistryChangeWatcher::StartVolatile(
    HKEY const rootKey,
    std::wstring const& parentPath,
    std::wstring const& subKeyName,
    std::function<void()> onChanged) noexcept
try
{
    wil::unique_hkey parent{ };

    RETURN_IF_WIN32_ERROR(::RegOpenKeyExW(
        rootKey,
        parentPath.c_str(),
        0,
        KEY_CREATE_SUB_KEY,
        parent.put()));

    RETURN_IF_WIN32_ERROR(::RegCreateKeyExW(
        parent.get(),
        subKeyName.c_str(),
        0,
        nullptr,
        REG_OPTION_VOLATILE,
        KEY_NOTIFY | KEY_QUERY_VALUE,
        nullptr,
        m_key.put(),
        nullptr));

    return BeginWatching(std::move(onChanged));
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT RegistryChangeWatcher::StartPersistent(
    HKEY const rootKey,
    std::wstring const& subKeyPath,
    std::function<void()> onChanged) noexcept
try
{
    RETURN_IF_WIN32_ERROR(::RegCreateKeyExW(
        rootKey,
        subKeyPath.c_str(),
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_NOTIFY | KEY_QUERY_VALUE,
        nullptr,
        m_key.put(),
        nullptr));

    return BeginWatching(std::move(onChanged));
}
CATCH_RETURN()

_Use_decl_annotations_
HRESULT RegistryChangeWatcher::BeginWatching(std::function<void()> onChanged) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, onChanged);

    m_onChanged = std::move(onChanged);

    m_changeEvent.create(wil::EventOptions::ManualReset);

    m_wait = ::CreateThreadpoolWait(&OnWaitCallback, this, nullptr);
    RETURN_LAST_ERROR_IF_NULL(m_wait);

    RETURN_IF_FAILED(ArmNotification());

    return S_OK;
}
CATCH_RETURN()

void RegistryChangeWatcher::Stop() noexcept
{
    {
        // The cancel shares the lock with arming, so whichever of the two runs second sees the
        // other's result: either the re-arm is undone here, or it never happens.
        std::lock_guard<std::mutex> guard{ m_armLock };

        m_stopping = true;

        if (m_wait != nullptr)
        {
            ::SetThreadpoolWait(m_wait, nullptr, nullptr);
        }
    }

    // Outside the lock, because a callback which is already running has to be able to take it
    // and finish before this can return.
    if (m_wait != nullptr)
    {
        ::WaitForThreadpoolWaitCallbacks(m_wait, TRUE);
        ::CloseThreadpoolWait(m_wait);
        m_wait = nullptr;
    }

    m_key.reset();
    m_changeEvent.reset();
}

HRESULT RegistryChangeWatcher::ArmNotification() noexcept
{
    std::lock_guard<std::mutex> guard{ m_armLock };

    if (m_stopping || !m_key || !m_changeEvent || m_wait == nullptr)
    {
        return S_FALSE;
    }

    m_changeEvent.ResetEvent();

    // Re-armed after every change, because a single registration reports one change only.
    RETURN_IF_WIN32_ERROR(::RegNotifyChangeKeyValue(
        m_key.get(),
        FALSE,
        REG_NOTIFY_CHANGE_LAST_SET,
        m_changeEvent.get(),
        TRUE));

    ::SetThreadpoolWait(m_wait, m_changeEvent.get(), nullptr);

    return S_OK;
}

_Use_decl_annotations_
void CALLBACK RegistryChangeWatcher::OnWaitCallback(
    PTP_CALLBACK_INSTANCE,
    PVOID context,
    PTP_WAIT,
    TP_WAIT_RESULT) noexcept
{
    auto self = static_cast<RegistryChangeWatcher*>(context);

    if (self == nullptr)
    {
        return;
    }

    // Re-armed before the callback runs, so a change which happens while the callback is working
    // is still caught rather than falling into the gap. S_FALSE means Stop won the race.
    auto const armed = self->ArmNotification();

    LOG_IF_FAILED(armed);

    if (armed != S_OK)
    {
        return;
    }

    try
    {
        self->m_onChanged();
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
    }
}
