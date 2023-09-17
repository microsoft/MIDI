// THIS IS A DEVELOPER-ONLY WORKAROUND THAT IS NOT ENDORSED OR SUPPORTED BY MICROSOFT

using Microsoft.Win32;
using System;
using System.Collections.Generic;
using System.Drawing.Text;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using WixToolset.Dtf.WindowsInstaller;
using static System.Collections.Specialized.BitVector32;

namespace RegistryCustomActions
{
    public class CustomActions
    {
        [CustomAction()]
        public static ActionResult GrantPermissions(Session session)
        {
            //session.Log("REGKEYHACK: GrantPermissions");

            //TakeKeyPermissions(session);

            //session.Log("REGKEYHACK: GrantPermissions Leaving");

            return ActionResult.Failure;
        }

        [CustomAction()]
        public static ActionResult ReturnToDefaultPermissions(Session session)
        {
//            session.Log("REGKEYRESTORE: ReturnToDefaultPermissions");

 //           RestoreKeyPermissions(session);

//            session.Log("REGKEYRESTORE: ReturnToDefaultPermissions Leaving");

            return ActionResult.Failure;

        }

        [CustomAction()]
        public static ActionResult WriteActivatableClassIdKeys(Session session)
        {
            session.Log("REGKEYWRITE: WriteTestRegKey");

            string path = System.IO.Path.Combine(session["API_INSTALLFOLDER"], "Windows.Devices.Midi2.dll");

            WriteRegistryKeys(session, path, true);

            session.Log("REGKEYWRITE: WriteTestRegKey Leaving");

            return ActionResult.Success;
        }

        [CustomAction()]
        public static ActionResult EraseActivatableClassIdKeys(Session session)
        {
            session.Log("REGKEYERASE: WriteTestRegKey");

            //string path = session["MIDISERVICES_API_DLL_PATH"];
            string path = "";

            WriteRegistryKeys(session, path, false);

            session.Log("REGKEYERASE: WriteTestRegKey Leaving");

            return ActionResult.Success;
        }










        //const string rootKey = "SOFTWARE\\Microsoft\\WindowsRuntime\\ActivatableClassId";
        const string windowsRuntimeRootKey = "SOFTWARE\\Microsoft\\WindowsRuntime";
        const string activatableClassIdKey = windowsRuntimeRootKey + "\\ActivatableClassId";

        //const string administratorsGroupSID = "S-1-5-32-544";
        const string trustedInstallerSID = "S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";    // NT SERVICE\TrustedInstaller


        // Code shamelessly taken from https://stackoverflow.com/questions/5510311/how-can-i-manipulate-token-privileges-in-net
        // which is based on this 2005 MSDN article http://msdn.microsoft.com/en-us/magazine/cc163823.aspx


        private static RegistryKey OpenWindowsActivatableClassIdKey(Session session)
        {
            // in case the installer is launching us in a 32 bit process
            var baseKey = RegistryKey.OpenBaseKey(RegistryHive.LocalMachine, RegistryView.Registry64);
            if (baseKey == null)
            {
                session.Log($"REGKEYHACK: Base HKLM key is null");
                return null;
            }

            string keyPath = activatableClassIdKey;

            // Let's get to that registry entry
            session.Log($"REGKEYHACK: Attempting to open registry key {baseKey.ToString()} {keyPath}");
            var regKey = baseKey.OpenSubKey(
                keyPath,
                Microsoft.Win32.RegistryKeyPermissionCheck.ReadWriteSubTree,
                System.Security.AccessControl.RegistryRights.TakeOwnership);

            if (regKey == null)
            {
                session.Log($"REGKEYHACK: reg key is null");
                return null;
            }
            else
            {
                session.Log($"REGKEYHACK: opened {regKey.ToString()}");
            }

            return regKey; 
        }

        private static WindowsIdentity GetCurrentUser(Session session)
        {
            var currentUserIdentity = WindowsIdentity.GetCurrent();

            if (currentUserIdentity == null)
            {
                session.Log($"REGKEYHACK: Current user identity is null");
                return null;
            }
            session.Log($"REGKEYHACK: Current user={currentUserIdentity.Name}");

            return currentUserIdentity;
        }


        private static void WriteRegistryKeys(Session session, string apiDllPath, bool installMode)
        {
            var privs = new[] { Privilege.TakeOwnership, Privilege.Backup, Privilege.Restore };

            session.Log($"REGKEYHACK: DLL path is {apiDllPath}");

            session.Log("REGKEYHACK: Attempting to run with privileges");

            Privilege.RunWithPrivileges(() =>
            {
                session.Log("REGKEYHACK: Getting current user");

                // get the current running user 
                var currentUserIdentity = GetCurrentUser(session);
                if (currentUserIdentity == null)
                {
                    session.Log($"REGKEYHACK: Aborting");
                    return;
                }

                // open the Windows Runtime key
                var classIdRootKey = OpenWindowsActivatableClassIdKey(session);
                if (classIdRootKey == null)
                {
                    session.Log($"REGKEYHACK: Aborting");
                    return;
                }

                var registrySecurity = classIdRootKey.GetAccessControl(System.Security.AccessControl.AccessControlSections.All);
                if (registrySecurity == null)
                {
                    session.Log($"REGKEYHACK: Aborting: GetAccessControl return null ACL");
                    return;
                }

                // and now take ownership so we can write to it. Normally, this key and all subkeys
                // are owned by TrustedInstaller. This is something we're doing ONLY for a limited 
                // developer preview. This should never be done on a production machine as it 
                // compromises WinRT / UWP security.

                session.Log("REGKEYHACK: Setting the reg key owner to the current user");
                registrySecurity.SetOwner(currentUserIdentity.User);
                classIdRootKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Setting access rule protection on that reg key");
                registrySecurity.SetAccessRuleProtection(false, true);
                classIdRootKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Creating the full access rule");
                var fullAccessRule = new RegistryAccessRule(
                    currentUserIdentity.Name,
                    RegistryRights.FullControl,
                    InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                    PropagationFlags.None,
                    AccessControlType.Allow);

                if (fullAccessRule == null)
                {
                    session.Log($"REGKEYHACK: Creating Registry Access Rule return null");
                    return;
                }
                registrySecurity.AddAccessRule(fullAccessRule);

                // sometimes need to set on the activatableclassid key as well

                session.Log("REGKEYHACK: Setting ACL on the key to allow full access by the current user");
                classIdRootKey.SetAccessControl(registrySecurity);

                foreach (RegistryAccessRule ar in
                    registrySecurity.GetAccessRules(true, true, typeof(NTAccount)))
                {
                    if (ar.IdentityReference.Value == currentUserIdentity.Name)
                    {
                        session.Log($"Permissions for {currentUserIdentity.Name} on {classIdRootKey.Name}");
                        session.Log("           Type: {0}", ar.AccessControlType);
                        session.Log("         Rights: {0}", ar.RegistryRights);
                        session.Log("    Inheritance: {0}", ar.InheritanceFlags);
                        session.Log("    Propagation: {0}", ar.PropagationFlags);
                        session.Log("      Inherited? {0}", ar.IsInherited);
                        //session.Log("---------------------------------------------");
                    }
                }

                // Permissions are set. Now to write the keys

                try
                {
                    RegistryEntries entries = new RegistryEntries();

                    foreach (var regEntry in entries.ActivationEntries)
                    {
                        if (installMode)
                        {
                            var activationKey = classIdRootKey.CreateSubKey(regEntry.ClassName);

                            activationKey.SetValue("ActivationType", regEntry.ActivationType, RegistryValueKind.DWord);
                            activationKey.SetValue("DllPath", apiDllPath, RegistryValueKind.String);
                            activationKey.SetValue("Threading", regEntry.Threading, RegistryValueKind.DWord);
                            activationKey.SetValue("TrustLevel", regEntry.TrustLevel, RegistryValueKind.DWord);

                            activationKey.Close();

                            session.Log($"Wrote {regEntry.ClassName} pointing to {apiDllPath}");

                        }
                        else
                        {
                            // we're uninstalling, so nuke 'em
                            classIdRootKey.DeleteSubKey(regEntry.ClassName, false);
                        }

                    }

                }
                catch (Exception ex) 
                {
                    session.Log(ex.ToString());
                }

                // And now to reset the permissions

                // grant ownership back to Trusted Installer
                var trustedInstallerSecurityIdentifier = new SecurityIdentifier(trustedInstallerSID);

                session.Log("REGKEYRESTORE: Setting the reg key owner to TrustedInstaller");
                var trustedInstallerSid = new SecurityIdentifier(trustedInstallerSID);
                registrySecurity.SetOwner(trustedInstallerSecurityIdentifier);
                classIdRootKey.SetAccessControl(registrySecurity);


                session.Log("REGKEYRESTORE: Setting access rule protection on that reg key");
                registrySecurity.SetAccessRuleProtection(false, true);
                classIdRootKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYRESTORE: Deleting current user's registry rights for this key");

                var removalRule = new RegistryAccessRule(
                    currentUserIdentity.Name,
                    RegistryRights.ReadKey,
                    AccessControlType.Allow);

                registrySecurity.RemoveAccessRuleAll(removalRule);
                classIdRootKey.SetAccessControl(registrySecurity);

                //registrySecurity.RemoveAccessRuleAll(fullAccessRuleForCurrentUser);

                //session.Log("REGKEYRESTORE: Setting ACL on the key");
                //regKey.SetAccessControl(registrySecurity);


                session.Log("REGKEYRESTORE: Complete");



            }, privs);

            session.Log("REGKEYHACK: Done");

        }



#if false
        private static void TakeKeyPermissions(Session session)
        {
            //bool previousValue;

            //session.Log("REGKEYHACK: Adjusting privileges with RtlAdjustPrivilege");
            // give them permission to take ownership
            //var ownershipResult = RtlAdjustPrivilege(SeTakeOwnership, true, false, out previousValue);
            //var backupResult = RtlAdjustPrivilege(SeBackup, true, false, out previousValue);
            //var restoreResult = RtlAdjustPrivilege(SeRestore, true, false, out previousValue);
            //session.Log($"REGKEYHACK: ownershipResult={ownershipResult.ToString("X8")}, backupResult={backupResult.ToString("X8")}, restoreResult={restoreResult.ToString("X8")}");

            var privs = new[] { Privilege.TakeOwnership, Privilege.Backup, Privilege.Restore };

            session.Log("REGKEYHACK: Attempting to run with privileges");

            Privilege.RunWithPrivileges(() =>
            {
                session.Log("REGKEYHACK: Getting current user");

                // get the current running user 
                var currentUserIdentity = GetCurrentUser(session);
                if (currentUserIdentity == null)
                {
                    session.Log($"REGKEYHACK: Aborting");
                    return;
                }

                // open the Windows Runtime key
                var windowsRuntimeKey = OpenWindowsRuntimeKey(session);
                if (windowsRuntimeKey == null)
                {
                    session.Log($"REGKEYHACK: Aborting");
                    return;
                }

                var registrySecurity = windowsRuntimeKey.GetAccessControl(System.Security.AccessControl.AccessControlSections.All);
                if (registrySecurity == null)
                {
                    session.Log($"REGKEYHACK: Aborting: GetAccessControl return null ACL");
                    return;
                }

                // and now take ownership so we can write to it. Normally, this key and all subkeys
                // are owned by TrustedInstaller. This is something we're doing ONLY for a limited 
                // developer preview. This should never be done on a production machine as it 
                // compromises WinRT / UWP security.

                session.Log("REGKEYHACK: Setting the reg key owner to the current user");
                registrySecurity.SetOwner(currentUserIdentity.User);
                windowsRuntimeKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Setting access rule protection on that reg key");
                registrySecurity.SetAccessRuleProtection(false, true);
                windowsRuntimeKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYHACK: Creating the full access rule");
                var fullAccessRule = new RegistryAccessRule(
                    currentUserIdentity.Name,
                    RegistryRights.FullControl,
                    InheritanceFlags.ContainerInherit | InheritanceFlags.ObjectInherit,
                    PropagationFlags.None, 
                    AccessControlType.Allow);

                if (fullAccessRule == null)
                {
                    session.Log($"REGKEYHACK: Creating Registry Access Rule return null");
                    return;
                }
                registrySecurity.AddAccessRule(fullAccessRule);

                // sometimes need to set on the activatableclassid key as well


                session.Log("REGKEYHACK: Setting ACL on the key to allow full access by the current user");
                windowsRuntimeKey.SetAccessControl(registrySecurity);

                foreach (RegistryAccessRule ar in
                    registrySecurity.GetAccessRules(true, true, typeof(NTAccount)))
                {
                    if (ar.IdentityReference.Value == currentUserIdentity.Name)
                    {
                        session.Log($"Permissions for {currentUserIdentity.Name} on {windowsRuntimeKey.Name}");
                        session.Log("           Type: {0}", ar.AccessControlType);
                        session.Log("         Rights: {0}", ar.RegistryRights);
                        session.Log("    Inheritance: {0}", ar.InheritanceFlags);
                        session.Log("    Propagation: {0}", ar.PropagationFlags);
                        session.Log("      Inherited? {0}", ar.IsInherited);
                        //session.Log("---------------------------------------------");
                    }
                }



                session.Log("REGKEYHACK: Complete");
            }, privs);
        }




        private static void RestoreKeyPermissions(Session session)
        {
            var privs = new[] { Privilege.TakeOwnership, Privilege.Backup, Privilege.Restore };

            session.Log("REGKEYRESTORE: Attempting to run with privileges");

            Privilege.RunWithPrivileges(() =>
            {
                session.Log("REGKEYRESTORE: Getting current user");

                // get the current running user 
                var currentUserIdentity = GetCurrentUser(session);

                var regKey = OpenWindowsRuntimeKey(session);               

                var registrySecurity = regKey.GetAccessControl(System.Security.AccessControl.AccessControlSections.All);

                if (registrySecurity == null)
                {
                    session.Log($"REGKEYRESTORE: GetAccessControl return null ACL");
                    return;
                }

                // grant ownership to Trusted Installer

                var trustedInstallerSecurityIdentifier = new SecurityIdentifier(trustedInstallerSID);

                session.Log("REGKEYRESTORE: Setting the reg key owner to TrustedInstaller");
                var trustedInstallerSid = new SecurityIdentifier(trustedInstallerSID);
                registrySecurity.SetOwner(trustedInstallerSecurityIdentifier);
                regKey.SetAccessControl(registrySecurity);


                session.Log("REGKEYRESTORE: Setting access rule protection on that reg key");
                registrySecurity.SetAccessRuleProtection(false, true);
                regKey.SetAccessControl(registrySecurity);

                session.Log("REGKEYRESTORE: Deleting current user's registry rights for this key");

                var removalRule = new RegistryAccessRule(
                    currentUserIdentity.Name,
                    RegistryRights.ReadKey,
                    AccessControlType.Allow);

                registrySecurity.RemoveAccessRuleAll(removalRule);
                regKey.SetAccessControl(registrySecurity);

                //registrySecurity.RemoveAccessRuleAll(fullAccessRuleForCurrentUser);

                //session.Log("REGKEYRESTORE: Setting ACL on the key");
                //regKey.SetAccessControl(registrySecurity);


                session.Log("REGKEYRESTORE: Complete");
            }, privs);
        }

#endif
    }
}
