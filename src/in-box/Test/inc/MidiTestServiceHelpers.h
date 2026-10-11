// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// Windows MIDI Services
// Service-level test helpers: sessions, connections, and a guard against a hung service
// https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDITESTSERVICEHELPERS_H
#define MIDITESTSERVICEHELPERS_H

// Talks to the service the way the SDK does, without the SDK, so the tests which use this run in
// the same build as the service. Include it after windows.h, wil, WexTestClass.h, the C++/WinRT
// Foundation headers, WindowsMidiServices.h and Midi2MidiSrvTransport.h.

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <winrt/Windows.Devices.Enumeration.h>

#include "ump_helpers.h"

namespace MidiTest
{
    // the calling component id the service-level transport tests use. The service only traces it.
    inline constexpr GUID ServiceTestCallingComponent
    {
        0xc24cc593, 0xbc6b, 0x4726, { 0xb5, 0x52, 0xbe, 0xc8, 0x2d, 0xed, 0xb6, 0x8c }
    };

    // Far longer than any healthy call takes, including protocol negotiation timing out at its
    // configurable maximum of 50 seconds, so only a hung service gets here.
    inline constexpr DWORD HangTimeoutMilliseconds{ 60000 };


    // Puts the calling thread in the multithreaded apartment for its lifetime. Declare it before
    // any COM pointer, so that it is destroyed after them.
    class MultithreadedApartment
    {
    public:
        MultithreadedApartment() noexcept :
            m_initialized{ SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) }
        {
            // C++/WinRT caches factories for the whole process, and they dangle once the last apartment ends.
            static bool const keptAlive = []
                {
                    CO_MTA_USAGE_COOKIE cookie{ };
                    return SUCCEEDED(CoIncrementMTAUsage(&cookie));
                }();

            (void)keptAlive;
        }

        ~MultithreadedApartment()
        {
            if (m_initialized)
            {
                CoUninitialize();
            }
        }

        MultithreadedApartment(MultithreadedApartment const&) = delete;
        MultithreadedApartment& operator=(MultithreadedApartment const&) = delete;

    private:
        bool const m_initialized;
    };


    inline bool WaitUntil(_In_ std::function<bool()> const& condition, _In_ DWORD const timeoutMilliseconds)
    {
        auto const deadline = GetTickCount64() + timeoutMilliseconds;

        for (;;)
        {
            if (condition())
            {
                return true;
            }

            if (GetTickCount64() >= deadline)
            {
                return false;
            }

            Sleep(20);
        }
    }


    struct GuardedRunResult
    {
        bool Finished{ false };     // false means it was still running when the time ran out
        HRESULT Result{ S_OK };     // what the work threw, if it threw
    };

    // Runs work on its own thread and waits for it. A hung service never returns from the call
    // that hangs, so waiting for that call inline would stall the whole test pass instead of
    // failing one test.
    //
    // work must own everything it touches: capture shared_ptrs and COM pointers by value, never
    // the caller's locals by reference. When time runs out the thread is abandoned, still
    // holding whatever work captured.
    inline GuardedRunResult RunWithHangGuard(
        _In_ std::function<void()> work,
        _In_ DWORD const timeoutMilliseconds = HangTimeoutMilliseconds)
    {
        struct State
        {
            std::function<void()> Work{ };
            HRESULT Result{ S_OK };
            wil::unique_event_nothrow Finished{ };
        };

        auto state = std::make_shared<State>();
        state->Work = std::move(work);

        THROW_IF_FAILED(state->Finished.create(wil::EventOptions::ManualReset));

        std::thread worker([state]
            {
                {
                    MultithreadedApartment apartment{ };

                    try
                    {
                        state->Work();
                    }
                    catch (...)
                    {
                        state->Result = wil::ResultFromCaughtException();
                    }

                    // released here, inside the apartment the captured objects were used in
                    state->Work = nullptr;
                }

                state->Finished.SetEvent();
            });

        if (state->Finished.wait(timeoutMilliseconds))
        {
            worker.join();

            return { true, state->Result };
        }

        // Keep this DLL loaded, so the abandoned thread's code is still there if its call into
        // the service ever returns.
        static char anchor{ 0 };
        HMODULE pinned{ nullptr };

        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&anchor),
            &pinned);

        worker.detach();

        return { false, S_OK };
    }


    // Collects what arrives on one connection. Reference counted for real, because the service
    // transport holds it until the connection closes, and an abandoned connection holds it for good.
    class ReceivedMessages final : public IMidiCallback
    {
    public:
        // keepMessages false only counts them, for loops which carry millions of messages
        static wil::com_ptr_nothrow<ReceivedMessages> Create(_In_ bool const keepMessages = true) noexcept
        {
            wil::com_ptr_nothrow<ReceivedMessages> created{ };
            created.attach(new (std::nothrow) ReceivedMessages(keepMessages));

            return created;
        }

        STDMETHOD(QueryInterface)(_In_ REFIID riid, _COM_Outptr_ void** object) noexcept override
        {
            if (object == nullptr)
            {
                return E_POINTER;
            }

            if (riid == __uuidof(IUnknown) || riid == __uuidof(IMidiCallback))
            {
                *object = static_cast<IMidiCallback*>(this);
                AddRef();

                return S_OK;
            }

            *object = nullptr;

            return E_NOINTERFACE;
        }

        STDMETHOD_(ULONG, AddRef)() noexcept override
        {
            return static_cast<ULONG>(InterlockedIncrement(&m_referenceCount));
        }

        STDMETHOD_(ULONG, Release)() noexcept override
        {
            auto const count = InterlockedDecrement(&m_referenceCount);

            if (count == 0)
            {
                delete this;
            }

            return static_cast<ULONG>(count);
        }

        STDMETHOD(Callback)(
            _In_ MessageOptionFlags,
            _In_ PVOID message,
            _In_ UINT size,
            _In_ LONGLONG,
            _In_ LONGLONG) noexcept override
        {
            // Messages sharing a timestamp can arrive together in one callback.
            auto const words = static_cast<uint32_t const*>(message);
            auto const wordCount = static_cast<size_t>(size / sizeof(uint32_t));

            size_t index{ 0 };

            while (index < wordCount)
            {
                auto const length = static_cast<size_t>(
                    WindowsMidiServicesInternal::GetUmpLengthInMidiWordsFromFirstWord(words[index]));

                if (length == 0 || index + length > wordCount)
                {
                    break;
                }

                if (m_keepMessages)
                {
                    try
                    {
                        auto lock = m_messagesLock.lock();
                        m_messages.emplace_back(words + index, words + index + length);
                    }
                    catch (...)
                    {
                    }
                }

                m_count++;

                {
                    auto lock = m_echoLock.lock();

                    if (m_echoThrough != nullptr)
                    {
                        if (SUCCEEDED(m_echoThrough->SendMidiMessage(
                            MessageOptionFlags_None,
                            const_cast<uint32_t*>(words + index),
                            static_cast<UINT>(length * sizeof(uint32_t)),
                            0)))
                        {
                            m_echoCount++;
                        }
                    }
                }

                index += length;
            }

            return S_OK;
        }

        size_t Count() const noexcept { return m_count; }
        uint64_t EchoCount() const noexcept { return m_echoCount; }

        std::vector<std::vector<uint32_t>> Messages()
        {
            auto lock = m_messagesLock.lock();

            return m_messages;
        }

        bool WaitForMessage(_In_ std::vector<uint32_t> const& words, _In_ DWORD const timeoutMilliseconds)
        {
            return WaitUntil([&]
                {
                    auto lock = m_messagesLock.lock();

                    return std::find(m_messages.begin(), m_messages.end(), words) != m_messages.end();
                },
                timeoutMilliseconds);
        }

        bool WaitForFirstWord(_In_ std::function<bool(uint32_t)> const& matches, _In_ DWORD const timeoutMilliseconds)
        {
            return WaitUntil([&]
                {
                    auto lock = m_messagesLock.lock();

                    return std::any_of(m_messages.begin(), m_messages.end(),
                        [&](std::vector<uint32_t> const& received) { return !received.empty() && matches(received[0]); });
                },
                timeoutMilliseconds);
        }

        // Sends everything which arrives straight back out through connection, which turns both
        // ends of a loopback into a feedback loop. Set it back to nullptr before the connection
        // closes: this holds the pointer without a reference.
        void EchoThrough(_In_opt_ IMidiBidirectional* connection) noexcept
        {
            auto lock = m_echoLock.lock();

            m_echoThrough = connection;
        }

    private:
        explicit ReceivedMessages(_In_ bool const keepMessages) noexcept :
            m_keepMessages{ keepMessages }
        {
        }

        ~ReceivedMessages() = default;

        LONG m_referenceCount{ 1 };
        bool const m_keepMessages;

        std::atomic<size_t> m_count{ 0 };
        std::atomic<uint64_t> m_echoCount{ 0 };

        wil::critical_section m_messagesLock{ };
        std::vector<std::vector<uint32_t>> m_messages{ };

        wil::critical_section m_echoLock{ };
        IMidiBidirectional* m_echoThrough{ nullptr };
    };


    // A session registered with the service, as an app's MidiSession would be, and the
    // transport its connections are opened through.
    class ServiceSession
    {
    public:
        ServiceSession() = default;

        ~ServiceSession()
        {
            Close();
        }

        ServiceSession(ServiceSession const&) = delete;
        ServiceSession& operator=(ServiceSession const&) = delete;

        HRESULT Open(_In_z_ wchar_t const* name) noexcept
        {
            RETURN_IF_FAILED(CoCreateInstance(__uuidof(Midi2MidiSrvTransport), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&m_transport)));
            RETURN_IF_FAILED(m_transport->Activate(__uuidof(IMidiSessionTracker), (void**)&m_tracker));
            RETURN_IF_FAILED(m_tracker->Initialize());
            RETURN_IF_FAILED(CoCreateGuid(&m_id));
            RETURN_IF_FAILED(m_tracker->AddClientSession(m_id, name));

            m_registered = true;

            return S_OK;
        }

        // A UMP connection to one endpoint, in this session. What arrives goes to callback.
        HRESULT Connect(
            _In_ std::wstring const& endpointDeviceId,
            _In_ IMidiCallback* callback,
            _Inout_ wil::com_ptr_nothrow<IMidiBidirectional>& connection) noexcept
        {
            connection.reset();

            RETURN_HR_IF_NULL(E_UNEXPECTED, m_transport);

            wil::com_ptr_nothrow<IMidiBidirectional> created{ };
            RETURN_IF_FAILED(m_transport->Activate(__uuidof(IMidiBidirectional), (void**)&created));

            TRANSPORTCREATIONPARAMS creationParams{ MessageOptionFlags_None, MidiDataFormats_UMP, ServiceTestCallingComponent };
            DWORD mmcssTaskId{ 0 };

            RETURN_IF_FAILED(created->Initialize(endpointDeviceId.c_str(), &creationParams, &mmcssTaskId, callback, 0, m_id));

            connection = std::move(created);

            return S_OK;
        }

        HRESULT GetSessionList(_Inout_ std::wstring& sessionListJson) noexcept try
        {
            sessionListJson.clear();

            RETURN_HR_IF_NULL(E_UNEXPECTED, m_tracker);

            wil::unique_cotaskmem_string list{ };
            RETURN_IF_FAILED(m_tracker->GetSessionList(list.put()));

            if (list)
            {
                sessionListJson = list.get();
            }

            return S_OK;
        }
        CATCH_RETURN();

        void Close() noexcept
        {
            if (m_registered)
            {
                m_tracker->RemoveClientSession(m_id);
                m_registered = false;
            }

            m_tracker.reset();
            m_transport.reset();
        }

        // After the service hangs, so that nothing here calls into it again. Leaks on purpose.
        void Abandon() noexcept
        {
            m_registered = false;
            (void)m_tracker.detach();
            (void)m_transport.detach();
        }

        GUID Id() const noexcept { return m_id; }

    private:
        wil::com_ptr_nothrow<IMidiTransport> m_transport{ };
        wil::com_ptr_nothrow<IMidiSessionTracker> m_tracker{ };
        GUID m_id{ };
        bool m_registered{ false };
    };


    // Closes a connection on a guarded thread. False means the service never returned.
    inline bool CloseConnectionWithHangGuard(_Inout_ wil::com_ptr_nothrow<IMidiBidirectional>& connection) noexcept
    {
        if (connection == nullptr)
        {
            return true;
        }

        wil::com_ptr_nothrow<IMidiBidirectional> toClose{ std::move(connection) };

        try
        {
            return RunWithHangGuard([toClose]() mutable
                {
                    toClose->Shutdown();
                    toClose.reset();
                }).Finished;
        }
        catch (...)
        {
            return false;
        }
    }


    inline HRESULT SendWords(_In_ IMidiBidirectional* connection, _In_ std::vector<uint32_t> const& words) noexcept
    {
        RETURN_HR_IF_NULL(E_POINTER, connection);
        RETURN_HR_IF(E_INVALIDARG, words.empty());

        return connection->SendMidiMessage(
            MessageOptionFlags_None,
            const_cast<uint32_t*>(words.data()),
            static_cast<UINT>(words.size() * sizeof(uint32_t)),
            0);
    }


    // One property of an endpoint's device interface, or nullptr when it is not set or the
    // endpoint is not there.
    inline winrt::Windows::Foundation::IInspectable ReadEndpointProperty(
        _In_ std::wstring const& endpointDeviceId,
        _In_ winrt::hstring const& propertyKey) noexcept
    {
        try
        {
            auto additionalProperties = winrt::single_threaded_vector<winrt::hstring>();
            additionalProperties.Append(propertyKey);

            auto info = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                endpointDeviceId,
                additionalProperties,
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

            if (info == nullptr || !info.Properties().HasKey(propertyKey))
            {
                return nullptr;
            }

            return info.Properties().Lookup(propertyKey);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    inline std::wstring ReadEndpointStringProperty(
        _In_ std::wstring const& endpointDeviceId,
        _In_ winrt::hstring const& propertyKey) noexcept
    {
        auto const value = ReadEndpointProperty(endpointDeviceId, propertyKey);

        try
        {
            return std::wstring{ winrt::unbox_value_or<winrt::hstring>(value, L"") };
        }
        catch (...)
        {
            return {};
        }
    }

    inline std::optional<bool> ReadEndpointBoolProperty(
        _In_ std::wstring const& endpointDeviceId,
        _In_ winrt::hstring const& propertyKey) noexcept
    {
        auto const value = ReadEndpointProperty(endpointDeviceId, propertyKey);

        if (value == nullptr)
        {
            return std::nullopt;
        }

        try
        {
            auto const reference = value.try_as<winrt::Windows::Foundation::IReference<bool>>();

            if (reference == nullptr)
            {
                return std::nullopt;
            }

            return reference.Value();
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    inline size_t Utf8ByteCount(_In_ std::wstring const& value) noexcept
    {
        if (value.empty())
        {
            return 0;
        }

        auto const count = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.length()), nullptr, 0, nullptr, nullptr);

        return count > 0 ? static_cast<size_t>(count) : 0;
    }
}

#endif
