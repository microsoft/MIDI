#include "pch.h"
#include "MidiBidirectionalAggregatedEndpointConnection.h"
#include "MidiBidirectionalAggregatedEndpointConnection.g.cpp"

namespace winrt::Windows::Devices::Midi2::implementation
{


    // Callback handler from the Midi Service endpoint abstraction

    IFACEMETHODIMP MidiBidirectionalAggregatedEndpointConnection::Callback(_In_ PVOID /*data*/, _In_ UINT /*size*/, _In_ LONGLONG /*timestamp*/)
    {
        //try
        //{
        //    // one copy of the event args for this gets sent to all listeners and the main event
        //    auto args = winrt::make_self<MidiMessageReceivedEventArgs>(Data, Size, Timestamp);

        //    // we failed to create the event args
        //    if (args == nullptr)
        //    {
        //        internal::LogGeneralError(__FUNCTION__, L"Unable to create MidiMessageReceivedEventArgs");

        //        return E_FAIL;
        //    }

        //    bool skipMainMessageReceivedEvent = false;
        //    bool skipFurtherListeners = false;

        //    // If any listeners are hooked up, use them

        //    if (m_messageListeners && m_messageListeners.Size() > 0)
        //    {
        //        // loop through listeners
        //        for (const auto& listener : m_messageListeners)
        //        {
        //            // TODO: this is synchronous by design, but that requires the client to not block
        //            listener.ProcessIncomingMessage(*args, skipFurtherListeners, skipMainMessageReceivedEvent);

        //            // if the listener has told us to skip further listeners, effectively 
        //            // removing this message from the queue, then break out of the loop
        //            if (skipFurtherListeners) break;
        //        }
        //    }

        //    // if the main message received event is hooked up, and we're not skipping it, use it
        //    if (m_messageReceivedEvent && !skipMainMessageReceivedEvent)
        //    {
        //        m_messageReceivedEvent(*this, *args);
        //    }

            return S_OK;
        //}
        //catch (winrt::hresult_error const& ex)
        //{
        //    internal::LogHresultError(__FUNCTION__, L"hresult exception calling message handlers", ex);

        //    return E_FAIL;
        //}
    }


    _Success_(return == true)
    bool MidiBidirectionalAggregatedEndpointConnection::SendUmp(
        winrt::Windows::Devices::Midi2::IMidiUmp const& /*ump*/)
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
    bool MidiBidirectionalAggregatedEndpointConnection::Open()
    {
        throw hresult_not_implemented();
    }


    _Success_(return == true)
        bool MidiBidirectionalAggregatedEndpointConnection::InternalInitialize(
            _In_ winrt::com_ptr<IMidiAbstraction> /*serviceAbstraction*/,
            _In_ winrt::hstring const /*endpointInstanceId*/,
            _In_ winrt::hstring const /*deviceIdInputConnection*/,
            _In_ winrt::hstring const /*deviceIdOutputConnection*/)
    {
        //try
        //{
        //    m_id = endpointId;
        //    m_deviceId = deviceId;

        //    WINRT_ASSERT(!DeviceId().empty());
        //    WINRT_ASSERT(serviceAbstraction != nullptr);

        //    m_serviceAbstraction = serviceAbstraction;

        //    // TODO: Read any settings we need for this endpoint

        //    return true;
        //}
        //catch (winrt::hresult_error const& ex)
        //{
        //    internal::LogHresultError(__FUNCTION__, L" hresult exception initializing endpoint.", ex);

        //    return false;
        //}
    }


    MidiBidirectionalAggregatedEndpointConnection::~MidiBidirectionalAggregatedEndpointConnection()
    {

    }


}
