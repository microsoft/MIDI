// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"
#include "MidiSession.h"
#include "MidiSession.g.cpp"

#include "MidiEndpointConnection.h"

//#include <atlcomcli.h>

namespace MIDI_CPP_NAMESPACE::implementation
{

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::CreateSession(
        winrt::hstring const& sessionName,
        midi2::MidiSessionSettings const& settings
        ) noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Session create");

        try
        {
            auto session = winrt::make_self<implementation::MidiSession>();

            session->SetName(sessionName);
            session->SetSettings(settings);

            if (session->InternalStart())
            {
                return *session;
            }
            else
            {
                // this can happen is service is not available or running

                internal::LogInfo(__FUNCTION__, L"Session start failed. Returning nullptr");

                return nullptr;
            }
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception initializing creating session. Service may be unavailable.", ex);

            return nullptr;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception initializing creating session. Service may be unavailable.");

            return nullptr;
        }
    }

    _Use_decl_annotations_
    midi2::MidiSession MidiSession::CreateSession(
        winrt::hstring const& sessionName
        ) noexcept
    {
        return CreateSession(sessionName, MidiSessionSettings());
    }

    // Internal method called inside the API to connect to the abstraction. Called by the code which creates
    // the session instance
    _Use_decl_annotations_
    bool MidiSession::InternalStart()
    {
        internal::LogInfo(__FUNCTION__, L"Start Session ");

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
                    m_sessionTracker->Initialize();

                    DWORD clientProcessId = GetCurrentProcessId();

                    //std::wstring modulePath{ _wpgmptr };

                    std::wstring modulePath{ 0 };
                    modulePath.resize(2048);   // MAX_PATH is almost never big enough. This is a wild shot. Not going to allocate 32k chars for this but I know this will bite me some day

                    auto numPathCharacters = GetModuleFileName(NULL, modulePath.data(), (DWORD)modulePath.capacity());
                    
                    if (numPathCharacters > 0)
                    {
                        internal::LogInfo(__FUNCTION__, (std::wstring(L"Module Path: ") + modulePath).c_str());

                        std::wstring processName = (std::filesystem::path(modulePath).filename()).c_str();
                        internal::LogInfo(__FUNCTION__, (std::wstring(L"Process Name: ") + processName).c_str());

                        m_sessionTracker->AddClientSession(m_id, m_name.c_str(), clientProcessId, processName.c_str());
                    }
                    else
                    {
                        // couldn't get the process name
                        internal::LogGeneralError(__FUNCTION__, L"Unable to get current process name.");

                        return false;
                    }
                }
                else
                {
                    internal::LogGeneralError(__FUNCTION__, L"Error activating IMidiSessionTracker.");

                    return false;
                }

            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L"Error starting session. Service abstraction is nullptr.");

                return false;
            }

        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception starting session. Service may be unavailable.", ex);

            return false;
        }
        catch (...)
        {
            internal::LogGeneralError(__FUNCTION__, L"Exception starting session.");

            return false;
        }

        return true;
    }

    _Use_decl_annotations_
    bool MidiSession::UpdateName(winrt::hstring const& newName) noexcept
    {
        UNREFERENCED_PARAMETER(newName);

        internal::LogInfo(__FUNCTION__, L"Enter");

        // TODO: Implement UpdateName


        return false;

    }




    void MidiSession::Close() noexcept
    {
        internal::LogInfo(__FUNCTION__, L"Closing session");

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
            internal::LogGeneralError(__FUNCTION__, L"Exception on close");
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
