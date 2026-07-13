#pragma once
#include "Transports.BasicLoopback.MidiBasicLoopbackUpdateResult.g.h"

namespace winrt::Windows::Devices::Midi2::Transports::BasicLoopback::implementation
{
    struct MidiBasicLoopbackUpdateResult : MidiBasicLoopbackUpdateResultT<MidiBasicLoopbackUpdateResult>
    {
        MidiBasicLoopbackUpdateResult() = default;

        bool Success() const noexcept { return m_success; };
        bloop::MidiBasicLoopbackErrorCode ErrorCode() const noexcept { return m_errorCode; }
        winrt::hstring ErrorMessage() const noexcept { return m_errorMessage; }


        void InternalSetSuccess()
        {
            m_success = true;
        }

        void InternalSetFailure(_In_ bloop::MidiBasicLoopbackErrorCode const errorCode, _In_ winrt::hstring const& errorMessage)
        {
            m_success = false;
            m_errorCode = errorCode;
            m_errorMessage = errorMessage;
        }

    private:
        bool m_success{ false };
        bloop::MidiBasicLoopbackErrorCode m_errorCode{ bloop::MidiBasicLoopbackErrorCode::NoErrorInformationAvailable };
        winrt::hstring m_errorMessage{ };
    };
}
