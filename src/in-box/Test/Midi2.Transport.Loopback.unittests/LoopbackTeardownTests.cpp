// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#include "pch.h"

#include "LoopbackTeardownTests.h"
#include "LoopbackTestSupport.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace LoopbackTest;

namespace
{
    // Shared between the test thread and the worker doing the run, which the test thread may have
    // to abandon.
    struct RemovalRun
    {
        explicit RemovalRun(_In_ bool const removeWhileOpen) : RemoveWhileOpen{ removeWhileOpen } { }

        bool const RemoveWhileOpen;

        // String literals only. The test thread reads this after giving up on a hung run, when
        // nothing else here is safe to read.
        std::atomic<wchar_t const*> Step{ L"starting" };

        // Written by the run, and read only after it has finished.
        HRESULT Result{ S_OK };
        wchar_t const* Message{ L"" };
        std::wstring Detail{ };
    };


    void Fail(_Inout_ RemovalRun& run, _In_ HRESULT const result, _In_z_ wchar_t const* message)
    {
        run.Result = FAILED(result) ? result : E_FAIL;
        run.Message = message;
    }


    void RunRemoval(_Inout_ RemovalRun& run)
    {
        run.Step = L"connecting to the service";

        MidiTest::ServiceSession session{ };

        auto hr = session.Open(L"Loopback teardown test");
        if (FAILED(hr)) { Fail(run, hr, L"Could not register a session with the service."); return; }

        run.Step = L"creating the loopback";

        auto const loopback = CreateLoopback(L"Teardown Test");

        if (!loopback.Created())
        {
            Fail(run, E_FAIL, L"The service did not create the loopback.");
            run.Detail = loopback.Detail;
            return;
        }

        // nothing else removes it, so this has to happen even if a later step fails
        auto removeLoopback = wil::scope_exit([&]
        {
            run.Step = L"removing the loopback after a failed step";
            RemoveLoopback(loopback.AssociationId);
        });

        run.Step = L"opening both sides";

        auto const receivedA = MidiTest::ReceivedMessages::Create(false);
        auto const receivedB = MidiTest::ReceivedMessages::Create(false);

        wil::com_ptr_nothrow<IMidiBidirectional> a{ };
        wil::com_ptr_nothrow<IMidiBidirectional> b{ };

        hr = session.Connect(loopback.EndpointA, receivedA.get(), a);
        if (FAILED(hr)) { Fail(run, hr, L"Could not open side A."); return; }

        hr = session.Connect(loopback.EndpointB, receivedB.get(), b);
        if (FAILED(hr)) { a->Shutdown(); Fail(run, hr, L"Could not open side B."); return; }

        removeLoopback.release();

        if (run.RemoveWhileOpen)
        {
            // The step under test. The service has to take both endpoints away from the open
            // connections, and this is the call which never returns if it hangs doing that.
            run.Step = L"removing the loopback while both sides are open";

            auto const removed = RemoveLoopback(loopback.AssociationId);

            if (!removed.CallSucceeded)
            {
                Fail(run, E_FAIL, L"The service did not remove the loopback.");
                run.Detail = removed.Message + L" " + removed.ResponseJson;
            }

            // the service has already taken these endpoints away, so only returning matters here
            run.Step = L"closing both sides after the loopback was removed";
            a->Shutdown();
            b->Shutdown();
        }
        else
        {
            run.Step = L"closing both sides";
            a->Shutdown();
            b->Shutdown();

            run.Step = L"removing the loopback";

            auto const removed = RemoveLoopback(loopback.AssociationId);

            if (!removed.CallSucceeded)
            {
                Fail(run, E_FAIL, L"The service did not remove the loopback.");
                run.Detail = removed.Message + L" " + removed.ResponseJson;
            }
        }

        run.Step = L"closing the session";
        session.Close();
    }


    std::shared_ptr<RemovalRun> RunGuardedRemoval(
        _In_ bool const removeWhileOpen,
        _Out_ MidiTest::GuardedRunResult& guarded)
    {
        auto const run = std::make_shared<RemovalRun>(removeWhileOpen);

        guarded = MidiTest::RunWithHangGuard([run] { RunRemoval(*run); });

        return run;
    }
}


bool LoopbackTeardownTests::TestSetup()
{
    m_deviceNodeTracker.Start();

    return true;
}

bool LoopbackTeardownTests::TestCleanup()
{
    m_deviceNodeTracker.RemoveDeviceNodesCreatedSinceStart();

    return true;
}


void LoopbackTeardownTests::TestRemovingLoopbackWithBothSidesOpenDoesNotHangService()
{
    if (!LoopbackAvailable())
    {
        Log::Result(TestResults::Skipped, L"Loopback transport is not available.");
        return;
    }

    MidiTest::GuardedRunResult guarded{ };
    auto const run = RunGuardedRemoval(true, guarded);

    if (!guarded.Finished)
    {
        VERIFY_FAIL(String().Format(
            L"The service did not return within %u seconds while %s, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
            MidiTest::HangTimeoutMilliseconds / 1000,
            run->Step.load()));

        return;
    }

    VERIFY_SUCCEEDED(guarded.Result, String().Format(L"Exception while %s", run->Step.load()));
    VERIFY_SUCCEEDED(run->Result, String().Format(L"%s (while %s) %s", run->Message, run->Step.load(), run->Detail.c_str()));

    // The removal can return while another service thread is still stuck, so the service also has
    // to be able to add and remove another loopback afterwards.
    MidiTest::GuardedRunResult probeGuarded{ };
    auto const probe = RunGuardedRemoval(false, probeGuarded);

    if (!probeGuarded.Finished)
    {
        VERIFY_FAIL(String().Format(
            L"After the first loopback was removed, the service did not return within %u seconds while %s for another loopback, so it is hung. MIDI will not work on this PC until the MIDI service is restarted.",
            MidiTest::HangTimeoutMilliseconds / 1000,
            probe->Step.load()));

        return;
    }

    VERIFY_SUCCEEDED(probeGuarded.Result, String().Format(L"Exception while %s", probe->Step.load()));
    VERIFY_SUCCEEDED(probe->Result, String().Format(L"Another loopback: %s (while %s) %s", probe->Message, probe->Step.load(), probe->Detail.c_str()));
}
