#pragma once
#include "MidiServiceConfigResponse.g.h"

namespace winrt::Windows::Devices::Midi2::ServiceConfig::implementation
{
    struct MidiServiceConfigResponse : MidiServiceConfigResponseT<MidiServiceConfigResponse>
    {
        MidiServiceConfigResponse() = default;

        svc::MidiServiceConfigResponseStatus Status() const noexcept { return m_status; }
        uint32_t ServiceErrorCode() const noexcept { return m_serviceErrorCode; }
        winrt::hstring ServiceErrorMessage() const noexcept { return m_serviceErrorMessage; }
        json::JsonObject ResponseJson() const noexcept { return m_responseJson; }


        void InternalSetStatus(_In_ svc::MidiServiceConfigResponseStatus const& status);
        void InternalSetServiceError(_In_ uint32_t const& serviceErrorCode, _In_ winrt::hstring const& serviceErrorMessage, _In_ json::JsonObject const& responseJson);
        void InternalSetServiceSuccess(_In_ json::JsonObject const& responseJson);

    private:
        svc::MidiServiceConfigResponseStatus m_status{ svc::MidiServiceConfigResponseStatus::ErrorNotImplemented };
        uint32_t m_serviceErrorCode{};
        winrt::hstring m_serviceErrorMessage{};
        json::JsonObject m_responseJson{ nullptr };

    };
}
