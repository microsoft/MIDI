// Collapses a DLS connection block list into the parameters the voice engine actually uses.
// Unspecified connections take the defaults from Table 2 of the DLS Level 1 specification.

#pragma once

#include "DlsTypes.h"

#include <sal.h>

#include <vector>

namespace MidiSynth
{
    struct ResolvedArticulation
    {
        double Eg1AttackSeconds{ 0.0 };
        double Eg1DecaySeconds{ 0.0 };
        double Eg1SustainFraction{ 1.0 };
        double Eg1ReleaseSeconds{ 0.0 };
        int32_t Eg1VelocityToAttackTimeCents{ 0 };
        int32_t Eg1KeyToDecayTimeCents{ 0 };

        double Eg2AttackSeconds{ 0.0 };
        double Eg2DecaySeconds{ 0.0 };
        double Eg2SustainFraction{ 1.0 };
        double Eg2ReleaseSeconds{ 0.0 };
        int32_t Eg2VelocityToAttackTimeCents{ 0 };
        int32_t Eg2KeyToDecayTimeCents{ 0 };
        double Eg2ToPitchCents{ 0.0 };

        double LfoFrequencyHertz{ 5.0 };
        double LfoStartDelaySeconds{ 0.01 };
        double LfoToAttenuationDb{ 0.0 };
        double LfoToPitchCents{ 0.0 };
        double LfoModWheelToAttenuationDb{ 0.0 };
        double LfoModWheelToPitchCents{ 0.0 };

        // -0.5 is hard left, +0.5 is hard right.
        double PanFraction{ 0.0 };
    };

    // Region connections are applied after instrument connections, so a drum region overrides
    // the instrument level articulation it inherits.
    ResolvedArticulation ResolveArticulation(
        _In_ const std::vector<DlsConnection>& instrumentConnections,
        _In_ const std::vector<DlsConnection>& regionConnections) noexcept;
}
