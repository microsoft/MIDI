using System;

namespace MidiSettings.Contracts.Services;

public interface IPageService
{
    Type GetPageType(string key);
}
