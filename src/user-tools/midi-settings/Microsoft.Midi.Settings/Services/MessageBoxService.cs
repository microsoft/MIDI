// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace Microsoft.Midi.Settings.Services
{
    public class MessageBoxService : IMessageBoxService
    {
        private enum MessageBoxType : uint
        {
            MB_ABORTRETRYIGNORE = 0x00000002,
            MB_CANCELTRYCONTINUE = 0x00000006,
            MB_HELP = 0x00004000,
            MB_OK = 0x00000000,
            MB_OKCANCEL = 0x00000001,
            MB_RETRYCANCEL = 0x00000005,
            MB_YESNO = 0x00000004,
            MB_YESNOCANCEL = 0x00000003,


            MB_ICONEXCLAMATION = 0x00000030,
            MB_ICONWARNING = 0x00000030,
            MB_ICONINFORMATION = 0x00000040,
            MB_ICONASTERISK = 0x00000040,
            MB_ICONQUESTION = 0x00000020,
            MB_ICONSTOP = 0x00000010,
            MB_ICONERROR = 0x00000010,
            MB_ICONHAND = 0x00000010

        }


        [DllImport("User32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        static extern int MessageBox(
                            IntPtr hWnd,
                            string lpText,
                            string lpCaption,
                            uint uType);



        public void ShowInfo(string message, string title)
        {
            MessageBox(
                (IntPtr)0,
                message,
                title,
                (uint)(MessageBoxType.MB_ICONINFORMATION | MessageBoxType.MB_OK));
        }


        public void ShowError(string message, string title)
        {
            MessageBox(
                (IntPtr)0,
                message,
                title,
                (uint)(MessageBoxType.MB_ICONERROR | MessageBoxType.MB_OK));
        }
    }
}
