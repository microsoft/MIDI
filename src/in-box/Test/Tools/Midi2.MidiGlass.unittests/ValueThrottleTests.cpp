// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "ValueThrottleTests.h"

#include "ValueThrottle.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

void ValueThrottleTests::SendsEverythingWhenThereIsNoLimit()
{
    glass::ValueThrottle throttle{};

    // Buttons, notes, sequences and system exclusive run unthrottled. Rate limiting a note on
    // would be a defect, not a feature.
    for (uint64_t now = 0; now < 10; ++now)
    {
        VERIFY_IS_TRUE(throttle.ShouldSend(now / 10.0, now));
    }

    VERIFY_ARE_EQUAL(uint32_t{ 0 }, throttle.SuppressedCount());

    double trailing{ -1.0 };
    VERIFY_IS_FALSE(throttle.Release(trailing));
}

void ValueThrottleTests::TheFirstMoveAlwaysGoes()
{
    glass::ValueThrottle throttle{};
    throttle.SetMinimumInterval(10);

    // However heavy the rate limit, a control has to respond the instant it is touched.
    VERIFY_IS_TRUE(throttle.ShouldSend(0.1, 1000));
}

void ValueThrottleTests::HoldsBackMovesInsideTheInterval()
{
    glass::ValueThrottle throttle{};
    throttle.SetMinimumInterval(10);

    VERIFY_IS_TRUE(throttle.ShouldSend(0.0, 1000));

    VERIFY_IS_FALSE(throttle.ShouldSend(0.1, 1003));
    VERIFY_IS_FALSE(throttle.ShouldSend(0.2, 1006));
    VERIFY_IS_FALSE(throttle.ShouldSend(0.3, 1009));

    // exactly on the boundary counts as due
    VERIFY_IS_TRUE(throttle.ShouldSend(0.4, 1010));

    VERIFY_ARE_EQUAL(uint32_t{ 3 }, throttle.SuppressedCount());
}

void ValueThrottleTests::TheLastValueIsAlwaysSent()
{
    glass::ValueThrottle throttle{};
    throttle.SetMinimumInterval(10);

    VERIFY_IS_TRUE(throttle.ShouldSend(0.0, 1000));
    VERIFY_IS_FALSE(throttle.ShouldSend(0.9, 1002));

    // This is the rule that gets forgotten and it is the one that matters. Without it the fader
    // on screen says 0.9 and the desk is still at 0.0, for the rest of the session.
    double trailing{ -1.0 };
    VERIFY_IS_TRUE(throttle.Release(trailing));
    VERIFY_ARE_EQUAL(0.9, trailing);
}

void ValueThrottleTests::DoesNotRepeatAValueThatAlreadyWentOut()
{
    glass::ValueThrottle throttle{};
    throttle.SetMinimumInterval(10);

    VERIFY_IS_TRUE(throttle.ShouldSend(0.5, 1000));

    // The last move was the one that went out, so there is nothing owing. Sending it again is
    // harmless but it is noise on a wire that is rate limited because it has none to spare.
    double trailing{ -1.0 };
    VERIFY_IS_FALSE(throttle.Release(trailing));
}

void ValueThrottleTests::AFullDragEndsOnWhereTheFingerLeftIt()
{
    glass::ValueThrottle throttle{};
    throttle.SetMinimumInterval(10);

    double lastSent{ -1.0 };
    double finalPosition{ 0.0 };

    // A two hundred step drag at one step per millisecond, which is what a high rate digitizer
    // actually delivers.
    for (uint64_t step = 0; step < 200; ++step)
    {
        finalPosition = step / 199.0;

        if (throttle.ShouldSend(finalPosition, 1000 + step))
        {
            lastSent = finalPosition;
        }
    }

    double trailing{ -1.0 };

    if (throttle.Release(trailing))
    {
        lastSent = trailing;
    }

    Log::Comment(String().Format(L"200 moves, %u suppressed, ended at %.4f",
        throttle.SuppressedCount(), lastSent));

    // The throttle must have done something, or the test proves nothing.
    VERIFY_IS_GREATER_THAN(throttle.SuppressedCount(), uint32_t{ 100 });

    // and the wire has to agree with the surface exactly
    VERIFY_ARE_EQUAL(finalPosition, lastSent);
}
