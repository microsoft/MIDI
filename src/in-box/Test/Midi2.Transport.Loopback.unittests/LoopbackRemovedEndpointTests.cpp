// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include <Feature_Servicing_MIDI2LoopbackRejectRemovedEndpoint.h>

#include "LoopbackRemovedEndpointTests.h"
#include "LoopbackTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace LoopbackTest;

namespace
{
    constexpr DWORD InterfaceDisabledWaitMilliseconds{ 5000 };
    constexpr DWORD InterfaceDisabledPollMilliseconds{ 100 };

    // Asked of PnP directly, not of the service.
    bool EndpointInterfaceIsEnabled(_In_ std::wstring const& endpointDeviceId) noexcept
    {
        try
        {
            auto const info = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                endpointDeviceId,
                winrt::single_threaded_vector<winrt::hstring>(),
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface).get();

            return info != nullptr && info.IsEnabled();
        }
        catch (...)
        {
            return false;
        }
    }

    // Removal returns before PnP has caught up, and the service decides from what PnP reports.
    bool WaitForEndpointInterfaceDisabled(_In_ std::wstring const& endpointDeviceId) noexcept
    {
        for (DWORD waited = 0; waited < InterfaceDisabledWaitMilliseconds; waited += InterfaceDisabledPollMilliseconds)
        {
            if (!EndpointInterfaceIsEnabled(endpointDeviceId))
            {
                return true;
            }

            Sleep(InterfaceDisabledPollMilliseconds);
        }

        return !EndpointInterfaceIsEnabled(endpointDeviceId);
    }
}


bool LoopbackRemovedEndpointTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool LoopbackRemovedEndpointTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void LoopbackRemovedEndpointTests::TestOpeningAnEndpointOfARemovedLoopbackFailsWithNotFound()
{
    if (!Feature_Servicing_MIDI2LoopbackRejectRemovedEndpoint::IsEnabled())
    {
        Log::Result(TestResults::Skipped, L"Feature_Servicing_MIDI2LoopbackRejectRemovedEndpoint is disabled.");
        return;
    }

    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    MidiTest::MultithreadedApartment apartment{ };

    auto const loopback = CreateLoopback(L"Removed Endpoint Test");
    VERIFY_IS_TRUE(loopback.Created(), loopback.Detail.c_str());

    auto const removed = RemoveLoopback(loopback.AssociationId);
    VERIFY_IS_TRUE(removed.IsSuccess(), (removed.Message + L" " + removed.ResponseJson).c_str());

    VERIFY_IS_TRUE(WaitForEndpointInterfaceDisabled(loopback.EndpointA), L"endpoint A's interface was still enabled after the loopback was removed");

    MidiTest::ServiceSession session{ };
    VERIFY_SUCCEEDED(session.Open(L"Loopback removed endpoint test"));

    auto received = MidiTest::ReceivedMessages::Create();
    VERIFY_IS_NOT_NULL(received.get());

    wil::com_ptr_nothrow<IMidiBidirectional> connection{ };

    auto const hr = session.Connect(loopback.EndpointA, received.get(), connection);

    Log::Comment(String().Format(L"Open of removed endpoint A: 0x%08X", hr));

    if (SUCCEEDED(hr))
    {
        MidiTest::CloseConnectionWithHangGuard(connection);
    }

    VERIFY_ARE_EQUAL(hr, HRESULT_FROM_WIN32(ERROR_NOT_FOUND), L"open of an endpoint whose loopback was removed");
}
