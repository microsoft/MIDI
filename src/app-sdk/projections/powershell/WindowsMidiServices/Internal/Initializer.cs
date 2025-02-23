using Microsoft.Windows.Devices.Midi2.Initialization;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace WindowsMidiServices
{
    internal class Initializer
    {
        private static MidiDesktopAppSdkInitializer? m_initializer = null;

        internal bool IsInitialized => (bool)(m_initializer != null);

        // todo: this could throw exceptions to pwsh. That may be better
        internal static bool CreateInitializer()
        {
            if (m_initializer != null)
            {
                // already created
                return false;
            }

            m_initializer = MidiDesktopAppSdkInitializer.Create();

            if (m_initializer == null)
            {
                return false;
            }

            return true;
        }

        internal static bool Initialize()
        {
            if (m_initializer == null)
            {
                return false;
            }

            if (!m_initializer.InitializeSdkRuntime())
            {
                return false;
            }

            if (!m_initializer.EnsureServiceAvailable())
            {
                return false;
            }

            return true;
        }

        internal static bool Shutdown()
        {
            if (m_initializer == null)
            {
                return false;
            }

            m_initializer.ShutdownSdkRuntime();

            m_initializer.Dispose();

            m_initializer = null;

            return true;
        }

    }
}
