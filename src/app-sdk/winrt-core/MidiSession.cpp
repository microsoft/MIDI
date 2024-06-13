// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================


#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"

#include "MidiEndpointConnection.h"

//#include <atlcomcli.h>

namespace winrt::Microsoft::Windows::Devices::Midi2::implementation
{

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::Create(
        winrt::hstring const& sessionName
        ) noexcept
    {
        try
        {
            auto session = winrt::make_self<implementation::MidiSession>();

            session->SetName(sessionName);

            if (session->InternalStart())
            {
                return *session;
            }
            else
            {
                // this can happen is service is not available or running
                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception initializing creating session. Service may be unavailable.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return nullptr;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(MIDI_SDK_STATIC_THIS_PLACEHOLDER_FIELD_VALUE, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception initializing creating session. Service may be unavailable", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return nullptr;
        }
    }

    // Internal method called inside the API to connect to the abstraction. Called by the code which creates
    // the session instance
    _Use_decl_annotations_
    bool MidiSession::InternalStart()
    {
        try
        {
            // We're talking to the service, so use the MIDI Service abstraction, not a KS or other one
            m_serviceAbstraction = winrt::create_instance<IMidiAbstraction>(__uuidof(Midi2MidiSrvAbstraction), CLSCTX_ALL);

            if (m_serviceAbstraction != nullptr)
            {
                // the session id is created on the client and provided in calls to the endpoints
                m_id = foundation::GuidHelper::CreateNewGuid();

                m_isOpen = true;


                // register the session

                if (SUCCEEDED(m_serviceAbstraction->Activate(__uuidof(IMidiSessionTracker), (void**)&m_sessionTracker)))
                {
                    auto initHR = m_sessionTracker->Initialize();
                    if (FAILED(initHR))
                    {
                        LOG_IF_FAILED(initHR);   // this also generates a fallback error with file and line number info

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Unable to initialize session tracker?", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingHResult(initHR, MIDI_SDK_TRACE_HRESULT_FIELD)
                        );
                        return false;
                    }

                    auto addHR = m_sessionTracker->AddClientSession(m_id, m_name.c_str());
                    if (FAILED(addHR))
                    {
                        LOG_IF_FAILED(addHR);   // this also generates a fallback error with file and line number info

                        TraceLoggingWrite(
                            Midi2SdkTelemetryProvider::Provider(),
                            MIDI_SDK_TRACE_EVENT_ERROR,
                            TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                            TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                            TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                            TraceLoggingWideString(L"Unable to add client session to session tracker", MIDI_SDK_TRACE_MESSAGE_FIELD),
                            TraceLoggingHResult(addHR, MIDI_SDK_TRACE_HRESULT_FIELD)
                        );

                        return false;
                    }
                }
                else
                {
                    LOG_IF_FAILED(E_FAIL);   // this also generates a fallback error with file and line number info

                    TraceLoggingWrite(
                        Midi2SdkTelemetryProvider::Provider(),
                        MIDI_SDK_TRACE_EVENT_ERROR,
                        TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                        TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                        TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                        TraceLoggingWideString(L"Error activating IMidiSessionTracker", MIDI_SDK_TRACE_MESSAGE_FIELD)
                    );

                    return false;
                }
            }
            else
            {
                LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Error starting session. Service abstraction is nullptr", MIDI_SDK_TRACE_MESSAGE_FIELD)
                );

                return false;
            }

        }
        catch (winrt::hresult_error const& ex)
        {
            LOG_IF_FAILED(static_cast<HRESULT>(ex.code()));   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"hresult exception starting session. Service may be unavailable.", MIDI_SDK_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(ex.code()), MIDI_SDK_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(ex.message().c_str(), MIDI_SDK_TRACE_ERROR_FIELD)
            );

            return false;
        }
        catch (...)
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception starting session", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiSession::UpdateName(winrt::hstring const& newName) noexcept
    {
        auto cleanName = internal::TrimmedHStringCopy(newName);

        // this can be called only if we've already initialized the session tracker
        if (m_sessionTracker)
        {
            DWORD clientProcessId = GetCurrentProcessId();

            auto hr = m_sessionTracker->UpdateClientSessionName(m_id, cleanName.c_str(), clientProcessId);

            if (SUCCEEDED(hr))
            {
                m_name = cleanName;
                return true;
            }
            else
            {
                LOG_IF_FAILED(hr);   // this also generates a fallback error with file and line number info

                TraceLoggingWrite(
                    Midi2SdkTelemetryProvider::Provider(),
                    MIDI_SDK_TRACE_EVENT_ERROR,
                    TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                    TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                    TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                    TraceLoggingWideString(L"Unable to update session name", MIDI_SDK_TRACE_MESSAGE_FIELD),
                    TraceLoggingHResult(hr, MIDI_SDK_TRACE_HRESULT_FIELD)
                );

                return false;
            }
        }
        else
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Session tracker interface wasn't already initialized", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

            return false;
        }
    }




    void MidiSession::Close() noexcept
    {
        if (!m_isOpen) return;

        try
        {
            if (m_serviceAbstraction != nullptr)
            {
                // TODO: Call any cleanup method on the service
                for (auto connection : m_connections)
                {
                    // close the one connection
                    //connection.Value().as<foundation::IClosable>().Close();

                    auto c = winrt::get_self<implementation::MidiEndpointConnection>(connection.Value());
                    c->InternalClose();
                }

                m_serviceAbstraction = nullptr;
            }

            if (m_sessionTracker != nullptr)
            {
                m_sessionTracker->RemoveClientSession(m_id);

                m_sessionTracker = nullptr;
            }

            m_connections.Clear();

            // Id is no longer valid, and session is not open. Clear these in case the client tries to use the held reference
            //m_id.clear();
            m_isOpen = false;
            m_mmcssTaskId = 0;
            
        }
        catch (...)
        {
            LOG_IF_FAILED(E_POINTER);   // this also generates a fallback error with file and line number info

            TraceLoggingWrite(
                Midi2SdkTelemetryProvider::Provider(),
                MIDI_SDK_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_SDK_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingPointer(this, MIDI_SDK_TRACE_THIS_FIELD),
                TraceLoggingWideString(L"Exception on close", MIDI_SDK_TRACE_MESSAGE_FIELD)
            );

        }

    }


    MidiSession::~MidiSession() noexcept
    {
        if (m_isOpen)
        {
            Close();
        }
    }



}
