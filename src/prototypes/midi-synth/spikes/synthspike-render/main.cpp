// Spike: render a score offline to a WAV file, with no audio device and no timing, so the
// engine can be judged and regression tested deterministically.

#include "MidiSynth/ChorusEffect.h"
#include "MidiSynth/ReverbEffect.h"
#include "MidiSynth/SynthEngine.h"
#include "MidiSynth/UmpDispatcher.h"

#include <windows.h>

#include <algorithm>
#include <complex>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace MidiSynth;

namespace
{
    enum class EventKind
    {
        NoteOn,
        NoteOff,
        ControlChange,
        ProgramChange,
        PitchBend,
    };

    struct TimedEvent
    {
        double Seconds{ 0.0 };
        EventKind Kind{ EventKind::NoteOn };
        uint8_t Channel{ 0 };
        uint8_t Data1{ 0 };
        int32_t Data2{ 0 };
    };

    std::string ToUtf8(_In_ const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int required = WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), required, nullptr, nullptr);

        return result;
    }

    std::wstring DefaultDlsPath()
    {
        wchar_t systemDirectory[MAX_PATH]{};

        if (GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory)) == 0)
        {
            return L"gm.dls";
        }

        return std::wstring(systemDirectory) + L"\\drivers\\gm.dls";
    }

    void AddNote(
        _Inout_ std::vector<TimedEvent>& events,
        _In_ double startSeconds,
        _In_ uint8_t channel,
        _In_ uint8_t note,
        _In_ uint8_t velocity,
        _In_ double durationSeconds)
    {
        events.push_back({ startSeconds, EventKind::NoteOn, channel, note, velocity });
        events.push_back({ startSeconds + durationSeconds, EventKind::NoteOff, channel, note, 0 });
    }

    // Something recognizable that exercises melodic voices, pitch bend, sustain and drums.
    std::vector<TimedEvent> BuildDemoScore()
    {
        std::vector<TimedEvent> events;

        events.push_back({ 0.0, EventKind::ProgramChange, 0, 0, 0 });    // acoustic grand
        events.push_back({ 0.0, EventKind::ProgramChange, 1, 48, 0 });   // string ensemble
        events.push_back({ 0.0, EventKind::ControlChange, 1, 7, 80 });

        const uint8_t chord[] = { 60, 64, 67, 72 };
        double time = 0.25;

        for (const auto note : chord)
        {
            AddNote(events, time, 0, note, 100, 2.0);
            time += 0.18;
        }

        for (const auto note : chord)
        {
            AddNote(events, 1.6, 1, static_cast<uint8_t>(note - 12), 70, 2.4);
        }

        // Pitch bend sweep on a lead.
        events.push_back({ 3.2, EventKind::ProgramChange, 2, 80, 0 });
        AddNote(events, 3.2, 2, 72, 110, 1.4);

        for (int i = 0; i <= 20; i++)
        {
            const double t = 3.2 + i * 0.06;
            const int32_t bend = 8192 + static_cast<int32_t>(i * 180);
            events.push_back({ t, EventKind::PitchBend, 2, 0, bend });
        }

        events.push_back({ 4.7, EventKind::PitchBend, 2, 0, 8192 });

        // Drums on channel 10, including the hi hat choke pair.
        const uint8_t kick = 36;
        const uint8_t snare = 38;
        const uint8_t closedHat = 42;
        const uint8_t openHat = 46;

        for (int beat = 0; beat < 16; beat++)
        {
            const double t = 4.8 + beat * 0.25;

            if (beat % 4 == 0) { AddNote(events, t, 9, kick, 110, 0.2); }
            if (beat % 4 == 2) { AddNote(events, t, 9, snare, 100, 0.2); }

            AddNote(events, t, 9, (beat % 8 == 7) ? openHat : closedHat, 80, 0.2);
        }

        return events;
    }

    bool TryParseScore(_In_ const std::wstring& path, _Out_ std::vector<TimedEvent>& events)
    {
        events.clear();

        std::ifstream file(path);

        if (!file.is_open())
        {
            return false;
        }

        std::string line;

        while (std::getline(file, line))
        {
            const auto hash = line.find('#');

            if (hash != std::string::npos)
            {
                line.erase(hash);
            }

            std::istringstream parser(line);
            std::string command;

            if (!(parser >> command))
            {
                continue;
            }

            if (command == "note")
            {
                double start = 0.0;
                double duration = 0.0;
                int channel = 0;
                int note = 0;
                int velocity = 0;

                if (parser >> start >> channel >> note >> velocity >> duration)
                {
                    AddNote(events, start, static_cast<uint8_t>(channel), static_cast<uint8_t>(note),
                        static_cast<uint8_t>(velocity), duration);
                }
            }
            else if (command == "program")
            {
                double start = 0.0;
                int channel = 0;
                int bankMsb = 0;
                int bankLsb = 0;
                int program = 0;

                if (parser >> start >> channel >> bankMsb >> bankLsb >> program)
                {
                    events.push_back({ start, EventKind::ControlChange, static_cast<uint8_t>(channel), 0, bankMsb });
                    events.push_back({ start, EventKind::ControlChange, static_cast<uint8_t>(channel), 32, bankLsb });
                    events.push_back({ start, EventKind::ProgramChange, static_cast<uint8_t>(channel),
                        static_cast<uint8_t>(program), 0 });
                }
            }
            else if (command == "cc")
            {
                double start = 0.0;
                int channel = 0;
                int controller = 0;
                int value = 0;

                if (parser >> start >> channel >> controller >> value)
                {
                    events.push_back({ start, EventKind::ControlChange, static_cast<uint8_t>(channel),
                        static_cast<uint8_t>(controller), value });
                }
            }
            else if (command == "bend")
            {
                double start = 0.0;
                int channel = 0;
                int value = 0;

                if (parser >> start >> channel >> value)
                {
                    events.push_back({ start, EventKind::PitchBend, static_cast<uint8_t>(channel), 0, value });
                }
            }
        }

        return true;
    }

    bool WriteWaveFile(
        _In_ const std::wstring& path,
        _In_ const std::vector<float>& interleavedStereo,
        _In_ uint32_t sampleRate)
    {
        std::ofstream file(path, std::ios::binary);

        if (!file.is_open())
        {
            return false;
        }

        const uint32_t frameCount = static_cast<uint32_t>(interleavedStereo.size() / 2);
        const uint16_t channels = 2;
        const uint16_t bitsPerSample = 16;
        const uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
        const uint16_t blockAlign = channels * (bitsPerSample / 8);
        const uint32_t dataBytes = frameCount * blockAlign;

        auto writeUInt32 = [&file](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); };
        auto writeUInt16 = [&file](uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); };

        file.write("RIFF", 4);
        writeUInt32(36 + dataBytes);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        writeUInt32(16);
        writeUInt16(1);
        writeUInt16(channels);
        writeUInt32(sampleRate);
        writeUInt32(byteRate);
        writeUInt16(blockAlign);
        writeUInt16(bitsPerSample);
        file.write("data", 4);
        writeUInt32(dataBytes);

        std::vector<int16_t> pcm(interleavedStereo.size());

        for (size_t i = 0; i < interleavedStereo.size(); i++)
        {
            const float clamped = (std::clamp)(interleavedStereo[i], -1.0f, 1.0f);
            pcm[i] = static_cast<int16_t>(clamped * 32767.0f);
        }

        file.write(reinterpret_cast<const char*>(pcm.data()), static_cast<std::streamsize>(pcm.size() * sizeof(int16_t)));

        return file.good();
    }

    void DispatchEvent(_Inout_ SynthEngine& engine, _In_ const TimedEvent& event)
    {
        switch (event.Kind)
        {
        case EventKind::NoteOn:
            // Scale a 7 bit velocity across the full 16 bit range the engine accepts.
            engine.NoteOn(event.Channel, event.Data1,
                static_cast<uint16_t>((event.Data2 << 9) | (event.Data2 << 2) | (event.Data2 >> 5)));
            break;

        case EventKind::NoteOff:
            engine.NoteOff(event.Channel, event.Data1);
            break;

        case EventKind::ControlChange:
            engine.ControlChange(event.Channel, event.Data1, static_cast<uint8_t>(event.Data2));
            break;

        case EventKind::ProgramChange:
            engine.ProgramChange(event.Channel, event.Data1);
            break;

        case EventKind::PitchBend:
            engine.PitchBend(event.Channel, event.Data2);
            break;
        }
    }

    // Normalized autocorrelation with parabolic peak refinement. Harmonically rich samples still
    // repeat at the fundamental period, so this measures pitch rather than spectral content.
    double MeasureFundamentalHertz(
        _In_reads_(count) const float* mono,
        _In_ size_t count,
        _In_ double sampleRate,
        _In_ double expectedHertz)
    {
        const double expectedPeriod = sampleRate / expectedHertz;

        const auto minLag = (std::max)(2, static_cast<int>(expectedPeriod * 0.6));
        const auto maxLag = (std::min)(static_cast<int>(count / 2), static_cast<int>(expectedPeriod * 1.7));

        if (maxLag <= minLag)
        {
            return 0.0;
        }

        std::vector<double> scores(static_cast<size_t>(maxLag - minLag + 1), 0.0);

        for (int lag = minLag; lag <= maxLag; lag++)
        {
            double correlation = 0.0;
            double energyA = 0.0;
            double energyB = 0.0;

            for (size_t i = 0; i + static_cast<size_t>(lag) < count; i++)
            {
                const double a = mono[i];
                const double b = mono[i + static_cast<size_t>(lag)];

                correlation += a * b;
                energyA += a * a;
                energyB += b * b;
            }

            scores[static_cast<size_t>(lag - minLag)] = correlation / (std::sqrt(energyA * energyB) + 1e-12);
        }

        size_t bestIndex = 0;

        for (size_t i = 1; i < scores.size(); i++)
        {
            if (scores[i] > scores[bestIndex])
            {
                bestIndex = i;
            }
        }

        double refinedLag = static_cast<double>(minLag + static_cast<int>(bestIndex));

        if (bestIndex > 0 && bestIndex + 1 < scores.size())
        {
            const double previous = scores[bestIndex - 1];
            const double current = scores[bestIndex];
            const double next = scores[bestIndex + 1];
            const double denominator = previous - 2.0 * current + next;

            if (std::abs(denominator) > 1e-12)
            {
                refinedLag += 0.5 * (previous - next) / denominator;
            }
        }

        return (refinedLag > 0.0) ? sampleRate / refinedLag : 0.0;
    }

    // Renders one note in isolation and measures how far its pitch lands from equal temperament.
    // This exercises region selection, the wsmp unity note and fine tune, and the sample rate
    // ratio all at once, so a units mistake anywhere shows up here as tens of cents.
    int RunTuningTest(_In_ const DlsCollection& collection, _In_ const SynthConfig& config, _In_ uint8_t program)
    {
        const uint32_t sampleRate = config.RenderSampleRate();
        const double toleranceCents = 10.0;

        printf("Tuning test, program %u\n\n", program);
        printf("  %-5s %-10s %-12s %-10s %-10s %s\n",
            "note", "expected", "measured", "engine", "analyzer", "");

        double worstError = 0.0;
        double worstAnalyzerError = 0.0;
        int measured = 0;
        int failures = 0;

        for (uint8_t note = 36; note <= 96; note = static_cast<uint8_t>(note + 4))
        {
            SynthEngine engine;

            if (!engine.Initialize(&collection, config))
            {
                printf("engine initialization failed\n");
                return 1;
            }

            engine.ProgramChange(0, program);
            engine.NoteOn(0, note, 40000);

            const auto totalFrames = static_cast<uint32_t>(0.45 * sampleRate);
            std::vector<float> stereo(static_cast<size_t>(totalFrames) * 2, 0.0f);

            uint32_t rendered = 0;

            while (rendered < totalFrames)
            {
                const uint32_t frames = (std::min)(256u, totalFrames - rendered);
                engine.Render(stereo.data() + static_cast<size_t>(rendered) * 2, frames);
                rendered += frames;
            }

            // Analyze after the attack has settled.
            const auto analysisStart = static_cast<size_t>(0.15 * sampleRate);
            const size_t analysisLength = (std::min)(static_cast<size_t>(8192), totalFrames - analysisStart);

            std::vector<float> mono(analysisLength);
            double energy = 0.0;

            for (size_t i = 0; i < analysisLength; i++)
            {
                mono[i] = 0.5f * (stereo[(analysisStart + i) * 2] + stereo[(analysisStart + i) * 2 + 1]);
                energy += static_cast<double>(mono[i]) * mono[i];
            }

            const double expectedHertz = 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);

            if (energy < 1e-9)
            {
                printf("  %-5u %-10.2f %-12s %-10s %-10s silent\n", note, expectedHertz, "-", "-", "-");
                continue;
            }

            const double measuredHertz =
                MeasureFundamentalHertz(mono.data(), mono.size(), sampleRate, expectedHertz);

            if (measuredHertz <= 0.0)
            {
                printf("  %-5u %-10.2f %-12s %-10s %-10s no pitch\n", note, expectedHertz, "-", "-", "-");
                continue;
            }

            // Run the analyzer against a synthetic sine at the same frequency, so its own bias is
            // reported separately rather than being attributed to the engine.
            std::vector<float> reference(analysisLength);

            for (size_t i = 0; i < analysisLength; i++)
            {
                reference[i] = static_cast<float>(
                    std::sin(2.0 * 3.14159265358979323846 * expectedHertz * i / sampleRate));
            }

            const double referenceHertz =
                MeasureFundamentalHertz(reference.data(), reference.size(), sampleRate, expectedHertz);

            const double analyzerErrorCents =
                (referenceHertz > 0.0) ? 1200.0 * std::log2(referenceHertz / expectedHertz) : 0.0;

            const double rawErrorCents = 1200.0 * std::log2(measuredHertz / expectedHertz);
            const double errorCents = rawErrorCents - analyzerErrorCents;

            measured++;
            worstError = (std::max)(worstError, std::abs(errorCents));
            worstAnalyzerError = (std::max)(worstAnalyzerError, std::abs(analyzerErrorCents));

            const bool failed = std::abs(errorCents) > toleranceCents;

            if (failed)
            {
                failures++;
            }

            printf("  %-5u %-10.2f %-12.2f %+-10.1f %+-10.1f %s\n",
                note, expectedHertz, measuredHertz, errorCents, analyzerErrorCents, failed ? "FAIL" : "");
        }

        printf("\n  notes measured         %d\n", measured);
        printf("  worst engine error     %.1f cents\n", worstError);
        printf("  worst analyzer bias    %.1f cents\n", worstAnalyzerError);
        printf("  tolerance              %.1f cents\n", toleranceCents);
        printf("\n  %s\n", (failures == 0 && measured > 0) ? "PASS" : "FAIL");

        return (failures == 0 && measured > 0) ? 0 : 1;
    }

    struct LoadedWave
    {
        uint32_t SampleRate{ 0 };
        uint16_t Channels{ 0 };
        std::vector<float> Interleaved;
    };

    bool TryLoadWaveFile(_In_ const std::wstring& path, _Out_ LoadedWave& wave)
    {
        wave = {};

        std::ifstream file(path, std::ios::binary);

        if (!file.is_open())
        {
            return false;
        }

        std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        if (bytes.size() < 44 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
            std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
        {
            return false;
        }

        size_t offset = 12;
        uint16_t bitsPerSample = 0;
        const char* data = nullptr;
        uint32_t dataBytes = 0;

        while (offset + 8 <= bytes.size())
        {
            char id[5]{};
            std::memcpy(id, bytes.data() + offset, 4);

            uint32_t chunkSize = 0;
            std::memcpy(&chunkSize, bytes.data() + offset + 4, 4);

            const size_t payload = offset + 8;

            if (payload + chunkSize > bytes.size())
            {
                break;
            }

            if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16)
            {
                std::memcpy(&wave.Channels, bytes.data() + payload + 2, 2);
                std::memcpy(&wave.SampleRate, bytes.data() + payload + 4, 4);
                std::memcpy(&bitsPerSample, bytes.data() + payload + 14, 2);
            }
            else if (std::memcmp(id, "data", 4) == 0)
            {
                data = bytes.data() + payload;
                dataBytes = chunkSize;
            }

            offset = payload + chunkSize + (chunkSize & 1u);
        }

        if (data == nullptr || bitsPerSample != 16 || wave.Channels == 0)
        {
            return false;
        }

        const size_t sampleCount = dataBytes / 2;
        wave.Interleaved.resize(sampleCount);

        for (size_t i = 0; i < sampleCount; i++)
        {
            int16_t value = 0;
            std::memcpy(&value, data + i * 2, 2);
            wave.Interleaved[i] = value / 32768.0f;
        }

        return true;
    }

    void ForwardFft(_Inout_ std::vector<std::complex<double>>& values)
    {
        const size_t count = values.size();

        for (size_t i = 1, j = 0; i < count; i++)
        {
            size_t bit = count >> 1;

            for (; j & bit; bit >>= 1)
            {
                j ^= bit;
            }

            j ^= bit;

            if (i < j)
            {
                std::swap(values[i], values[j]);
            }
        }

        for (size_t length = 2; length <= count; length <<= 1)
        {
            const double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(length);

            for (size_t i = 0; i < count; i += length)
            {
                for (size_t k = 0; k < length / 2; k++)
                {
                    const std::complex<double> twiddle(std::cos(angle * k), std::sin(angle * k));
                    const std::complex<double> even = values[i + k];
                    const std::complex<double> odd = values[i + k + length / 2] * twiddle;

                    values[i + k] = even + odd;
                    values[i + k + length / 2] = even - odd;
                }
            }
        }
    }

    // Average magnitude spectrum over Hann windowed frames.
    std::vector<double> AverageSpectrum(_In_ const std::vector<float>& mono, _In_ size_t frameSize)
    {
        std::vector<double> spectrum(frameSize / 2, 0.0);

        if (mono.size() < frameSize)
        {
            return spectrum;
        }

        const size_t hop = frameSize / 2;
        size_t frames = 0;

        for (size_t start = 0; start + frameSize <= mono.size(); start += hop)
        {
            std::vector<std::complex<double>> buffer(frameSize);

            for (size_t i = 0; i < frameSize; i++)
            {
                const double window =
                    0.5 * (1.0 - std::cos(2.0 * 3.14159265358979323846 * i / (frameSize - 1)));
                buffer[i] = mono[start + i] * window;
            }

            ForwardFft(buffer);

            for (size_t i = 0; i < spectrum.size(); i++)
            {
                spectrum[i] += std::abs(buffer[i]);
            }

            frames++;
        }

        if (frames > 0)
        {
            for (auto& value : spectrum)
            {
                value /= static_cast<double>(frames);
            }
        }

        return spectrum;
    }

    double BandEnergyDb(
        _In_ const std::vector<double>& spectrum,
        _In_ uint32_t sampleRate,
        _In_ double lowHertz,
        _In_ double highHertz)
    {
        const double binWidth = static_cast<double>(sampleRate) / (spectrum.size() * 2.0);

        double energy = 0.0;

        for (size_t i = 0; i < spectrum.size(); i++)
        {
            const double frequency = i * binWidth;

            if (frequency >= lowHertz && frequency < highHertz)
            {
                energy += spectrum[i] * spectrum[i];
            }
        }

        return (energy > 0.0) ? 10.0 * std::log10(energy) : -200.0;
    }

    // Objective A/B of two renders. Needed to judge Compatible against the real in-box synth,
    // where "sounds about the same" is not a usable answer.
    int RunCompare(_In_ const std::wstring& pathA, _In_ const std::wstring& pathB, _In_ bool normalize)
    {
        LoadedWave a;
        LoadedWave b;

        if (!TryLoadWaveFile(pathA, a))
        {
            printf("could not read %s (16 bit PCM WAV only)\n", ToUtf8(pathA).c_str());
            return 1;
        }

        if (!TryLoadWaveFile(pathB, b))
        {
            printf("could not read %s (16 bit PCM WAV only)\n", ToUtf8(pathB).c_str());
            return 1;
        }

        printf("A  %s\n   %u Hz, %u channels, %zu frames\n",
            ToUtf8(pathA).c_str(), a.SampleRate, a.Channels, a.Interleaved.size() / a.Channels);
        printf("B  %s\n   %u Hz, %u channels, %zu frames\n\n",
            ToUtf8(pathB).c_str(), b.SampleRate, b.Channels, b.Interleaved.size() / b.Channels);

        auto toMono = [](const LoadedWave& wave)
        {
            std::vector<float> mono(wave.Interleaved.size() / wave.Channels);

            for (size_t i = 0; i < mono.size(); i++)
            {
                double sum = 0.0;

                for (uint16_t c = 0; c < wave.Channels; c++)
                {
                    sum += wave.Interleaved[i * wave.Channels + c];
                }

                mono[i] = static_cast<float>(sum / wave.Channels);
            }

            return mono;
        };

        std::vector<float> monoA = toMono(a);
        std::vector<float> monoB = toMono(b);

        // Compare at the higher rate so neither is judged through a downsample.
        const uint32_t compareRate = (std::max)(a.SampleRate, b.SampleRate);

        auto resample = [](const std::vector<float>& input, uint32_t fromRate, uint32_t toRate)
        {
            if (fromRate == toRate)
            {
                return input;
            }

            const double ratio = static_cast<double>(fromRate) / toRate;
            const auto count = static_cast<size_t>(input.size() / ratio);

            std::vector<float> output(count);

            for (size_t i = 0; i < count; i++)
            {
                const double position = i * ratio;
                const auto index = static_cast<size_t>(position);
                const double fraction = position - index;
                const float first = input[(std::min)(index, input.size() - 1)];
                const float second = input[(std::min)(index + 1, input.size() - 1)];

                output[i] = static_cast<float>(first + (second - first) * fraction);
            }

            return output;
        };

        monoA = resample(monoA, a.SampleRate, compareRate);
        monoB = resample(monoB, b.SampleRate, compareRate);

        if (a.SampleRate != b.SampleRate)
        {
            printf("note: resampled to %u Hz for comparison\n\n", compareRate);
        }

        const size_t count = (std::min)(monoA.size(), monoB.size());

        // Two synthesizers need not agree on output level, and a constant offset would otherwise
        // show up in every band and hide the actual spectral difference.
        if (normalize && count > 0)
        {
            double energyReference = 0.0;
            double energyTarget = 0.0;

            for (size_t i = 0; i < count; i++)
            {
                energyReference += static_cast<double>(monoA[i]) * monoA[i];
                energyTarget += static_cast<double>(monoB[i]) * monoB[i];
            }

            if (energyTarget > 0.0)
            {
                const double gain = std::sqrt(energyReference / energyTarget);

                printf("normalized B by %+.2f dB to match A\n\n", 20.0 * std::log10(gain));

                for (auto& sample : monoB)
                {
                    sample = static_cast<float>(sample * gain);
                }
            }
        }

        double energyA = 0.0;
        double energyB = 0.0;
        double energyDifference = 0.0;
        double peakDifference = 0.0;

        for (size_t i = 0; i < count; i++)
        {
            const double difference = static_cast<double>(monoA[i]) - monoB[i];

            energyA += static_cast<double>(monoA[i]) * monoA[i];
            energyB += static_cast<double>(monoB[i]) * monoB[i];
            energyDifference += difference * difference;
            peakDifference = (std::max)(peakDifference, std::abs(difference));
        }

        auto toDb = [](double value) { return (value > 0.0) ? 20.0 * std::log10(value) : -200.0; };

        const double rmsA = std::sqrt(energyA / count);
        const double rmsB = std::sqrt(energyB / count);
        const double rmsDifference = std::sqrt(energyDifference / count);

        printf("Level\n");
        printf("  RMS A                  %.1f dBFS\n", toDb(rmsA));
        printf("  RMS B                  %.1f dBFS\n", toDb(rmsB));
        printf("  RMS of difference      %.1f dBFS\n", toDb(rmsDifference));
        printf("  difference below A     %.1f dB\n", toDb(rmsA) - toDb(rmsDifference));
        printf("  peak difference        %.1f dBFS\n", toDb(peakDifference));

        std::vector<float> difference(count);

        for (size_t i = 0; i < count; i++)
        {
            difference[i] = monoA[i] - monoB[i];
        }

        constexpr size_t FrameSize = 4096;

        const auto spectrumA = AverageSpectrum(monoA, FrameSize);
        const auto spectrumB = AverageSpectrum(monoB, FrameSize);
        const auto spectrumDifference = AverageSpectrum(difference, FrameSize);

        printf("\nWhere the energy is, by octave band (dB)\n");
        printf("  %-14s %10s %10s %10s\n", "band", "A", "B", "A - B");

        const double bandEdges[] = { 31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000, 24000 };

        for (size_t i = 0; i + 1 < ARRAYSIZE(bandEdges); i++)
        {
            const double low = bandEdges[i];
            const double high = bandEdges[i + 1];

            if (low >= compareRate / 2.0)
            {
                break;
            }

            char label[32]{};
            (void)snprintf(label, sizeof(label), "%.0f - %.0f Hz", low, high);

            printf("  %-14s %10.1f %10.1f %10.1f\n",
                label,
                BandEnergyDb(spectrumA, compareRate, low, high),
                BandEnergyDb(spectrumB, compareRate, low, high),
                BandEnergyDb(spectrumDifference, compareRate, low, high));
        }

        return 0;
    }

    // Reports the level of each note in a regularly spaced sequence. Used to compare velocity
    // response curves, where only the shape matters and any constant offset is irrelevant.
    //
    // The first onset is detected rather than assumed, because a capture carries an unknown
    // lead in and the synth's own latency. Later notes follow from the known interval, which is
    // more reliable than trying to detect onsets that may be near silent.
    int RunLevels(
        _In_ const std::wstring& path,
        _In_ uint32_t noteCount,
        _In_ double intervalSeconds,
        _In_ double windowStartSeconds,
        _In_ double windowLengthSeconds)
    {
        LoadedWave wave;

        if (!TryLoadWaveFile(path, wave))
        {
            printf("could not read %s\n", ToUtf8(path).c_str());
            return 1;
        }

        const auto frameCount = wave.Interleaved.size() / wave.Channels;
        std::vector<float> mono(frameCount);

        for (size_t i = 0; i < frameCount; i++)
        {
            double sum = 0.0;

            for (uint16_t c = 0; c < wave.Channels; c++)
            {
                sum += wave.Interleaved[i * wave.Channels + c];
            }

            mono[i] = static_cast<float>(sum / wave.Channels);
        }

        const auto analysisFrame = static_cast<size_t>(0.01 * wave.SampleRate);

        if (analysisFrame == 0 || frameCount < analysisFrame)
        {
            printf("file too short\n");
            return 1;
        }

        std::vector<double> frameEnergy;

        for (size_t start = 0; start + analysisFrame <= frameCount; start += analysisFrame)
        {
            double energy = 0.0;

            for (size_t i = 0; i < analysisFrame; i++)
            {
                energy += static_cast<double>(mono[start + i]) * mono[start + i];
            }

            frameEnergy.push_back(energy / analysisFrame);
        }

        const double peakFrameEnergy = *std::max_element(frameEnergy.begin(), frameEnergy.end());

        if (peakFrameEnergy <= 0.0)
        {
            printf("file is silent\n");
            return 1;
        }

        size_t onsetFrame = 0;

        for (size_t i = 0; i < frameEnergy.size(); i++)
        {
            if (frameEnergy[i] > peakFrameEnergy * 0.1)
            {
                onsetFrame = i;
                break;
            }
        }

        const double onsetSeconds = static_cast<double>(onsetFrame * analysisFrame) / wave.SampleRate;

        printf("%s\n", ToUtf8(path).c_str());
        printf("  first onset at %.3f s\n\n", onsetSeconds);
        printf("  %-6s %-12s %s\n", "note", "RMS dBFS", "relative to loudest");

        std::vector<double> levels;

        for (uint32_t note = 0; note < noteCount; note++)
        {
            const double windowStart = onsetSeconds + note * intervalSeconds + windowStartSeconds;

            const auto first = static_cast<size_t>(windowStart * wave.SampleRate);
            const auto length = static_cast<size_t>(windowLengthSeconds * wave.SampleRate);

            if (first + length > frameCount)
            {
                levels.push_back(-200.0);
                continue;
            }

            double energy = 0.0;

            for (size_t i = 0; i < length; i++)
            {
                energy += static_cast<double>(mono[first + i]) * mono[first + i];
            }

            const double rms = std::sqrt(energy / length);
            levels.push_back((rms > 0.0) ? 20.0 * std::log10(rms) : -200.0);
        }

        const double loudest = *std::max_element(levels.begin(), levels.end());

        for (size_t i = 0; i < levels.size(); i++)
        {
            printf("  %-6zu %-12.2f %+.2f\n", i, levels[i], levels[i] - loudest);
        }

        return 0;
    }

    // Prints the amplitude envelope of each note, in dB relative to that note's own peak.
    // Normalizing per note is deliberate: it isolates envelope shape from level, so an attack or
    // decay difference is not masked by the two synths disagreeing about loudness.
    int RunEnvelope(
        _In_ const std::wstring& path,
        _In_ uint32_t noteCount,
        _In_ double intervalSeconds,
        _In_ double lengthSeconds,
        _In_ double stepSeconds)
    {
        LoadedWave wave;

        if (!TryLoadWaveFile(path, wave))
        {
            printf("could not read %s\n", ToUtf8(path).c_str());
            return 1;
        }

        const auto frameCount = wave.Interleaved.size() / wave.Channels;
        std::vector<float> mono(frameCount);

        for (size_t i = 0; i < frameCount; i++)
        {
            double sum = 0.0;

            for (uint16_t c = 0; c < wave.Channels; c++)
            {
                sum += wave.Interleaved[i * wave.Channels + c];
            }

            mono[i] = static_cast<float>(sum / wave.Channels);
        }

        const auto detectFrame = static_cast<size_t>(0.005 * wave.SampleRate);
        std::vector<double> frameEnergy;

        for (size_t start = 0; start + detectFrame <= frameCount; start += detectFrame)
        {
            double energy = 0.0;

            for (size_t i = 0; i < detectFrame; i++)
            {
                energy += static_cast<double>(mono[start + i]) * mono[start + i];
            }

            frameEnergy.push_back(energy / detectFrame);
        }

        if (frameEnergy.empty())
        {
            printf("file too short\n");
            return 1;
        }

        const double peakFrameEnergy = *std::max_element(frameEnergy.begin(), frameEnergy.end());
        size_t onsetFrame = 0;

        for (size_t i = 0; i < frameEnergy.size(); i++)
        {
            if (frameEnergy[i] > peakFrameEnergy * 0.02)
            {
                onsetFrame = i;
                break;
            }
        }

        const double onsetSeconds = static_cast<double>(onsetFrame * detectFrame) / wave.SampleRate;
        const auto steps = static_cast<size_t>(lengthSeconds / stepSeconds);
        const auto stepFrames = static_cast<size_t>(stepSeconds * wave.SampleRate);

        printf("# %s  first onset %.3f s\n", ToUtf8(path).c_str(), onsetSeconds);
        printf("# columns are milliseconds from note onset, values are dB relative to note peak\n");
        printf("note");

        for (size_t s = 0; s < steps; s++)
        {
            printf(",%.0f", s * stepSeconds * 1000.0);
        }

        printf("\n");

        for (uint32_t note = 0; note < noteCount; note++)
        {
            const double noteStart = onsetSeconds + note * intervalSeconds;
            const auto first = static_cast<size_t>(noteStart * wave.SampleRate);

            std::vector<double> envelope;

            for (size_t s = 0; s < steps; s++)
            {
                const size_t start = first + s * stepFrames;

                if (start + stepFrames > frameCount)
                {
                    envelope.push_back(0.0);
                    continue;
                }

                double energy = 0.0;

                for (size_t i = 0; i < stepFrames; i++)
                {
                    energy += static_cast<double>(mono[start + i]) * mono[start + i];
                }

                envelope.push_back(std::sqrt(energy / stepFrames));
            }

            const double peak = *std::max_element(envelope.begin(), envelope.end());

            printf("%u", note);

            for (const auto value : envelope)
            {
                const double relative = (peak > 0.0 && value > 0.0)
                    ? 20.0 * std::log10(value / peak)
                    : -120.0;

                printf(",%.2f", (std::max)(relative, -120.0));
            }

            printf("\n");
        }

        return 0;
    }

    // Dumps the articulation the engine actually resolved for an instrument, so a measured
    // envelope difference can be traced to a parameter rather than guessed at.
    int RunArticulation(_In_ const DlsCollection& collection, _In_ uint8_t program, _In_ bool drumKit)
    {
        const DlsInstrument* instrument = collection.FindInstrument(0, 0, program, drumKit);

        if (instrument == nullptr)
        {
            printf("no instrument for program %u\n", program);
            return 1;
        }

        printf("Program %u: %s\n", program, ToUtf8(instrument->Name).c_str());
        printf("  instrument level connections %zu, regions %zu\n\n",
            instrument->Connections.size(), instrument->Regions.size());

        size_t regionIndex = 0;

        for (const auto& region : instrument->Regions)
        {
            const auto articulation = ResolveArticulation(instrument->Connections, region.Connections);

            printf("  region %zu  keys %u-%u  velocity %u-%u  region connections %zu\n",
                regionIndex, region.KeyLow, region.KeyHigh,
                region.VelocityLow, region.VelocityHigh, region.Connections.size());

            printf("    EG1  attack %.4f s  decay %.4f s  sustain %.1f %%  release %.4f s\n",
                articulation.Eg1AttackSeconds, articulation.Eg1DecaySeconds,
                articulation.Eg1SustainFraction * 100.0, articulation.Eg1ReleaseSeconds);
            printf("         velocity to attack %d  key to decay %d\n",
                articulation.Eg1VelocityToAttackTimeCents, articulation.Eg1KeyToDecayTimeCents);
            printf("    EG2  attack %.4f s  decay %.4f s  sustain %.1f %%  release %.4f s  to pitch %.1f cents\n",
                articulation.Eg2AttackSeconds, articulation.Eg2DecaySeconds,
                articulation.Eg2SustainFraction * 100.0, articulation.Eg2ReleaseSeconds,
                articulation.Eg2ToPitchCents);
            printf("    LFO  %.2f Hz  delay %.4f s  to attenuation %.2f dB  to pitch %.1f cents\n",
                articulation.LfoFrequencyHertz, articulation.LfoStartDelaySeconds,
                articulation.LfoToAttenuationDb, articulation.LfoToPitchCents);
            printf("    pan  %.1f %%\n\n", articulation.PanFraction * 100.0);

            regionIndex++;

            if (regionIndex >= 4)
            {
                printf("  (%zu more regions not shown)\n", instrument->Regions.size() - regionIndex);
                break;
            }
        }

        return 0;
    }

    // A voice cut off mid waveform leaves a step in the signal, which is heard as a click. Real
    // audio has a bounded slew rate, so an isolated large sample to sample jump is a defect.
    int RunClicks(_In_ const std::wstring& path, _In_ double thresholdFraction)
    {
        LoadedWave wave;

        if (!TryLoadWaveFile(path, wave))
        {
            printf("could not read %s\n", ToUtf8(path).c_str());
            return 1;
        }

        const auto frameCount = wave.Interleaved.size() / wave.Channels;

        if (frameCount < 2)
        {
            printf("file too short\n");
            return 1;
        }

        double maxDelta = 0.0;
        double maxDeltaSeconds = 0.0;
        size_t exceeding = 0;
        double peak = 0.0;

        for (size_t i = 1; i < frameCount; i++)
        {
            for (uint16_t channel = 0; channel < wave.Channels; channel++)
            {
                const double current = wave.Interleaved[i * wave.Channels + channel];
                const double previous = wave.Interleaved[(i - 1) * wave.Channels + channel];
                const double delta = std::abs(current - previous);

                peak = (std::max)(peak, std::abs(current));

                if (delta > maxDelta)
                {
                    maxDelta = delta;
                    maxDeltaSeconds = static_cast<double>(i) / wave.SampleRate;
                }

                if (delta > thresholdFraction)
                {
                    exceeding++;
                }
            }
        }

        printf("%s\n", ToUtf8(path).c_str());
        printf("  peak level             %.3f\n", peak);
        printf("  largest sample step    %.4f at %.3f s\n", maxDelta, maxDeltaSeconds);
        printf("  steps over %.3f        %zu\n", thresholdFraction, exceeding);
        printf("\n  %s\n", (exceeding == 0) ? "PASS - no discontinuities" : "FAIL - discontinuities present");

        return (exceeding == 0) ? 0 : 1;
    }

    // Effects run on the mix bus, so their cost is independent of polyphony and worth knowing
    // separately from the voice cost.
    int RunEffectsBenchmark(_In_ uint32_t sampleRate, _In_ double seconds)
    {
        constexpr uint32_t blockFrames = 480;

        std::vector<float> input(static_cast<size_t>(blockFrames) * 2, 0.0f);
        std::vector<float> output(static_cast<size_t>(blockFrames) * 2, 0.0f);

        // Something with content across the spectrum, so no denormal shortcuts flatter the result.
        for (size_t i = 0; i < input.size(); i++)
        {
            input[i] = 0.25f * std::sin(static_cast<float>(i) * 0.07f);
        }

        ReverbEffect reverb;
        ChorusEffect chorus;

        if (!reverb.Configure(sampleRate) || !chorus.Configure(sampleRate))
        {
            printf("could not configure the effects\n");
            return 1;
        }

        reverb.SetParameter(AudioEffectParameter::WetLevel, 0.35);
        reverb.SetParameter(AudioEffectParameter::Time, 2.0);
        chorus.SetParameter(AudioEffectParameter::WetLevel, 0.30);

        const auto blocks = static_cast<uint32_t>(seconds * sampleRate / blockFrames);

        LARGE_INTEGER frequency{};
        QueryPerformanceFrequency(&frequency);

        auto measure = [&](const char* name, IAudioEffect* first, IAudioEffect* second)
        {
            LARGE_INTEGER start{};
            QueryPerformanceCounter(&start);

            for (uint32_t i = 0; i < blocks; i++)
            {
                std::fill(output.begin(), output.end(), 0.0f);

                if (first != nullptr) { first->Process(input.data(), output.data(), blockFrames); }
                if (second != nullptr) { second->Process(input.data(), output.data(), blockFrames); }
            }

            LARGE_INTEGER end{};
            QueryPerformanceCounter(&end);

            const double elapsed =
                static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);

            const double audioSeconds = static_cast<double>(blocks) * blockFrames / sampleRate;

            printf("  %-22s %6.3f %% of one core\n", name, 100.0 * elapsed / audioSeconds);
        };

        printf("Effects at %u Hz\n\n", sampleRate);

        measure("reverb", &reverb, nullptr);
        measure("chorus", nullptr, &chorus);
        measure("both", &reverb, &chorus);

        return 0;
    }

    int RunBenchmark(_In_ SynthEngine& engine, _In_ uint32_t targetVoices, _In_ double seconds)
    {
        const uint32_t sampleRate = engine.Config().RenderSampleRate();
        const uint32_t blockFrames = 480;

        std::vector<float> block(static_cast<size_t>(blockFrames) * 2);

        // Hold a dense chord across many channels to reach the requested polyphony.
        engine.ProgramChange(0, 48);

        uint32_t started = 0;

        for (uint8_t channel = 0; channel < 16 && started < targetVoices; channel++)
        {
            if (channel == 9)
            {
                continue;
            }

            engine.ProgramChange(channel, 48);

            for (uint8_t note = 24; note < 108 && started < targetVoices; note += 1)
            {
                engine.NoteOn(channel, note, 40000);
                started++;
            }
        }

        const uint32_t totalFrames = static_cast<uint32_t>(seconds * sampleRate);

        LARGE_INTEGER frequency{};
        LARGE_INTEGER start{};
        LARGE_INTEGER end{};
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&start);

        uint32_t rendered = 0;

        while (rendered < totalFrames)
        {
            const uint32_t frames = (std::min)(blockFrames, totalFrames - rendered);
            engine.Render(block.data(), frames);
            rendered += frames;
        }

        QueryPerformanceCounter(&end);

        const double elapsedSeconds =
            static_cast<double>(end.QuadPart - start.QuadPart) / static_cast<double>(frequency.QuadPart);
        const double audioSeconds = static_cast<double>(totalFrames) / sampleRate;

        printf("Benchmark\n");
        printf("  requested voices       %u\n", targetVoices);
        printf("  active voices          %u\n", engine.ActiveVoiceCount());
        printf("  render sample rate     %u Hz\n", sampleRate);
        printf("  audio rendered         %.2f s\n", audioSeconds);
        printf("  wall clock             %.3f s\n", elapsedSeconds);
        printf("  realtime factor        %.1fx\n", audioSeconds / elapsedSeconds);
        printf("  single core load       %.2f %%\n", 100.0 * elapsedSeconds / audioSeconds);

        return 0;
    }
}

int wmain(int argc, wchar_t** argv)
{
    SetConsoleOutputCP(CP_UTF8);

    std::wstring dlsPath;
    std::wstring scorePath;
    std::wstring outputPath = L"render.wav";
    SynthMode mode = SynthMode::Modern;
    uint32_t sampleRate = 48000;
    bool benchmark = false;
    bool effectsBenchmark = false;
    uint32_t benchmarkVoices = 64;
    bool tuningTest = false;
    uint8_t tuningProgram = 0;
    bool articulationDump = false;
    uint8_t articulationProgram = 0;

    for (int i = 1; i < argc; i++)
    {
        const std::wstring argument = argv[i];

        if (argument == L"--compat")
        {
            mode = SynthMode::Compatible;
        }
        else if (argument == L"--modern")
        {
            mode = SynthMode::Modern;
        }
        else if (argument == L"--dls" && i + 1 < argc)
        {
            dlsPath = argv[++i];
        }
        else if (argument == L"--score" && i + 1 < argc)
        {
            scorePath = argv[++i];
        }
        else if (argument == L"--out" && i + 1 < argc)
        {
            outputPath = argv[++i];
        }
        else if (argument == L"--rate" && i + 1 < argc)
        {
            sampleRate = static_cast<uint32_t>(_wtoi(argv[++i]));
        }
        else if (argument == L"--fxbench")
        {
            effectsBenchmark = true;
        }
        else if (argument == L"--bench")
        {
            benchmark = true;

            if (i + 1 < argc && argv[i + 1][0] != L'-')
            {
                benchmarkVoices = static_cast<uint32_t>(_wtoi(argv[++i]));
            }
        }
        else if (argument == L"--tuning")
        {
            tuningTest = true;

            if (i + 1 < argc && argv[i + 1][0] != L'-')
            {
                tuningProgram = static_cast<uint8_t>(_wtoi(argv[++i]) & 0x7F);
            }
        }
        else if (argument == L"--compare" && i + 2 < argc)
        {
            const std::wstring pathA = argv[++i];
            const std::wstring pathB = argv[++i];
            const bool normalize = (i + 1 < argc) && (std::wstring(argv[i + 1]) == L"--normalize");
            return RunCompare(pathA, pathB, normalize);
        }
        else if (argument == L"--levels" && i + 5 < argc)
        {
            const std::wstring levelsPath = argv[++i];
            const auto noteCount = static_cast<uint32_t>(_wtoi(argv[++i]));
            const double interval = _wtof(argv[++i]);
            const double windowStart = _wtof(argv[++i]);
            const double windowLength = _wtof(argv[++i]);
            return RunLevels(levelsPath, noteCount, interval, windowStart, windowLength);
        }
        else if (argument == L"--envelope" && i + 5 < argc)
        {
            const std::wstring envelopePath = argv[++i];
            const auto noteCount = static_cast<uint32_t>(_wtoi(argv[++i]));
            const double interval = _wtof(argv[++i]);
            const double length = _wtof(argv[++i]);
            const double step = _wtof(argv[++i]);
            return RunEnvelope(envelopePath, noteCount, interval, length, step);
        }
        else if (argument == L"--articulation" && i + 1 < argc)
        {
            articulationDump = true;
            articulationProgram = static_cast<uint8_t>(_wtoi(argv[++i]) & 0x7F);
        }
        else if (argument == L"--clicks" && i + 1 < argc)
        {
            const std::wstring clicksPath = argv[++i];
            const double threshold = (i + 1 < argc && argv[i + 1][0] != L'-') ? _wtof(argv[++i]) : 0.05;
            return RunClicks(clicksPath, threshold);
        }
        else
        {
            printf("usage: synthspike-render [--dls <file>] [--score <file>] [--out <file.wav>]\n");
            printf("                         [--compat|--modern] [--rate <hz>] [--bench [voices]]\n");
            printf("                         [--tuning [program]]\n");
            printf("                         [--compare <a.wav> <b.wav> [--normalize]]\n");
            printf("                         [--levels <wav> <count> <interval> <winStart> <winLen>]\n");
            printf("                         [--envelope <wav> <count> <interval> <len> <step>]\n");
            return 2;
        }
    }

    if (dlsPath.empty())
    {
        dlsPath = DefaultDlsPath();
    }

    DlsCollection collection;
    const auto status = DlsCollection::LoadFromFile(
        dlsPath, DlsParseLimits{}, SoundSetOrigin::AnyPath, collection);

    if (status != DlsParseStatus::Ok)
    {
        printf("failed to load %s: %s\n", ToUtf8(dlsPath).c_str(), DlsParseStatusToString(status));
        return 1;
    }

    SynthConfig config = SynthConfig::ForMode(mode, sampleRate);

    // Analysis measures the voice engine, so a reverb tail would smear the very thing being
    // measured. The effects have their own benchmark.
    if (tuningTest)
    {
        config.EnableEffects = false;
    }

    if (benchmark)
    {
        config.MaxVoices = (std::max)(config.MaxVoices, benchmarkVoices);
    }

    SynthEngine engine;

    if (!engine.Initialize(&collection, config))
    {
        printf("engine initialization failed\n");
        return 1;
    }

    printf("Sound set   %s (%zu instruments)\n", ToUtf8(collection.Name()).c_str(), collection.Instruments().size());
    printf("Mode        %s\n", mode == SynthMode::Compatible ? "Compatible" : "Modern");
    printf("Render rate %u Hz\n\n", config.RenderSampleRate());

    if (tuningTest)
    {
        return RunTuningTest(collection, config, tuningProgram);
    }

    if (articulationDump)
    {
        return RunArticulation(collection, articulationProgram, false);
    }

    // Effects do not need a sound set, so this runs before any file is touched.
    if (effectsBenchmark)
    {
        return RunEffectsBenchmark(sampleRate, 5.0);
    }

    if (benchmark)
    {
        return RunBenchmark(engine, benchmarkVoices, 10.0);
    }

    std::vector<TimedEvent> events;

    if (!scorePath.empty())
    {
        if (!TryParseScore(scorePath, events))
        {
            printf("could not read score %s\n", ToUtf8(scorePath).c_str());
            return 1;
        }
    }
    else
    {
        events = BuildDemoScore();
    }

    std::stable_sort(events.begin(), events.end(),
        [](const TimedEvent& left, const TimedEvent& right) { return left.Seconds < right.Seconds; });

    double lastEventSeconds = 0.0;

    for (const auto& event : events)
    {
        lastEventSeconds = (std::max)(lastEventSeconds, event.Seconds);
    }

    // Two extra seconds so release tails are not cut off.
    const uint32_t renderRate = config.RenderSampleRate();
    const auto totalFrames = static_cast<uint32_t>((lastEventSeconds + 2.0) * renderRate);

    std::vector<float> output(static_cast<size_t>(totalFrames) * 2, 0.0f);

    const uint32_t blockFrames = 128;
    size_t nextEvent = 0;
    uint32_t rendered = 0;
    uint32_t peakVoices = 0;

    while (rendered < totalFrames)
    {
        while (nextEvent < events.size() &&
               static_cast<uint32_t>(events[nextEvent].Seconds * renderRate) <= rendered)
        {
            DispatchEvent(engine, events[nextEvent]);
            nextEvent++;
        }

        // Render only up to the next event rather than through it, so a note starts on the frame
        // it is due rather than at the next block boundary. Without this the event grid is the
        // block size, which differs between render rates and is audible as timing jitter.
        uint32_t limit = totalFrames;

        if (nextEvent < events.size())
        {
            const auto eventFrame = static_cast<uint32_t>(events[nextEvent].Seconds * renderRate);
            limit = (std::min)(limit, (std::max)(eventFrame, rendered + 1));
        }

        const uint32_t frames = (std::min)(blockFrames, limit - rendered);

        engine.Render(output.data() + static_cast<size_t>(rendered) * 2, frames);

        peakVoices = (std::max)(peakVoices, engine.ActiveVoiceCount());
        rendered += frames;
    }

    float peakLevel = 0.0f;

    for (const auto sample : output)
    {
        peakLevel = (std::max)(peakLevel, std::abs(sample));
    }

    if (!WriteWaveFile(outputPath, output, renderRate))
    {
        printf("could not write %s\n", ToUtf8(outputPath).c_str());
        return 1;
    }

    printf("Rendered %.2f s to %s\n", static_cast<double>(totalFrames) / renderRate, ToUtf8(outputPath).c_str());
    printf("  events                 %zu\n", events.size());
    printf("  peak simultaneous voices %u\n", peakVoices);
    printf("  peak level             %.3f (%.1f dBFS)\n",
        peakLevel, (peakLevel > 0.0f) ? 20.0 * std::log10(static_cast<double>(peakLevel)) : -96.0);
    printf("  voices stolen          %llu\n", static_cast<unsigned long long>(engine.StolenVoiceCount()));
    printf("  notes dropped          %llu\n", static_cast<unsigned long long>(engine.DroppedNoteCount()));

    return 0;
}
