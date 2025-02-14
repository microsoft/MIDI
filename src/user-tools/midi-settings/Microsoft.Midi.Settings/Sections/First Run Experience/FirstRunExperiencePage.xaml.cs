using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Microsoft.Midi.Settings.Contracts.ViewModels;
using Microsoft.Midi.Settings.ViewModels;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace Microsoft.Midi.Settings.Views
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class FirstRunExperiencePage : Page
    {
        public FirstRunExperienceViewModel ViewModel
        {
            get;
        }


        public FirstRunExperiencePage()
        {
            ViewModel = App.GetService<FirstRunExperienceViewModel>();

            Loaded += FirstRunExperiencePage_Loaded;

            this.InitializeComponent();
        }

        private void FirstRunExperiencePage_Loaded(object sender, RoutedEventArgs e)
        {
            ShowFirstStep();
        }

        private async void ShowFirstStep()
        {
            var result = await Dialog_CreateConfigFile.ShowAsync();


        }





    }
}
