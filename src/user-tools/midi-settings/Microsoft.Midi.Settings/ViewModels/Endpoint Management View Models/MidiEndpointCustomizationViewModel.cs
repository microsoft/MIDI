using CommunityToolkit.Mvvm.ComponentModel;
using Microsoft.Midi.Settings.Contracts.Services;
using Microsoft.Midi.Settings.Services;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
using Microsoft.Windows.Devices.Midi2.Utilities.Metadata;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;

namespace Microsoft.Midi.Settings.ViewModels
{
    public partial class MidiEndpointMidi1PortCustomizationViewModel : ObservableRecipient
    {
        [ObservableProperty]
        private MidiGroup group;

        [ObservableProperty]
        private string currentDisplayName;

        [ObservableProperty]
        private string calculatedOldStyleName;

        [ObservableProperty]
        private string calculatedNewStyleName;

        [ObservableProperty]
        private string originalCustomName;

        [ObservableProperty]
        private string newCustomName;
    }

    public partial class MidiEndpointCustomizationViewModel : ObservableRecipient
    {
        [ObservableProperty]
        private MidiEndpointWrapper endpointWrapper;

        [ObservableProperty]
        private string customName;

        [ObservableProperty]
        private string customDescription;

        [ObservableProperty]
        private string imageFileName;

        [ObservableProperty]
        private string imageFullPath;

        [ObservableProperty]
        private bool hasImage;

        [ObservableProperty]
        private ImageSource? image;


        [ObservableProperty]
        private bool supportsMidiPolyphonicExpression;

        [ObservableProperty]
        private bool requiresNoteOffTranslation;

        [ObservableProperty]
        private ushort recommendedControlChangeAutomationIntervalMilliseconds;

        [ObservableProperty]
        private Midi1PortNamingApproach midi1PortNaming;

        [ObservableProperty]
        private ObservableCollection<Midi1PortNamingApproach> midi1PortNamingOptions = [];


        [ObservableProperty]
        private ObservableCollection<MidiEndpointMidi1PortCustomizationViewModel> midi1SourcePorts = [];

        [ObservableProperty]
        private ObservableCollection<MidiEndpointMidi1PortCustomizationViewModel> midi1DestinationPorts = [];


        public void UpdateImage()
        {
            if (!string.IsNullOrEmpty(ImageFullPath))
            {
                HasImage = File.Exists(ImageFullPath);
                ImageFileName = App.GetService<IMidiEndpointImageService>().GetImageAssetFileName(ImageFullPath);
            }
            else
            {
                // use the image in the wrapper

                HasImage = Microsoft.Windows.Devices.Midi2.Utilities.Metadata.MidiImageAssetHelper.EndpointHasValidCustomImageAsset(endpointWrapper.DeviceInformation);
                ImageFullPath = Microsoft.Windows.Devices.Midi2.Utilities.Metadata.MidiImageAssetHelper.GetImageFullPathForEndpoint(endpointWrapper.DeviceInformation);
            }


            if (HasImage)
            {
                ImageFileName = App.GetService<IMidiEndpointImageService>().GetImageAssetFileName(ImageFullPath);

                Image = App.GetService<IMidiEndpointImageService>().GetImageSource(ImageFullPath);
            }
            else
            {
                ImageFileName = string.Empty;
                Image = null;
            }


        }

        public MidiEndpointCustomizationViewModel(MidiEndpointWrapper endpointWrapper)
        {
            Midi1PortNaming = endpointWrapper.DeviceInformation.Midi1PortNamingApproach;
            EndpointWrapper = endpointWrapper;

            CustomName = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().Name;
            CustomDescription = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().Description;
            ImageFileName = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().ImageFileName;
            SupportsMidiPolyphonicExpression = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().SupportsMidiPolyphonicExpression;
            RequiresNoteOffTranslation = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().RequiresNoteOffTranslation;
            RecommendedControlChangeAutomationIntervalMilliseconds = endpointWrapper.DeviceInformation.GetUserSuppliedInfo().RecommendedControlChangeAutomationIntervalMilliseconds;

            midi1PortNamingOptions.Add(Midi1PortNamingApproach.Default);
            midi1PortNamingOptions.Add(Midi1PortNamingApproach.UseClassicCompatible);
            midi1PortNamingOptions.Add(Midi1PortNamingApproach.UseNewStyle);

            UpdateImage();

            // name table entries

            foreach (var nameTableEntry in EndpointWrapper.DeviceInformation.GetNameTable())
            {
                var vm = new MidiEndpointMidi1PortCustomizationViewModel();

                vm.NewCustomName = nameTableEntry.CustomName;
                vm.OriginalCustomName = nameTableEntry.CustomName;

                vm.CalculatedOldStyleName = nameTableEntry.LegacyCompatibleName;
                vm.CalculatedNewStyleName = nameTableEntry.NewStyleName;

                vm.Group = new MidiGroup(nameTableEntry.GroupIndex);

                vm.CurrentDisplayName = EndpointWrapper.DeviceInformation.FindAssociatedMidi1PortForGroupForThisEndpoint(vm.Group, nameTableEntry.Flow).PortName;

                if (nameTableEntry.Flow == Midi1PortFlow.MidiMessageSource)
                {
                    Midi1SourcePorts.Add(vm);
                }
                else
                {
                    // TODO: find current display name


                    Midi1DestinationPorts.Add(vm);
                }

            }
        }

        public MidiServiceEndpointCustomizationConfig GetUpdateConfig()
        {
            var configUpdate = new MidiServiceEndpointCustomizationConfig(
                EndpointWrapper.DeviceInformation.GetTransportSuppliedInfo().TransportId);

            configUpdate.Name = CustomName;
            configUpdate.Description = CustomDescription;
            configUpdate.ImageFileName = ImageFileName;
            configUpdate.RequiresNoteOffTranslation = RequiresNoteOffTranslation;
            configUpdate.SupportsMidiPolyphonicExpression = SupportsMidiPolyphonicExpression;
            configUpdate.RecommendedControlChangeIntervalMilliseconds = RecommendedControlChangeAutomationIntervalMilliseconds;

            configUpdate.MatchCriteria.EndpointDeviceId = EndpointWrapper.DeviceInformation.EndpointDeviceId;
            configUpdate.MatchCriteria.DeviceInstanceId = EndpointWrapper.DeviceInformation.DeviceInstanceId;
            //configUpdate.MatchCriteria.UsbVendorId = EndpointWrapper.DeviceInformation.GetTransportSuppliedInfo().VendorId;
            //configUpdate.MatchCriteria.UsbProductId = EndpointWrapper.DeviceInformation.GetTransportSuppliedInfo().ProductId;
            //configUpdate.MatchCriteria.UsbSerialNumber = EndpointWrapper.DeviceInformation.GetTransportSuppliedInfo().SerialNumber;

            configUpdate.Midi1PortNamingApproach = Midi1PortNaming;

            if (Midi1SourcePorts != null && Midi1SourcePorts.Count > 0)
            {
                foreach (var port in Midi1SourcePorts)
                {
                    if (!string.IsNullOrWhiteSpace(port.NewCustomName))
                    {
                        configUpdate.AddMidi1SourcePortCustomName(port.Group, port.NewCustomName.Trim());
                    }
                    else if (port.OriginalCustomName != port.NewCustomName)
                    {
                        // it used to have a value, and is now blank, so we need to clear it out
                        configUpdate.AddMidi1SourcePortCustomName(port.Group, port.NewCustomName.Trim());
                    }
                }
            }

            if (Midi1DestinationPorts != null && Midi1DestinationPorts.Count > 0)
            {
                foreach (var port in Midi1DestinationPorts)
                {
                    if (!string.IsNullOrWhiteSpace(port.NewCustomName))
                    {
                        configUpdate.AddMidi1DestinationPortCustomName(port.Group, port.NewCustomName.Trim());
                    }
                    else if (port.OriginalCustomName != port.NewCustomName)
                    {
                        // it used to have a value, and is now blank, so we need to clear it out
                        configUpdate.AddMidi1DestinationPortCustomName(port.Group, port.NewCustomName.Trim());
                    }
                }
            }

            return configUpdate;

        }


    }
}
