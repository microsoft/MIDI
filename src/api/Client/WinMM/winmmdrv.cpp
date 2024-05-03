#include <windows.h>
#include <mmsystem.h>
#include <mmddk.h>
#include <strsafe.h>

BOOL g_ProcessIsTerminating = FALSE;

LRESULT CALLBACK DriverProc
(
    DWORD           id,
    HDRVR           hDriver,
    WORD            msg,
    LPARAM          lParam1,
    LPARAM          lParam2
)
{
    switch (msg)
    {
        case DRV_LOAD:
            return 1L ;
        case DRV_FREE:
            return 1L ;
        case DRV_OPEN:
            return 1L ;
        case DRV_CLOSE:
            return 1L ;
        case DRV_ENABLE:
            return 1L ;
        case DRV_DISABLE:
            return 1L ;
        case DRV_QUERYCONFIGURE:
            return 0L ;
        case DRV_CONFIGURE:
            return 0L ;
        case DRV_INSTALL:
            return DRV_OK ;
        case DRV_REMOVE:
            return 0 ;
    }

    return DefDriverProc( id, hDriver, msg, lParam1, lParam2 ) ;
}

struct MIDICLIENT
{
    BOOL used {0};
    BOOL started {0};
    UINT device {0};
    DWORD_PTR user {0};
    MIDIOPENDESC desc {0};
    DWORD_PTR flags {0};
    LPMIDIHDR buffers[16] {0};
};

struct MIDICLIENT g_InClients[32];
struct MIDICLIENT g_OutClients[32];

void Callback(struct MIDICLIENT* client, UINT msg, DWORD_PTR dw1, DWORD_PTR dw2)
{
    DriverCallback(client->desc.dwCallback,
                    HIWORD(client->flags),
                    (HDRVR) client->desc.hMidi,
                    msg,
                    client->desc.dwInstance,
                    dw1,
                    dw2);
}

DWORD APIENTRY midMessage(UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    MMRESULT mmResult = MMSYSERR_NOERROR;

    switch(uMsg)
    {
        case MIDM_GETNUMDEVS:
            mmResult = MAKELONG(16, MMSYSERR_NOERROR);
            break;

        case MIDM_GETDEVCAPS:
            {
                MIDIINCAPSW *caps = (MIDIINCAPS *) dwParam1;
                memset(caps, 0, dwParam2);

                caps->wMid = 1;
                caps->vDriverVersion = 0x0100;

                if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, TEXT("MidiSrv device %d"), uDeviceID)))
                {
                    mmResult = MMSYSERR_ERROR;
                }
            }
            break;

        case MIDM_OPEN:
            {
                UINT freeIndex = 0;
                for (freeIndex = 0; g_InClients[freeIndex].used && freeIndex < 32; freeIndex++);

                if (freeIndex < 32 &&
                    dwParam1 >= sizeof(MIDIOPENDESC))
                {
                    g_InClients[freeIndex].used = TRUE;
                    g_InClients[freeIndex].device = uDeviceID;
                    g_InClients[freeIndex].user = (DWORD_PTR) &(g_InClients[freeIndex]);
                    g_InClients[freeIndex].flags = dwParam2;
                    memcpy(&(g_InClients[freeIndex].desc), (PVOID) dwParam1, sizeof(MIDIOPENDESC));
                    *(DWORD_PTR *)dwUser = g_InClients[freeIndex].user;
                    Callback(&g_InClients[freeIndex], MIM_OPEN, 0, 0);
                }
                else
                {
                    mmResult = MMSYSERR_ERROR;
                }
            }
            break;

        case MIDM_CLOSE:
            {
                mmResult = MMSYSERR_ERROR;

                for (UINT index = 0; index < 32; index++)
                {
                    if (g_InClients[index].used && g_InClients[index].user == dwUser)
                    {
                        g_InClients[index].used = FALSE;
                        memset(g_InClients[index].buffers, 0, sizeof(g_InClients[index].buffers));
                        Callback(&g_InClients[index], MIM_CLOSE, 0, 0);
                        mmResult = MMSYSERR_NOERROR;
                        break;
                    }
                }
            }
            break;

        case MIDM_ADDBUFFER:
            {
                mmResult = MMSYSERR_ERROR;

                LPMIDIHDR header = (LPMIDIHDR) dwParam1;

                if (nullptr != header &&
                    dwParam2 >= sizeof(MIDIHDR))
                {
                    for (UINT index = 0; index < 32; index++)
                    {
                        if (g_InClients[index].used && g_InClients[index].user == dwUser)
                        {
                            for (int bufIndex = 0; bufIndex < 16; bufIndex++)
                            {
                                if (g_InClients[index].buffers[bufIndex] == nullptr)
                                {
                                    g_InClients[index].buffers[bufIndex] = header;
                                    mmResult = MMSYSERR_NOERROR;
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                else
                {
                    mmResult = MMSYSERR_INVALPARAM;
                }
            }
            break;

        case MIDM_START:
            {
                mmResult = MMSYSERR_ERROR;
                
                for (UINT index = 0; index < 32; index++)
                {
                    if (g_InClients[index].used && g_InClients[index].user == dwUser)
                    {
                        g_InClients[index].started = TRUE;
                        mmResult = MMSYSERR_NOERROR;
                        break;
                    }
                }
            }
            break;

        case MIDM_STOP:
            {
                mmResult = MMSYSERR_ERROR;
                
                for (UINT index = 0; index < 32; index++)
                {
                    if (g_InClients[index].used && g_InClients[index].user == dwUser)
                    {
                        g_InClients[index].started = FALSE;
                        mmResult = MMSYSERR_NOERROR;
                        break;
                    }
                }
            }
            break;

        case MIDM_RESET:
            {
                mmResult = MMSYSERR_ERROR;
                
                for (UINT index = 0; index < 32; index++)
                {
                    if (g_InClients[index].used && g_InClients[index].user == dwUser)
                    {
                        mmResult = MMSYSERR_NOERROR;
                        // reset?
                        break;
                    }
                }
            }
            break;


        default:
            mmResult = MMSYSERR_NOTSUPPORTED;
            break;
    }

    return mmResult;
}

DWORD APIENTRY modMessage(UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    MMRESULT mmResult = MMSYSERR_NOERROR;

    switch(uMsg)
    {
        case MODM_GETNUMDEVS:
            mmResult = MAKELONG(16, MMSYSERR_NOERROR);
            break;

        case MODM_GETDEVCAPS:
            {
                MIDIOUTCAPSW *caps = (MIDIOUTCAPS *) dwParam1;
                memset(caps, 0, dwParam2);

                caps->wMid = 1;
                caps->vDriverVersion = 0x0100;
                caps->wTechnology = MOD_MIDIPORT;
                caps->wChannelMask = 0xFFFF;

                if (FAILED(StringCchPrintfW(caps->szPname, MAXPNAMELEN, TEXT("MidiSrv device %d"), uDeviceID)))
                {
                    mmResult = MMSYSERR_ERROR;
                }
            }
            break;

        case MODM_OPEN:
            {
                UINT freeIndex = 0;
                for (freeIndex = 0; g_OutClients[freeIndex].used && freeIndex < 32; freeIndex++);

                if (freeIndex < 32 &&
                    dwParam1 >= sizeof(MIDIOPENDESC))
                {
                    g_OutClients[freeIndex].used = TRUE;
                    g_OutClients[freeIndex].device = uDeviceID;
                    g_OutClients[freeIndex].user = (DWORD_PTR) &(g_InClients[freeIndex]);
                    g_OutClients[freeIndex].flags = dwParam2;
                    memcpy(&(g_OutClients[freeIndex].desc), (PVOID) dwParam1, sizeof(MIDIOPENDESC));
                    *(DWORD_PTR *)dwUser = g_OutClients[freeIndex].user;
                    Callback(&g_OutClients[freeIndex], MOM_OPEN, 0, 0);
                }
                else
                {
                    mmResult = MMSYSERR_ERROR;
                }
            }
            break;

        case MODM_CLOSE:
            {
                mmResult = MMSYSERR_ERROR;

                for (UINT index = 0; index < 32; index++)
                {
                    if (g_OutClients[index].used && 
                        g_OutClients[index].user == dwUser)
                    {
                        g_OutClients[index].used = FALSE;
                        Callback(&g_OutClients[index], MOM_CLOSE, 0, 0);
                        mmResult = MMSYSERR_NOERROR;
                        break;
                    }
                }
            }
            break;

        case MODM_LONGDATA:
            {
                mmResult = MMSYSERR_ERROR;

                LPMIDIHDR header = (LPMIDIHDR) dwParam1;

                if (nullptr != header && 
                    dwParam2 >= sizeof(MIDIHDR))
                {
                    for (UINT outIndex = 0; outIndex < 32; outIndex++)
                    {
                        if (g_OutClients[outIndex].used && g_OutClients[outIndex].user == dwUser)
                        {
                            mmResult = MMSYSERR_NOERROR;

                            for (UINT inIndex = 0; inIndex < 32; inIndex++)
                            {
                                if (g_InClients[inIndex].used && 
                                    g_OutClients[outIndex].device == g_InClients[inIndex].device)
                                {
                                    g_InClients[inIndex].buffers[0]->dwBytesRecorded = min(header->dwBufferLength, g_InClients[inIndex].buffers[0]->dwBufferLength);
                                    memcpy(g_InClients[inIndex].buffers[0]->lpData, header->lpData, min(header->dwBufferLength, g_InClients[inIndex].buffers[0]->dwBytesRecorded));
                                    // send this message out to the matching midi in client.
                                    Callback(&g_InClients[inIndex], MIM_LONGDATA, (DWORD_PTR) g_InClients[inIndex].buffers[0], 0);
                                    break;
                                }
                            }

                            Callback(&g_OutClients[outIndex], MOM_DONE, dwParam1, 0);
                            break;
                        }
                    }
                }
                else
                {
                    mmResult = MMSYSERR_INVALPARAM;
                }
            }
            break;

        case MODM_DATA:
            {
                mmResult = MMSYSERR_ERROR;

                for (UINT outIndex = 0; outIndex < 32; outIndex++)
                {
                    if (g_OutClients[outIndex].used && g_OutClients[outIndex].user == dwUser)
                    {
                        mmResult = MMSYSERR_NOERROR;

                        for (UINT inIndex = 0; inIndex < 32; inIndex++)
                        {
                            if (g_InClients[inIndex].used &&
                                g_OutClients[outIndex].device == g_InClients[inIndex].device)
                            {
                                // send this message out to the matching midi in client.
                                Callback(&g_InClients[inIndex], MIM_DATA, dwParam1, dwParam2);
                            }
                        }

                        Callback(&g_OutClients[outIndex], MOM_DONE, dwParam1, 0);

                        break;
                    }
                }
            }
            break;

        case MODM_RESET:
            {
                mmResult = MMSYSERR_ERROR;
                
                for (UINT index = 0; index < 32; index++)
                {
                    if (g_OutClients[index].used && g_OutClients[index].user == dwUser)
                    {
                        mmResult = MMSYSERR_NOERROR;
                        // reset?
                        break;
                    }
                }
            }
            break;

        default:
            mmResult = MMSYSERR_NOTSUPPORTED;
            break;
    }

    return mmResult;
}

BOOL WINAPI DllMain
(
    HINSTANCE hinstDLL,
    DWORD     fdwReason,
    LPVOID    lpvReserved
)
{
    UNREFERENCED_PARAMETER(hinstDLL);

/*
    if (DLL_PROCESS_ATTACH == fdwReason)
    {
    }
    else if ((DLL_PROCESS_DETACH == fdwReason) && ( NULL == lpvReserved))
    {
    }
*/

    if ((DLL_PROCESS_DETACH == fdwReason) && (NULL != lpvReserved))
    {
        g_ProcessIsTerminating = TRUE;
    }

    return TRUE;
}

