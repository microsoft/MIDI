#include "pch.h"
#include "MidiBidirectionalAggregatedEndpointConnection.h"
#include "MidiBidirectionalAggregatedEndpointConnection.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{


    // Callback handler from the Midi Service endpoint abstraction

    IFACEMETHODIMP MidiBidirectionalAggregatedEndpointConnection::Callback(_In_ PVOID data, _In_ UINT size, _In_ LONGLONG timestamp)
    {
        try
        {
            // one copy of the event args for this gets sent to all listeners and the main event
            auto args = winrt::make_self<MidiMessageReceivedEventArgs>(data, size, timestamp);

            // we failed to create the event args
            if (args == nullptr)
            {
                internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

                return E_FAIL;
            }

            bool skipMainMessageReceivedEvent = false;
            bool skipFurtherListeners = false;

            // If any listeners are hooked up, use them

            if (m_messageListeners && m_messageListeners.Size() > 0)
            {
                // loop through listeners
                for (const auto& listener : m_messageListeners)
                {
                    // TODO: this is synchronous by design, but that requires the client to not block
                    listener.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

                    // if the listener has told us to skip further listeners, effectively 
                    // removing this message from the queue, then break out of the loop
                    if (skipFurtherListeners) break;
                }
            }

            // if the main message received event is hooked up, and we're not skipping it, use it
            if (m_messageReceivedEvent && !skipMainMessageReceivedEvent)
            {
                m_messageReceivedEvent(*this, *args);
            }

            return S_OK;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception calling message handlers", ex);

            return E_FAIL;
        }
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpBuffer(
        _In_ internal::MidiTimestamp /* timestamp*/,
        _In_ winrt::Windows::Foundation::IMemoryBuffer const& /*buffer*/,
        _In_ uint32_t const /*byteOffset*/,
        _In_ uint32_t const /*byteLength*/
        )
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpWordArray(
        _In_ internal::MidiTimestamp const /* timestamp*/,
        _In_ array_view<uint32_t const> const /*words*/,
        _In_ uint32_t const /*wordCount*/
        )
    {
        throw hresult_not_implemented();
    }



    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const /* timestamp*/,
        _In_ uint32_t const /*word0*/
    )
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const /* timestamp*/,
        _In_ uint32_t const /*word0*/,
        _In_ uint32_t const /*word1*/
    )
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const /* timestamp*/,
        _In_ uint32_t const /*word0*/,
        _In_ uint32_t const /*word1*/,
        _In_ uint32_t const /*word2*/
        )
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmpWords(
        _In_ internal::MidiTimestamp const /* timestamp*/,
        _In_ uint32_t const /*word0*/,
        _In_ uint32_t const /*word1*/,
        _In_ uint32_t const /*word2*/,
        _In_ uint32_t const /*word3*/
        )
    {
        throw hresult_not_implemented();
    }

    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmp(
        winrt::Windows::Devices::Midi2::IMidiUmp const& /*ump*/)
    {
        throw hresult_not_implemented();
    }



    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::ActivateMidiStream(
        _In_ com_ptr<IMidiAbstraction> serviceAbstraction,
        _In_ const IID & iid,
        _Out_ void** iface)
    {
        try
        {
            winrt::check_hresult(serviceAbstraction->Activate(iid, iface));

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L"hresult exception activating stream. Service may be unavailable", ex);

            return false;
        }
    }



    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::InternalInitialize(
        _In_ winrt::com_ptr<IMidiAbstraction> serviceAbstraction,
        _In_ winrt::hstring const endpointInstanceId,
        _In_ winrt::hstring const deviceIdInputConnection,
        _In_ winrt::hstring const deviceIdOutputConnection)
    {
        try
        {
            m_id = endpointInstanceId;

            m_inputDeviceId = deviceIdInputConnection;
            m_outputDeviceId = deviceIdOutputConnection;

            WINRT_ASSERT(!InputDeviceId().empty());
            WINRT_ASSERT(!OutputDeviceId().empty());

            WINRT_ASSERT(serviceAbstraction != nullptr);

            m_serviceAbstraction = serviceAbstraction;

            // TODO: Read any settings we need for this endpoint

            return true;
        }
        catch (winrt::hresult_error const& ex)
        {
            internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint.", ex);

            return false;
        }
    }



    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::Open()
    {
        if (!m_isOpen)
        {
            // Activate the endpoint for this device. Will fail if the device is not a BiDi device
            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiIn), (void**)&m_inputEndpointInterface))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate input MIDI Stream");

                return false;
            }

            if (!ActivateMidiStream(m_serviceAbstraction, __uuidof(IMidiOut), (void**)&m_outputEndpointInterface))
            {
                internal::LogGeneralError(__FUNCTION__, L"Could not activate output MIDI Stream");

                return false;
            }



            if (m_outputEndpointInterface != nullptr && m_inputEndpointInterface != nullptr)
            {
                try
                {
                    DWORD mmcssTaskId{};  // TODO: Does this need to be session-wide? Probably, but it can be modified by the endpoint init, so maybe should be endpoint-local

                    winrt::check_hresult(m_inputEndpointInterface->Initialize(
                        (LPCWSTR)(InputDeviceId().c_str()),
                        &mmcssTaskId,
                        (IMidiCallback*)(this)
                    ));

                    winrt::check_hresult(m_outputEndpointInterface->Initialize(
                        (LPCWSTR)(OutputDeviceId().c_str()),
                        &mmcssTaskId
                    ));

                    m_isOpen = true;


                    // TODO: Send discovery messages using app provided settings and user settings read from the property store
                    // These get fired off here quickly so we can return. The listener is responsible for catching them.







                    return true;
                }
                catch (winrt::hresult_error const& ex)
                {
                    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint interface. Service may be unavailable.", ex);

                    m_inputEndpointInterface = nullptr;
                    m_outputEndpointInterface = nullptr;

                    return false;
                }
            }
            else
            {
                internal::LogGeneralError(__FUNCTION__, L" Endpoint interface is nullptr");

                return false;
            }
        }
        else
        {
            // already open. Just return true here. Not fatal in any way, so no error
            return true;
        }
    }



    MidiBidirectionalAggregatedEndpointConnection::~MidiBidirectionalAggregatedEndpointConnection()
    {

    }


}
