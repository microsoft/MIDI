// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Windows.Midi.PluginModel.Plugin;
using System.Reflection;

namespace MidiPluginLoader
{
    // This is a separate class and assembly so we can create a test tool
    // which plugin devs can use to test their plugin using the same
    // code as the service

    public class MidiPluginLoader
    {

        // Loads the plugin assembly and returns the IMidiPlugin it hosts
        // One plugin per assembly
        public IMidiDevicePlugin? LoadDevicePlugin(string pluginRootPath, string subfolder, string assemblyDllName, string pluginClassName)
        {
            string fullPath = Path.Combine(pluginRootPath, subfolder, assemblyDllName);

            if (Path.Exists(fullPath))
            {
                Assembly? assembly = null;

                // Several different types of exceptions to catch here. They're all
                // fatal, though, so we'll just let them bubble up.

                assembly = Assembly.LoadFrom(fullPath);

                if (assembly != null)
                {
                    var type = assembly.GetType(pluginClassName);

                    if (type != null)
                    {
                        var plugin = Activator.CreateInstance(type) as IMidiDevicePlugin;

                        if (plugin != null)
                        {
                            // we're good

                            return plugin;
                        }
                        else
                        {
                            // couldn't create type.
                            // throw new
                            return null;
                        }
                    }
                    else
                    {
                        // type doesn't exist
                        // throw new
                        return null;
                    }

                }
                else
                {
                    // couldn't load assembly
                    // throw new
                    return null;
                }

            }
            else
            {
                // path does not exist
                //throw new
                return null;
            }
        }


        public IMidiMessageProcessorPlugin LoadMessageProcessorPlugin(string pluginRootPath, string subfolder, string assemblyDllName, string pluginClassName)
        {
            throw new NotImplementedException();
        }











    }
}