// Copyright (c) The NodeRT Contributors
// All rights reserved. 
//
// Licensed under the Apache License, Version 2.0 (the ""License""); you may
// not use this file except in compliance with the License. You may obtain a
// copy of the License at http://www.apache.org/licenses/LICENSE-2.0 
//
// THIS CODE IS PROVIDED ON AN  *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
// OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY
// IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT. 
//
// See the Apache Version 2.0 License for specific language governing permissions
// and limitations under the License.

using System;
using System.Diagnostics;

namespace NodeRTLib
{
    // Provides helper methods for compiling the genrated projects and copying the needed files for a package to an output directory
    public class NodeRTProjectBuildUtils
    {
        private const string NODE_NPM_INSTALL_CMD_TEMPLATE = "\"cd \"{0}\" & npm install --ignore-scripts\"";
        private const string NODE_GYP_REBUILD_CMD_TEMPLATE = "\"cd \"{0}\" & node-gyp rebuild --msvs_version={1}\"";

        // Builds the given project/sln for the given platforms and copies the output & package file to the output directory
        public static void BuildWithNodeGyp(string moduleDirectory, VsVersions vsVersion, bool verbose = false)
        {
            BuildModule(moduleDirectory, vsVersion, verbose);
        }

        private static string CreateBuildCmd(string moduleDirectory, VsVersions vsVersion)
        {
            string versionString;

            switch (vsVersion)
            {
                case VsVersions.Vs2012:
                    versionString = "2012";
                    break;
                case VsVersions.Vs2013:
                    versionString = "2013";
                    break;
                case VsVersions.Vs2015:
                    versionString = "2015";
                    break;
                case VsVersions.Vs2017:
                    versionString = "2017";
                    break;
                case VsVersions.Vs2019:
                    versionString = "2019";
                    break;
                case VsVersions.Vs2022:
                    versionString = "2022";
                    break;
                default:
                    throw new Exception("Unknown VS Version");
            }

            string buildCmd = String.Format(NODE_GYP_REBUILD_CMD_TEMPLATE, moduleDirectory, versionString);

            Console.WriteLine("Node-Gyp Build Command: " + buildCmd);

            return buildCmd;
        }

        private static string CreateNpmInstallCmd(string moduleDirectory, VsVersions vsVersion)
        {
            string cmd = String.Format(NODE_NPM_INSTALL_CMD_TEMPLATE, moduleDirectory);

            Console.WriteLine("NPM Install Command: " + cmd);

            return cmd;
        }


        private static void BuildModule(string moduleDirectory, VsVersions vsVersion, bool verbose)
        {
            Console.WriteLine("BuildModule: moduleDirectory: " + moduleDirectory);
            Console.WriteLine("BuildModule: vsVersion:       " + vsVersion.ToString());
            Console.WriteLine("BuildModule: verbose:         " + verbose.ToString());

            string installCmd = CreateNpmInstallCmd(moduleDirectory, vsVersion);
            bool installResult = ExecuteCommand(installCmd, verbose);

            if (!installResult)
                throw new Exception("Failed to build project. NPM Install Command returned false.");

            string buildCmd = CreateBuildCmd(moduleDirectory, vsVersion);
            bool buildResult = ExecuteCommand(installCmd, verbose);

            if (!buildResult)
                throw new Exception("Failed to build project. Rebuild Command returned false.");
        }

        private static bool ExecuteCommand(string cmd, bool verbose)
        {
            Process process = new Process();
            process.StartInfo.FileName = "cmd.exe";
            process.StartInfo.Arguments = "/c " + cmd;
            process.StartInfo.CreateNoWindow = true;
            process.StartInfo.UseShellExecute = false;
            process.StartInfo.LoadUserProfile = true;

            if (process.StartInfo.EnvironmentVariables.ContainsKey("VisualStudioDir"))
                process.StartInfo.EnvironmentVariables.Remove("VisualStudioDir");
            if (process.StartInfo.EnvironmentVariables.ContainsKey("VisualStudioEdition"))
                process.StartInfo.EnvironmentVariables.Remove("VisualStudioEdition");
            if (process.StartInfo.EnvironmentVariables.ContainsKey("VisualStudioVersion"))
                process.StartInfo.EnvironmentVariables.Remove("VisualStudioVersion");

            if (verbose)
            {
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.RedirectStandardError = true;

                Action<object, DataReceivedEventArgs> actionWrite = (sender, e) =>
                {
                    Console.WriteLine(e.Data);
                };

                process.ErrorDataReceived += (sender, e) => actionWrite(sender, e);
                process.OutputDataReceived += (sender, e) => actionWrite(sender, e);
            }

            process.Start();

            if (verbose)
            {
                process.BeginOutputReadLine();
                process.BeginErrorReadLine();
            }

            process.WaitForExit();
            bool success = (process.ExitCode == 0);

            process.Close();

            return success;
        }
    }
}
