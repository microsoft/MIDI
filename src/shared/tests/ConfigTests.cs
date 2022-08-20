// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Microsoft.Extensions.Logging;
using Microsoft.VisualStudio.TestTools.UnitTesting.Logging;
using Microsoft.Windows.Midi.Internal.Config.Schema;

namespace SharedLibraryTests
{
    [TestClass]
    public class ConfigTests
    {

        //[TestMethod]
        //public void TestCreateConfigFile()
        //{
        //    MidiServicesConfig config = new MidiServicesConfig();

        //    config.LoadDefaults();

        //    config.Save();
        //}


        //[TestMethod]
        //public void TestLoadConfigFile()
        //{
        //    MidiServicesConfig config = new MidiServicesConfig();

        //    config.Load();
        //}


        [TestMethod]
        public void TestAddLoopbackPluginToDefaultConfig()
        {
            using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
            var logger = loggerFactory.CreateLogger("Unit Test");

            MidiServicesConfig config = new MidiServicesConfig(logger);

            // first run
            if (!config.Load())
            {
                // create new config
                System.Diagnostics.Debug.WriteLine("Creating new config file");

                config.LoadDefaults();
                config.Save();
            }


            // test loading a file we now know exists for sure

            if (config.Load())
            {
                TrustedDevicePlugin plug = new TrustedDevicePlugin();
                plug.Id = Guid.Parse("4b7abed2-0184-43c5-a3ce-b5b05c67f81d"); // guid is defined in the plugin
                plug.SubFolder = "Loopback";
                plug.PluginClassName = "MidiLoopbackPlugin.LoopbackMidiPlugin";
                plug.FileName = "MidiLoopbackPlugin.dll";

                var existing = config.MidiConfig.TrustedDevicePlugins.Where<TrustedDevicePlugin>(x => x.Id == plug.Id);

                if (existing != null && existing.Count() == 0)
                {
                    config.MidiConfig.TrustedDevicePlugins.Add(plug);
                    config.Save();
                }
                else
                {
                    System.Diagnostics.Debug.WriteLine("Trusted plugin entry already exists");
                }
            }
            else
            {
                Assert.Fail("Could not load config file");

            }
        }



        // THis test creates a lot of files. Don't run it as part of normal testing
        //[TestMethod]
        public void TestConfigFileSaveAndRenaming100k()
        {
            System.Diagnostics.Debug.WriteLine("Testing save/rename 100,000 times");

            using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
            var logger = loggerFactory.CreateLogger("Unit Test");

            MidiServicesConfig config = new MidiServicesConfig(logger);

            // first run
            if (!config.Load())
            {
                // create new config
                System.Diagnostics.Debug.WriteLine("Creating new config file");

                config.LoadDefaults();
                config.Save();
            }

            // If the number of digits in the backup extension is increased, change this number
            // to exercise them. At the time of this writing, it is a 4 digit number.
            for (int i = 0; i < 100000; i++)
            {
                if (i % 100 == 0)
                {
                    System.Diagnostics.Debug.WriteLine($"Loop {i}");
                }

                config.Save();
            }


            // made it here with no exceptions? We're good.

        }



        [TestMethod]
        public void TestConfigFileSaveAndRenaming100()
        {
            System.Diagnostics.Debug.WriteLine("Testing save/rename 100 times");

            using var loggerFactory = LoggerFactory.Create(builder => builder.AddConsole());
            var logger = loggerFactory.CreateLogger("Unit Test");

            MidiServicesConfig config = new MidiServicesConfig(logger);

            // first run
            if (!config.Load())
            {
                // create new config
                System.Diagnostics.Debug.WriteLine("Creating new config file");

                config.LoadDefaults();
                config.Save();
            }

            for (int i = 0; i < 100; i++)
            {
                if (i % 10 == 0)
                {
                    System.Diagnostics.Debug.WriteLine($"Loop {i}");
                }

                config.Save();
            }

            // made it here with no exceptions? We're good.

        }
    }




}