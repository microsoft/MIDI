using CommunityToolkit.Mvvm.ComponentModel;

using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;

namespace Microsoft.Midi.Settings.ViewModels;

public class ContentGridDetailViewModel : ObservableRecipient, INavigationAware
{
    private readonly ISampleDataService _sampleDataService;
    //private SampleOrder? _item;

    //public SampleOrder? Item
    //{
    //    get => _item;
    //    set => SetProperty(ref _item, value);
    //}

    public ContentGridDetailViewModel(ISampleDataService sampleDataService)
    {
        _sampleDataService = sampleDataService;
    }

    public void OnNavigatedTo(object parameter)
    {
        //if (parameter is long orderID)
        //{
        //    var data = await _sampleDataService.GetContentGridDataAsync();
        //    Item = data.First(i => i.OrderID == orderID);
        //}
    }

    public void OnNavigatedFrom()
    {
    }
}
