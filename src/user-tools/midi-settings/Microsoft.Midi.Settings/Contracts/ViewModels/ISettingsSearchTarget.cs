using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Midi.Settings.Contracts.ViewModels;

public interface ISettingsSearchTarget
{
    abstract static IList<string> GetSearchKeywords();

    abstract static string GetSearchPageTitle();

    abstract static string GetSearchPageDescription();
}
