using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using WixToolset.Dtf.WindowsInstaller;

namespace custom_actions
{
    // Used by both the Tools and the Transports installers.
    //
    // When a file is in use, Windows Installer cannot replace it, so it queues the new file to be
    // copied in at the next restart. Until the customer restarts, a newly installed app can load an
    // old DLL that was in use (or an app that was in use can be the old one), and that mix crashes.
    // So these actions close what can be closed before any file is touched, stop the MIDI service,
    // and make sure the installer reports that a restart is required whenever something could not
    // be closed or stopped. The bundle then shows its Restart button.
    public class InstallSafetyActions
    {
        private const string MidiServiceName = "midisrv";
        private const string NotificationsExeName = "midinotifications.exe";
        private const string NotificationsWindowClass = "MidiNotificationsMessageWindow";
        private const string WinUIWindowClass = "WinUIDesktopWin32WindowClass";

        // Queued for deletion at restart when the MIDI service could not be stopped. Windows removes
        // it at the next restart, so its presence in the pending list means "restart still needed".
        private const string RestartMarkerFileName = "WindowsMidiServicesRestartRequired.marker";

        // Error table entry in each package's .wxs
        private const int ErrorRestartAlreadyPending = 25000;

        // Set on the command line or by SetProperty to the folder each package installs into.
        private const string RestartCheckFolderProperty = "MIDIRESTARTCHECKFOLDER";

        private const int DefaultEarlyStopSeconds = 60;
        private const int DefaultDeferredStopSeconds = 60;
        private const int DeferredStopSecondsAfterEarlyFailure = 15;

        // How long to give each stop request before asking again. A second request often succeeds
        // where the first did not, while a client was still holding the service.
        private static readonly TimeSpan StopRequestWait = TimeSpan.FromSeconds(15);

        // midisrv is demand start with an RPC trigger, so any MIDI app that calls it starts it again.
        // A stop only counts once the service has stayed stopped for this long.
        private static readonly TimeSpan StoppedSettleTime = TimeSpan.FromSeconds(2);

        private static readonly TimeSpan NotificationsCloseWait = TimeSpan.FromSeconds(5);
        private static readonly TimeSpan ToolsCloseWait = TimeSpan.FromSeconds(15);

        // ------------------------------------------------------------------------------------------
        // Restart already pending
        // ------------------------------------------------------------------------------------------

        // Immediate. An earlier install that could not replace files in use left them queued for the
        // next restart. Installing over that leaves the queued, older files to be copied over the new
        // ones at the restart, so the customer is asked to restart first.
        [CustomAction]
        public static ActionResult WarnIfMidiRestartPending(Session session)
        {
            try
            {
                var folder = session[RestartCheckFolderProperty];

                if (!HasPendingFileOperations(session, folder))
                {
                    session.Log("WarnIfMidiRestartPending: No restart pending for Windows MIDI Services.");
                    return ActionResult.Success;
                }

                using (var record = new Record(1))
                {
                    record[1] = ErrorRestartAlreadyPending;

                    var result = session.Message(
                        InstallMessage.Warning | (InstallMessage)MessageButtons.OKCancel | (InstallMessage)MessageIcon.Warning | (InstallMessage)MessageDefaultButton.Button2,
                        record);

                    session.Log($"WarnIfMidiRestartPending: Customer answered {result}.");

                    // A silent or passive install has nobody to ask and gets no answer here, so it
                    // goes ahead and relies on the restart required result at the end.
                    if (result == MessageResult.Cancel)
                    {
                        return ActionResult.UserExit;
                    }
                }
            }
            catch (Exception ex)
            {
                session.Log("WarnIfMidiRestartPending: Exception, continuing. " + ex.ToString());
            }

            return ActionResult.Success;
        }

        // Immediate, after InstallFinalize. Anything still queued for the next restart in this
        // package's folder (files that were in use, or the marker written when the MIDI service
        // would not stop) means the installer must report that a restart is required.
        [CustomAction]
        public static ActionResult RequireRestartIfMidiFilesPending(Session session)
        {
            try
            {
                var alreadyRequired = session.GetMode(InstallRunMode.RebootAtEnd);
                var pending = HasPendingFileOperations(session, session[RestartCheckFolderProperty]);

                session.Log($"RequireRestartIfMidiFilesPending: RebootAtEnd already set: {alreadyRequired}. Pending file operations: {pending}.");

                if (alreadyRequired || pending)
                {
                    session.SetMode(InstallRunMode.RebootAtEnd, true);
                    session["MIDIRESTARTREQUIRED"] = "1";
                }
            }
            catch (Exception ex)
            {
                session.Log("RequireRestartIfMidiFilesPending: Exception. " + ex.ToString());
            }

            return ActionResult.Success;
        }

        private static bool HasPendingFileOperations(Session session, string folder)
        {
            string folderPrefix = null;

            if (!string.IsNullOrWhiteSpace(folder))
            {
                folderPrefix = folder.Trim();

                if (!folderPrefix.EndsWith("\\", StringComparison.Ordinal))
                {
                    folderPrefix += "\\";
                }
            }

            using (var hklm = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64))
            using (var key = hklm.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\Session Manager"))
            {
                if (key == null)
                {
                    return false;
                }

                foreach (var valueName in new[] { "PendingFileRenameOperations", "PendingFileRenameOperations2" })
                {
                    if (!(key.GetValue(valueName) is string[] entries))
                    {
                        continue;
                    }

                    foreach (var entry in entries)
                    {
                        var path = NormalizePendingPath(entry);

                        if (path.Length == 0)
                        {
                            continue;
                        }

                        var inFolder = folderPrefix != null && path.StartsWith(folderPrefix, StringComparison.OrdinalIgnoreCase);
                        var isMarker = string.Equals(Path.GetFileName(path), RestartMarkerFileName, StringComparison.OrdinalIgnoreCase);

                        if (inFolder || isMarker)
                        {
                            session.Log($"Pending restart operation found in {valueName}: \"{path}\".");
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        // Entries look like "\??\C:\Program Files\...", with flags in front of that such as "!" (the
        // target may be replaced) or "*1". The list alternates sources and targets, and a delete
        // has an empty target.
        private static string NormalizePendingPath(string entry)
        {
            if (string.IsNullOrWhiteSpace(entry))
            {
                return string.Empty;
            }

            var path = entry.Trim();
            var prefix = path.IndexOf(@"\??\", StringComparison.Ordinal);

            return prefix >= 0 ? path.Substring(prefix + 4) : path.TrimStart('!', '*');
        }

        // ------------------------------------------------------------------------------------------
        // MIDI service
        // ------------------------------------------------------------------------------------------

        // Immediate, before InstallValidate. That is before Windows Installer checks for files in use
        // and before an earlier version is removed, so neither of those finds the service holding
        // the transports. Under the bundle this runs elevated. If it cannot stop the service it says
        // so in the log and leaves the decision to the deferred action, which runs right before
        // files are replaced.
        [CustomAction]
        public static ActionResult StopMidiServiceBeforeInstall(Session session)
        {
            try
            {
                var stopped = StopMidiService(session, TimeSpan.FromSeconds(DefaultEarlyStopSeconds));

                session["MIDIDEFERREDSTOPSECONDS"] = (stopped ? DefaultDeferredStopSeconds : DeferredStopSecondsAfterEarlyFailure).ToString();
            }
            catch (Exception ex)
            {
                session.Log("StopMidiServiceBeforeInstall: Exception, continuing. " + ex.ToString());
            }

            return ActionResult.Success;
        }

        // Deferred, not impersonated, before StopServices. An earlier version's removal starts the
        // service again when it finishes, and any MIDI app can start it through its trigger, so it
        // is stopped again here, just before files are removed or replaced. If it will not stop,
        // the marker is queued so RequireRestartIfMidiFilesPending reports restart required.
        [CustomAction]
        public static ActionResult StopMidiServiceDeferred(Session session)
        {
            try
            {
                var seconds = DefaultDeferredStopSeconds;

                if (session.CustomActionData.TryGetValue("TimeoutSeconds", out var value) &&
                    int.TryParse(value, out var parsed) &&
                    parsed > 0)
                {
                    seconds = parsed;
                }

                if (!StopMidiService(session, TimeSpan.FromSeconds(seconds)))
                {
                    session.Log("StopMidiServiceDeferred: The MIDI service did not stop. A restart will be required.");
                    QueueRestartMarker(session);
                }
            }
            catch (Exception ex)
            {
                session.Log("StopMidiServiceDeferred: Exception. A restart will be required. " + ex.ToString());

                try
                {
                    QueueRestartMarker(session);
                }
                catch (Exception markerEx)
                {
                    session.Log("StopMidiServiceDeferred: Could not queue the restart marker. " + markerEx.ToString());
                }
            }

            return ActionResult.Success;
        }

        // True when the service is stopped (or not installed) and stayed stopped.
        private static bool StopMidiService(Session session, TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;

            ServiceController service;

            try
            {
                service = new ServiceController(MidiServiceName);
                session.Log($"StopMidiService: {MidiServiceName} is {service.Status}. Allowing up to {timeout.TotalSeconds} seconds to stop it.");
            }
            catch (InvalidOperationException ex) when (ex.InnerException is Win32Exception w && w.NativeErrorCode == 1060)
            {
                // ERROR_SERVICE_DOES_NOT_EXIST
                session.Log($"StopMidiService: {MidiServiceName} is not installed.");
                return true;
            }

            using (service)
            {
                var stopRequests = 0;

                while (true)
                {
                    service.Refresh();
                    var status = service.Status;

                    if (status == ServiceControllerStatus.Stopped)
                    {
                        Thread.Sleep(StoppedSettleTime);
                        service.Refresh();

                        if (service.Status == ServiceControllerStatus.Stopped)
                        {
                            session.Log($"StopMidiService: {MidiServiceName} is stopped.");
                            return true;
                        }

                        session.Log($"StopMidiService: {MidiServiceName} was started again by a client. Now {service.Status}.");
                        status = service.Status;
                    }

                    if (DateTime.UtcNow >= deadline)
                    {
                        session.Log($"StopMidiService: {MidiServiceName} is still {status} after {timeout.TotalSeconds} seconds and {stopRequests} stop requests.");
                        return false;
                    }

                    if (status == ServiceControllerStatus.Running || status == ServiceControllerStatus.Paused)
                    {
                        stopRequests++;

                        try
                        {
                            session.Log($"StopMidiService: Stop request {stopRequests}.");
                            service.Stop();
                        }
                        catch (InvalidOperationException ex)
                        {
                            session.Log($"StopMidiService: Stop request {stopRequests} failed. {ex.Message} {ex.InnerException?.Message}");

                            if (ex.InnerException is Win32Exception w && w.NativeErrorCode == 5)
                            {
                                // ERROR_ACCESS_DENIED: not elevated, so waiting will not help
                                return false;
                            }
                        }
                    }

                    var remaining = deadline - DateTime.UtcNow;
                    var wait = remaining < StopRequestWait ? remaining : StopRequestWait;

                    if (wait > TimeSpan.Zero)
                    {
                        try
                        {
                            service.WaitForStatus(ServiceControllerStatus.Stopped, wait);
                        }
                        catch (System.ServiceProcess.TimeoutException)
                        {
                            service.Refresh();
                            session.Log($"StopMidiService: {MidiServiceName} is {service.Status} after waiting {wait.TotalSeconds:0} seconds.");
                        }
                    }
                }
            }
        }

        private static void QueueRestartMarker(Session session)
        {
            var path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "Temp", RestartMarkerFileName);

            File.WriteAllText(path, "Windows MIDI Services setup could not stop the MIDI service. Windows deletes this file at the next restart.");

            if (!MoveFileEx(path, null, MOVEFILE_DELAY_UNTIL_REBOOT))
            {
                session.Log($"QueueRestartMarker: MoveFileEx failed with {Marshal.GetLastWin32Error()} for \"{path}\".");
                return;
            }

            session.Log($"QueueRestartMarker: Queued \"{path}\" for deletion at the next restart.");
        }

        // ------------------------------------------------------------------------------------------
        // MIDI apps in the Tools folder
        // ------------------------------------------------------------------------------------------

        // Immediate, before InstallValidate, so the files in use check and the removal of an earlier
        // version find nothing running. Reads TOOLSROOT_INSTALLFOLDER.
        //
        // The notifications app has no data to lose. It is asked to close and is ended if it does not.
        // The other apps are only asked to close, the same as clicking their close button, so an app
        // with unsaved work can ask the customer about it. Anything still running afterwards is left
        // to Windows Installer, which lists it in the files in use dialog, or replaces its files at
        // the next restart and reports restart required.
        [CustomAction]
        public static ActionResult CloseMidiTools(Session session)
        {
            var processes = new List<Process>();

            try
            {
                var folder = session["TOOLSROOT_INSTALLFOLDER"];

                if (string.IsNullOrWhiteSpace(folder))
                {
                    session.Log("CloseMidiTools: TOOLSROOT_INSTALLFOLDER is not set.");
                    return ActionResult.Success;
                }

                var folderPrefix = folder.EndsWith("\\", StringComparison.Ordinal) ? folder : folder + "\\";
                var currentSessionId = Process.GetCurrentProcess().SessionId;

                foreach (var process in Process.GetProcesses())
                {
                    var path = GetProcessImagePath(process.Id);

                    if (path != null && path.StartsWith(folderPrefix, StringComparison.OrdinalIgnoreCase))
                    {
                        session.Log($"CloseMidiTools: Running: \"{path}\" (process {process.Id}, session {process.SessionId}).");
                        processes.Add(process);
                    }
                    else
                    {
                        process.Dispose();
                    }
                }

                if (processes.Count == 0)
                {
                    session.Log("CloseMidiTools: No MIDI apps are running.");
                    return ActionResult.Success;
                }

                var notifications = new List<Process>();
                var tools = new List<Process>();

                foreach (var process in processes)
                {
                    var isNotifications = string.Equals(Path.GetFileName(GetProcessImagePath(process.Id) ?? string.Empty), NotificationsExeName, StringComparison.OrdinalIgnoreCase);

                    (isNotifications ? notifications : tools).Add(process);

                    if (isNotifications && process.SessionId == currentSessionId)
                    {
                        // Started again after the install, for the customer running setup.
                        session["MIDINOTIFICATIONSWASRUNNING"] = "1";
                    }
                }

                PostCloseToNotificationsWindows(session, notifications);
                PostCloseToTopLevelWindows(session, tools);

                foreach (var process in WaitForExit(notifications, NotificationsCloseWait))
                {
                    try
                    {
                        session.Log($"CloseMidiTools: Ending {NotificationsExeName} (process {process.Id}).");
                        process.Kill();
                        process.WaitForExit(5000);
                    }
                    catch (Exception ex)
                    {
                        session.Log($"CloseMidiTools: Could not end process {process.Id}. {ex.Message}");
                    }
                }

                foreach (var process in WaitForExit(tools, ToolsCloseWait))
                {
                    session.Log($"CloseMidiTools: Process {process.Id} is still running. Windows Installer will handle its files.");
                }
            }
            catch (Exception ex)
            {
                session.Log("CloseMidiTools: Exception, continuing. " + ex.ToString());
            }
            finally
            {
                foreach (var process in processes)
                {
                    process.Dispose();
                }
            }

            return ActionResult.Success;
        }

        // Its only window is a message-only window, which EnumWindows does not return. WM_CLOSE
        // destroys that window and the app exits cleanly, removing its tray icon.
        private static void PostCloseToNotificationsWindows(Session session, List<Process> notifications)
        {
            if (notifications.Count == 0)
            {
                return;
            }

            var ids = new HashSet<int>();
            foreach (var process in notifications)
            {
                ids.Add(process.Id);
            }

            var window = IntPtr.Zero;

            while ((window = FindWindowEx(HWND_MESSAGE, window, NotificationsWindowClass, null)) != IntPtr.Zero)
            {
                GetWindowThreadProcessId(window, out var processId);

                if (ids.Contains((int)processId))
                {
                    session.Log($"CloseMidiTools: Asking {NotificationsExeName} (process {processId}) to close.");
                    PostMessage(window, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);
                }
            }
        }

        private static void PostCloseToTopLevelWindows(Session session, List<Process> tools)
        {
            if (tools.Count == 0)
            {
                return;
            }

            var ids = new HashSet<int>();
            foreach (var process in tools)
            {
                ids.Add(process.Id);
            }

            var className = new StringBuilder(256);

            EnumWindows((window, _) =>
            {
                GetWindowThreadProcessId(window, out var processId);

                if (!ids.Contains((int)processId) || GetWindow(window, GW_OWNER) != IntPtr.Zero)
                {
                    return true;
                }

                className.Clear();
                GetClassName(window, className, className.Capacity);

                // An app minimized to the notification area may have hidden its main window.
                if (IsWindowVisible(window) || string.Equals(className.ToString(), WinUIWindowClass, StringComparison.Ordinal))
                {
                    session.Log($"CloseMidiTools: Asking process {processId} to close its window ({className}).");
                    PostMessage(window, WM_CLOSE, IntPtr.Zero, IntPtr.Zero);
                }

                return true;
            }, IntPtr.Zero);
        }

        // Returns the processes still running when the time is up.
        private static List<Process> WaitForExit(List<Process> processes, TimeSpan timeout)
        {
            var deadline = DateTime.UtcNow + timeout;
            var stillRunning = new List<Process>();

            foreach (var process in processes)
            {
                try
                {
                    var remaining = deadline - DateTime.UtcNow;
                    var milliseconds = remaining > TimeSpan.Zero ? (int)remaining.TotalMilliseconds : 0;

                    if (!process.WaitForExit(milliseconds))
                    {
                        stillRunning.Add(process);
                    }
                }
                catch (Exception)
                {
                    // Could not wait on it; treat it as still running rather than assume it exited.
                    stillRunning.Add(process);
                }
            }

            return stillRunning;
        }

        private static string GetProcessImagePath(int processId)
        {
            var handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, processId);

            if (handle == IntPtr.Zero)
            {
                return null;
            }

            try
            {
                var buffer = new StringBuilder(1024);
                var size = buffer.Capacity;

                return QueryFullProcessImageName(handle, 0, buffer, ref size) ? buffer.ToString() : null;
            }
            finally
            {
                CloseHandle(handle);
            }
        }

        // ------------------------------------------------------------------------------------------
        // Win32
        // ------------------------------------------------------------------------------------------

        private const uint PROCESS_QUERY_LIMITED_INFORMATION = 0x1000;
        private const uint WM_CLOSE = 0x0010;
        private const uint GW_OWNER = 4;
        private const uint MOVEFILE_DELAY_UNTIL_REBOOT = 0x4;
        private static readonly IntPtr HWND_MESSAGE = new IntPtr(-3);

        private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, int processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "QueryFullProcessImageNameW")]
        private static extern bool QueryFullProcessImageName(IntPtr process, uint flags, StringBuilder exeName, ref int size);

        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode, EntryPoint = "MoveFileExW")]
        private static extern bool MoveFileEx(string existingFileName, string newFileName, uint flags);

        [DllImport("user32.dll")]
        private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "FindWindowExW")]
        private static extern IntPtr FindWindowEx(IntPtr parent, IntPtr childAfter, string className, string windowName);

        [DllImport("user32.dll")]
        private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

        [DllImport("user32.dll")]
        private static extern IntPtr GetWindow(IntPtr window, uint command);

        [DllImport("user32.dll")]
        private static extern bool IsWindowVisible(IntPtr window);

        [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "GetClassNameW")]
        private static extern int GetClassName(IntPtr window, StringBuilder className, int maxCount);

        [DllImport("user32.dll", SetLastError = true)]
        private static extern bool PostMessage(IntPtr window, uint message, IntPtr wparam, IntPtr lparam);
    }
}
