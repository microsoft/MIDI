// Lists WinMM MIDI output devices. Used to confirm the in-box synth is present and to find its
// device index before capturing a reference render from it.

#include <windows.h>
#include <mmeapi.h>

#include <cstdio>

int wmain()
{
    SetConsoleOutputCP(CP_UTF8);

    const UINT count = midiOutGetNumDevs();

    printf("WinMM MIDI output devices: %u\n\n", count);
    printf("  %-4s %-12s %-8s %-8s %s\n", "id", "technology", "voices", "notes", "name");

    for (UINT id = 0; id < count; id++)
    {
        MIDIOUTCAPSW caps{};

        if (midiOutGetDevCapsW(id, &caps, sizeof(caps)) != MMSYSERR_NOERROR)
        {
            continue;
        }

        const char* technology = "unknown";

        switch (caps.wTechnology)
        {
        case MOD_MIDIPORT: technology = "hardware"; break;
        case MOD_SYNTH: technology = "synth"; break;
        case MOD_SQSYNTH: technology = "square"; break;
        case MOD_FMSYNTH: technology = "fm"; break;
        case MOD_MAPPER: technology = "mapper"; break;
        case MOD_WAVETABLE: technology = "wavetable"; break;
        case MOD_SWSYNTH: technology = "software"; break;
        default: break;
        }

        printf("  %-4u %-12s %-8u %-8u %ws\n", id, technology, caps.wVoices, caps.wNotes, caps.szPname);
    }

    return 0;
}
