using System.Collections.ObjectModel;
using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

using Microsoft.Windows.Devices.Midi2;
using Microsoft.Windows.Devices.Midi2.Initialization;
using Microsoft.Windows.Devices.Midi2.Messages;

namespace MessageLight
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        MidiEndpointDeviceWatcher? _watcher;
        MidiSession? _session;
        MidiEndpointConnection? _connection;
        MidiDesktopAppSdkInitializer? _initializer;

        private List<Ellipse> _indicators { get; } = [];
        private const int MaxIndicators = 20;


        public ObservableCollection<MidiEndpointDeviceInformation> Endpoints { get; } = [];

        public MainWindow()
        {
            DataContext = this;
            InitializeComponent();

            Loaded += MainWindow_Loaded;
            ConnectionStatus.Text = "Disconnected";
        }

        private void MainWindow_Loaded(object sender, RoutedEventArgs e)
        {
            _initializer = MidiDesktopAppSdkInitializer.Create();

            if (_initializer != null)
            {
                _initializer.InitializeSdkRuntime();

                _session = MidiSession.Create("Message Light");

                _watcher = MidiEndpointDeviceWatcher.Create();

                if (_watcher != null)
                {

                    _watcher.Added += _watcher_Added;
                    _watcher.Removed += _watcher_Removed;
                    _watcher.Updated += _watcher_Updated;

                    _watcher.Start();
                }
            }
        }


        private void AvailableEndpoints_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (_session == null)
            {
                return;
            }

            // close connection if open
            if (_connection != null)
            {
                _session.DisconnectEndpointConnection(_connection.ConnectionId);
                _connection.MessageReceived -= _connection_MessageReceived;
                _connection = null;
                ConnectionStatus.Text = "Disconnected";
            }

            _connection = _session.CreateEndpointConnection(((MidiEndpointDeviceInformation)AvailableEndpoints.SelectedValue).EndpointDeviceId);

            if (_connection != null)
            {
                ConnectionStatus.Text = "Connecting...";

                _connection.MessageReceived += _connection_MessageReceived;


                _connection.EndpointDeviceDisconnected += _connection_EndpointDeviceDisconnected;
                _connection.EndpointDeviceReconnected += _connection_EndpointDeviceReconnected;

                _connection.Open();

                if (_connection.IsOpen)
                {
                    ConnectionStatus.Text = "Connected";

                    // TODO: Change to a style
                    //ConnectionStatus.Foreground = new SolidColorBrush("#FF00DD00");
                }
                else
                {
                    ConnectionStatus.Text = "Unable to connect";
                }
            }
        }

        private void _connection_EndpointDeviceReconnected(IMidiEndpointConnectionSource sender, object args)
        {
            Dispatcher.BeginInvoke(() =>
            {
                ConnectionStatus.Text = "Reconnected";
            });
        }

        private void _connection_EndpointDeviceDisconnected(IMidiEndpointConnectionSource sender, object args)
        {
            Dispatcher.BeginInvoke(() =>
            {
                ConnectionStatus.Text = "Disconnected";
            });
        }


        // consider changing this so it has a round-robin of a dozen or so circles at random positions

        private void _connection_MessageReceived(IMidiMessageReceivedEventSource sender, MidiMessageReceivedEventArgs args)
        {
            // only light up for channel voice messages
            if (args.MessageType != MidiMessageType.Midi1ChannelVoice32 && args.MessageType != MidiMessageType.Midi2ChannelVoice64)
            {
                return; 
            }

            Dispatcher.BeginInvoke(() =>
            {
                OnMidiMessageReceived();
            });
        }

        private void _watcher_Updated(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationUpdatedEventArgs args)
        {
            if (!args.IsNameUpdated) return;

            Dispatcher.BeginInvoke(() =>
            {
                var newEp = MidiEndpointDeviceInformation.CreateFromEndpointDeviceId(args.EndpointDeviceId);

                if (newEp != null)
                {
                    foreach (var ep in Endpoints)
                    {
                        if (ep.EndpointDeviceId == args.EndpointDeviceId)
                        {
                            Endpoints.Remove(ep);
                            Endpoints.Add(newEp);
                            return;
                        }
                    }
                }

            });
        }

        private void _watcher_Removed(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationRemovedEventArgs args)
        {
            Dispatcher.BeginInvoke(() =>
            {
                foreach (var ep in Endpoints)
                {
                    if (ep.EndpointDeviceId == args.EndpointDeviceId)
                    {
                        Endpoints.Remove(ep);
                        return;
                    }
                }
            });

        }

        private void _watcher_Added(MidiEndpointDeviceWatcher sender, MidiEndpointDeviceInformationAddedEventArgs args)
        {
            Dispatcher.BeginInvoke(() =>
            {
                Endpoints.Add(args.AddedDevice);
            });
        }

        private void OnMidiMessageReceived()
        {
            double h = 300;

            Random r = new Random();
            h = r.NextDouble() * 360;

            double s = 100;
            double l = 90;

            // this code is neither robust nor thread-safe. It's quick demo code.
            // would also be cleaner, but not faster, to do this with an itemscontrol
            // and binding.

            if (_indicators.Count >= MaxIndicators)
            {
                _indicators.RemoveAt(0);
            }

            double circleSize = r.NextDouble() * (Math.Min(ContentPanel.ActualWidth, ContentPanel.ActualHeight) / 3.0) + 100;

            var thisIndicator = new Ellipse
            {
                Fill = new SolidColorBrush(HslToRgb(h, s, l)),
                Width = circleSize,
                Height = circleSize,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
                RenderTransformOrigin = new Point(0.5, 0.5),
                Opacity = 1.0
            };

            double x = (ContentPanel.ActualWidth - circleSize) * r.NextDouble();
            double y = (ContentPanel.ActualHeight - circleSize) * r.NextDouble();

            Canvas.SetLeft(thisIndicator, x);
            Canvas.SetTop(thisIndicator, y);

            var scaleTransform = new ScaleTransform
            {
                ScaleX = 0.3,
                ScaleY = 0.3
            };
            thisIndicator.RenderTransform = scaleTransform;


            var storyboard = new Storyboard();

            var opacityAnimation = new DoubleAnimation
            {
                BeginTime = TimeSpan.FromSeconds(0.2),
                Duration = new Duration(TimeSpan.FromSeconds(1)),
                From = 1.0,
                To = 0.0,
                EasingFunction = new ExponentialEase()
            };
            storyboard.Children.Add(opacityAnimation);

            var scaleXAnimation = new DoubleAnimation
            {
                BeginTime = TimeSpan.FromSeconds(0),
                Duration = new Duration(TimeSpan.FromSeconds(0.8)),
                From = 1.0,
                To = 0.3,
                EasingFunction = new ExponentialEase()
            };
            storyboard.Children.Add(scaleXAnimation);

            var scaleYAnimation = new DoubleAnimation
            {
                BeginTime = TimeSpan.FromSeconds(0),
                Duration = new Duration(TimeSpan.FromSeconds(0.8)),
                From = 1.0,
                To = 0.3,
                EasingFunction = new ExponentialEase()
            };
            storyboard.Children.Add(scaleYAnimation);

            Storyboard.SetTarget(opacityAnimation, thisIndicator);
            Storyboard.SetTargetProperty(opacityAnimation, new PropertyPath(Ellipse.OpacityProperty));

            Storyboard.SetTarget(scaleXAnimation, thisIndicator);
            Storyboard.SetTargetProperty(scaleXAnimation, new PropertyPath("RenderTransform.ScaleX"));

            Storyboard.SetTarget(scaleYAnimation, thisIndicator);
            Storyboard.SetTargetProperty(scaleYAnimation, new PropertyPath("RenderTransform.ScaleY"));

            ContentPanel.Children.Add(thisIndicator);
            _indicators.Add(thisIndicator);

            storyboard.Begin();

        }

        // from copilot
        public static Color HslToRgb(double h, double s, double l, byte alpha = 255)
        {
            double r, g, b;

            if (s == 0)
            {
                r = g = b = l; // Achromatic
            }
            else
            {
                double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
                double p = 2 * l - q;
                r = HueToRgb(p, q, h / 360 + 1.0 / 3.0);
                g = HueToRgb(p, q, h / 360);
                b = HueToRgb(p, q, h / 360 - 1.0 / 3.0);
            }

            return Color.FromArgb(alpha,
                (byte)Math.Round(r * 255),
                (byte)Math.Round(g * 255),
                (byte)Math.Round(b * 255));
        }

        // from copilot
        private static double HueToRgb(double p, double q, double t)
        {
            if (t < 0) t += 1;
            if (t > 1) t -= 1;
            if (t < 1.0 / 6.0) return p + (q - p) * 6 * t;
            if (t < 1.0 / 2.0) return q;
            if (t < 2.0 / 3.0) return p + (q - p) * (2.0 / 3.0 - t) * 6;
            return p;
        }

    }
}