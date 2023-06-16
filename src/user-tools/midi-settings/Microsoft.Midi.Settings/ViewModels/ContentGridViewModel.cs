using System.Collections.ObjectModel;
using System.Windows.Input;

using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.Core.Contracts.Services;
using Microsoft.Midi.Settings.Core.Models;

namespace Microsoft.Midi.Settings.ViewModels;

public class ContentGridViewModel : ObservableRecipient, INavigationAware
{
    private readonly INavigationService _navigationService;
    private readonly ISampleDataService _sampleDataService;

    public ICommand ItemClickCommand
    {
        get;
    }

    public ObservableCollection<MidiMessageViewModel> Source { get; } = new ObservableCollection<MidiMessageViewModel>();

    public ContentGridViewModel(INavigationService navigationService, ISampleDataService sampleDataService)
    {
        _navigationService = navigationService;
        _sampleDataService = sampleDataService;

//        ItemClickCommand = new RelayCommand<SampleOrder>(OnItemClick);
    }

    public async void OnNavigatedTo(object parameter)
    {
        //Source.Clear();

        //// TODO: Replace with real data.
        //var data = await _sampleDataService.GetContentGridDataAsync();
        //foreach (var item in data)
        //{
        //    Source.Add(item);
        //}
    }

    public void OnNavigatedFrom()
    {
    }

    //private void OnItemClick(SampleOrder? clickedItem)
    //{
    //    if (clickedItem != null)
    //    {
    //        _navigationService.SetListDataItemForNextConnectedAnimation(clickedItem);
    //        _navigationService.NavigateTo(typeof(ContentGridDetailViewModel).FullName!, clickedItem.OrderID);
    //    }
    //}
}
