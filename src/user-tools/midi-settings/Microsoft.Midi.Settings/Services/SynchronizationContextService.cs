using Microsoft.Midi.Settings.Contracts.Services;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Services
{
    public class SynchronizationContextService : ISynchronizationContextService
    {
        private SynchronizationContext _uiContext;
        public SynchronizationContext GetUIContext()
        {
            return _uiContext;
        }

        public SynchronizationContextService(SynchronizationContext uiContext)
        {
            _uiContext = uiContext;

            System.Diagnostics.Debug.WriteLine($"SynchronizationContextService: UI Context captured.");
        }


    }
}
