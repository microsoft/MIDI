#include "MidiSynth/Articulation.h"
#include "MidiSynth/DlsUnits.h"

namespace MidiSynth
{
    namespace
    {
        constexpr uint16_t ConnSrcNone = 0x0000;
        constexpr uint16_t ConnSrcLfo = 0x0001;
        constexpr uint16_t ConnSrcKeyOnVelocity = 0x0002;
        constexpr uint16_t ConnSrcKeyNumber = 0x0003;
        constexpr uint16_t ConnSrcEg2 = 0x0005;
        constexpr uint16_t ConnSrcCc1 = 0x0081;

        constexpr uint16_t ConnDstAttenuation = 0x0001;
        constexpr uint16_t ConnDstPitch = 0x0003;
        constexpr uint16_t ConnDstPan = 0x0004;
        constexpr uint16_t ConnDstLfoFrequency = 0x0104;
        constexpr uint16_t ConnDstLfoStartDelay = 0x0105;
        constexpr uint16_t ConnDstEg1AttackTime = 0x0206;
        constexpr uint16_t ConnDstEg1DecayTime = 0x0207;
        constexpr uint16_t ConnDstEg1ReleaseTime = 0x0209;
        constexpr uint16_t ConnDstEg1SustainLevel = 0x020A;
        constexpr uint16_t ConnDstEg2AttackTime = 0x030A;
        constexpr uint16_t ConnDstEg2DecayTime = 0x030B;
        constexpr uint16_t ConnDstEg2ReleaseTime = 0x030D;
        constexpr uint16_t ConnDstEg2SustainLevel = 0x030E;

        void ApplyConnection(_In_ const DlsConnection& connection, _Inout_ ResolvedArticulation& articulation) noexcept
        {
            const int32_t scale = connection.Scale;

            switch (connection.Destination)
            {
            case ConnDstLfoFrequency:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.LfoFrequencyHertz = AbsolutePitchUnitsToHertz(scale);
                }
                break;

            case ConnDstLfoStartDelay:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.LfoStartDelaySeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstAttenuation:
                if (connection.Source == ConnSrcLfo)
                {
                    if (connection.Control == ConnSrcCc1)
                    {
                        articulation.LfoModWheelToAttenuationDb = RelativeGainToDb(scale);
                    }
                    else
                    {
                        articulation.LfoToAttenuationDb = RelativeGainToDb(scale);
                    }
                }
                break;

            case ConnDstPitch:
                if (connection.Source == ConnSrcLfo)
                {
                    if (connection.Control == ConnSrcCc1)
                    {
                        articulation.LfoModWheelToPitchCents = PitchUnitsToCents(scale);
                    }
                    else
                    {
                        articulation.LfoToPitchCents = PitchUnitsToCents(scale);
                    }
                }
                else if (connection.Source == ConnSrcEg2)
                {
                    articulation.Eg2ToPitchCents = PitchUnitsToCents(scale);
                }
                break;

            case ConnDstPan:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.PanFraction = PercentUnitsToFraction(scale);
                }
                break;

            case ConnDstEg1AttackTime:
                if (connection.Source == ConnSrcKeyOnVelocity)
                {
                    articulation.Eg1VelocityToAttackTimeCents = scale;
                }
                else if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg1AttackSeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg1DecayTime:
                if (connection.Source == ConnSrcKeyNumber)
                {
                    articulation.Eg1KeyToDecayTimeCents = scale;
                }
                else if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg1DecaySeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg1ReleaseTime:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg1ReleaseSeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg1SustainLevel:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg1SustainFraction = PercentUnitsToFraction(scale);
                }
                break;

            case ConnDstEg2AttackTime:
                if (connection.Source == ConnSrcKeyOnVelocity)
                {
                    articulation.Eg2VelocityToAttackTimeCents = scale;
                }
                else if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg2AttackSeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg2DecayTime:
                if (connection.Source == ConnSrcKeyNumber)
                {
                    articulation.Eg2KeyToDecayTimeCents = scale;
                }
                else if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg2DecaySeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg2ReleaseTime:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg2ReleaseSeconds = TimeCentsToSeconds(scale);
                }
                break;

            case ConnDstEg2SustainLevel:
                if (connection.Source == ConnSrcNone)
                {
                    articulation.Eg2SustainFraction = PercentUnitsToFraction(scale);
                }
                break;

            default:
                break;
            }
        }
    }

    _Use_decl_annotations_
    ResolvedArticulation ResolveArticulation(
        const std::vector<DlsConnection>& instrumentConnections,
        const std::vector<DlsConnection>& regionConnections) noexcept
    {
        ResolvedArticulation articulation;

        for (const auto& connection : instrumentConnections)
        {
            ApplyConnection(connection, articulation);
        }

        for (const auto& connection : regionConnections)
        {
            ApplyConnection(connection, articulation);
        }

        if (articulation.Eg1SustainFraction < 0.0) { articulation.Eg1SustainFraction = 0.0; }
        if (articulation.Eg1SustainFraction > 1.0) { articulation.Eg1SustainFraction = 1.0; }
        if (articulation.Eg2SustainFraction < 0.0) { articulation.Eg2SustainFraction = 0.0; }
        if (articulation.Eg2SustainFraction > 1.0) { articulation.Eg2SustainFraction = 1.0; }
        if (articulation.PanFraction < -0.5) { articulation.PanFraction = -0.5; }
        if (articulation.PanFraction > 0.5) { articulation.PanFraction = 0.5; }

        if (articulation.LfoFrequencyHertz < 0.01) { articulation.LfoFrequencyHertz = 0.01; }
        if (articulation.LfoFrequencyHertz > 100.0) { articulation.LfoFrequencyHertz = 100.0; }

        return articulation;
    }
}
