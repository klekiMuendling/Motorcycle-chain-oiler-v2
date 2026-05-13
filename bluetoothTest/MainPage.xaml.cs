using Microsoft.Maui.ApplicationModel;
using Microsoft.Maui.Devices;
using Plugin.BLE;
using Plugin.BLE.Abstractions.Contracts;
using System.Text;





namespace bluetoothTest
{

#if ANDROID
using Android;
using Microsoft.Maui.ApplicationModel;
    using Xamarin.KotlinX.Coroutines.Selects;

public class BluetoothScanPermission : Permissions.BasePlatformPermission
{
    public override (string androidPermission, bool isRuntime)[] RequiredPermissions =>
        new[]
        {
            (Manifest.Permission.BluetoothScan, true)
        };
}

public class BluetoothConnectPermission : Permissions.BasePlatformPermission
{
    public override (string androidPermission, bool isRuntime)[] RequiredPermissions =>
        new[]
        {
            (Manifest.Permission.BluetoothConnect, true)
        };
}
#endif

    public partial class MainPage : ContentPage
    {
        public IAdapter adapter = CrossBluetoothLE.Current.Adapter;
        public IDevice? foundDevice = null;
        public IService? bleService = null;
        public ICharacteristic? rxCharacteristic = null;
        public ICharacteristic? txCharacteristic = null;
        public int PW = -1;      //PWM value
        public int TD = -1;      //time delay after start
        public int TP = -1;      //time pause between two runs
        public int TO = -1;      //time pump on
        public int TH = -1;      //time hand on
        
        public int count = 0;
        const double PING_TIMEOUT = 4.5;

        public DateTime lastPing = DateTime.MinValue;
        public bool deviceAlive = false;

        public string received = "";
        // UUIDs müssen zu deinem ESP32-Code passen
        public readonly Guid SERVICE_UUID = Guid.Parse("12345678-1234-1234-1234-1234567890ab");
        public readonly Guid RX_UUID = Guid.Parse("12345678-1234-1234-1234-1234567890ac");
        public readonly Guid TX_UUID = Guid.Parse("12345678-1234-1234-1234-1234567890ad");

        public MainPage()
        {
            InitializeComponent();
        }


        public async Task<bool> EnsureBluetoothPermissionsAsync()
        {
        #if ANDROID
        if (OperatingSystem.IsAndroidVersionAtLeast(31))
        {
            var scanStatus = await Permissions.RequestAsync<BluetoothScanPermission>();
            var connectStatus = await Permissions.RequestAsync<BluetoothConnectPermission>();

            return scanStatus == PermissionStatus.Granted &&
                   connectStatus == PermissionStatus.Granted;
        }
        else
        {
            var locationStatus = await Permissions.RequestAsync<Permissions.LocationWhenInUse>();
            return locationStatus == PermissionStatus.Granted;
        }
        #else
            return true;
        #endif
        }

        public async void ScanClicked(object sender, EventArgs e)
        {
            PW = -1; TD = -1; TP = -1; TO = -1; TH = -1; count = 0;
            bool granted = await EnsureBluetoothPermissionsAsync();
            if (!granted)
            {
                scanButton.Text =  "Bluetooth permission denied";
                return;
            }

            if (!CrossBluetoothLE.Current.IsOn)
            {
                scanButton.Text = "Bluetooth is OFF";
                return;
            }

            foundDevice = null;
            scanButton.Text = "Scanning...";

            adapter.DeviceDiscovered -= OnDeviceDiscovered;
            adapter.DeviceDiscovered += OnDeviceDiscovered;

            try
            {
                await adapter.StartScanningForDevicesAsync();
            }
            catch (Exception ex)
            {
                scanButton.Text = $"Scan error: {ex.Message}";
                adapter.DeviceDiscovered -= OnDeviceDiscovered;
                return;
            }

            await Task.Delay(10000);
            await adapter.StopScanningForDevicesAsync();
            adapter.DeviceDiscovered -= OnDeviceDiscovered;

            scanButton.Text = foundDevice != null
                ? $"Found device"
                : "Nothing found";
        }

        public void OnDeviceDiscovered(object? sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e)
        {
            string name = e.Device.Name ?? "(no name)";
            string id = e.Device.Id.ToString();

            MainThread.BeginInvokeOnMainThread(() =>
            {
                scanButton.Text = $"{name}";
            });

            Console.WriteLine($"FOUND: {name} | {id}");

            if (name.Contains("ESP32"))
            {
                foundDevice = e.Device;

                // stop scanning immediately
                _ = StopScanAsync();
            }
        }

        public async Task StopScanAsync()
        {
            try
            {
                if (adapter.IsScanning)
                    await adapter.StopScanningForDevicesAsync();
            }
            catch { }

            adapter.DeviceDiscovered -= OnDeviceDiscovered;

            MainThread.BeginInvokeOnMainThread(() =>
            {
                scanButton.Text = "Found device";
            });
        }


        /*
        private void OnDeviceDiscovered(object? sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e)
        {
            string name = e.Device.Name ?? "(no name)";
            string id = e.Device.Id.ToString();

            MainThread.BeginInvokeOnMainThread(() =>
            {
                scanButton.Text = $"{name}";
            });

            Console.WriteLine($"FOUND: {name} | {id}");

            if (name.Contains("ESP32"))
            {
                foundDevice = e.Device;
            }
        }
        */



        public async void ConnectClicked(object sender, EventArgs e)
        {
            if (foundDevice == null)
            {
                scanButton.Text = "No device found";
                return;
            }

            try
            {
                connectButton.Text =
                connectButton.Text = "Connecting...";
                await adapter.ConnectToDeviceAsync(foundDevice);

                bleService = await foundDevice.GetServiceAsync(SERVICE_UUID);
                rxCharacteristic = await bleService.GetCharacteristicAsync(RX_UUID);
                txCharacteristic = await bleService.GetCharacteristicAsync(TX_UUID);

                txCharacteristic.ValueUpdated += TxCharacteristic_ValueUpdated;
                await txCharacteristic.StartUpdatesAsync();
                
                connectButton.Text = "Connected";
                deviceAlive = true;
                lastPing = DateTime.Now;
                StartWatchdog();
            }
            catch (Exception ex)
            {
                connectButton.Text = $"Error: {ex.Message}";
            }
        }

        public void TxCharacteristic_ValueUpdated(object? sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e)
        {
            received = Encoding.UTF8.GetString(e.Characteristic.Value);

            MainThread.BeginInvokeOnMainThread(() =>
            {
                receivedLabel.Text = received;
                if (received.Substring(0, 3) == "PW:")
                {
                    PW = int.Parse(received.Substring(3));
                }
                else if (received.Substring(0, 3) == "TD:")
                {
                    TD = int.Parse(received.Substring(3));
                }
                else if (received.Substring(0, 3) == "TP:")
                {
                    TP = int.Parse(received.Substring(3));
                }
                else if (received.Substring(0, 3) == "TO:")
                {
                    TO = int.Parse(received.Substring(3));
                }
                else if (received.Substring(0, 3) == "TH:")
                {
                    TH = int.Parse(received.Substring(3));
                }
                else if (received.Substring(0, 4) == "Ping")
                {
                    deviceAlive = true;
                    lastPing = DateTime.Now;
                    LedBTalive.Color = Colors.LightGreen;
                    settingsButton.IsEnabled = true;
                    manualButton.IsEnabled = true;
                }
            });
        }

        public void StartWatchdog()
        {
            Device.StartTimer(TimeSpan.FromSeconds(1), () =>
            {
                var diff = DateTime.Now - lastPing;

                if (diff.TotalSeconds > PING_TIMEOUT) 
                {
                    if (deviceAlive)
                    {
                        deviceAlive = false;

                        MainThread.BeginInvokeOnMainThread(() =>
                        {
                            settingsButton.IsEnabled = false;
                            manualButton.IsEnabled = false;
                            LedBTalive.Color = Colors.Red;
                            connectButton.Text = "Connect";
                            scanButton.Text = "Scan BT";
                        });
                    }
                }

                return true; // keep running
            });
        }

        public async void SendClicked(object sender, EventArgs e)
        {
            if (rxCharacteristic == null)
            {
                sendButton.Text = "Not connected";
                await Task.Delay(3000);
                sendButton.Text = "Send";
                return;
            }

            try
            {
                string message = messageEntry.Text ?? "";
                byte[] data = Encoding.UTF8.GetBytes(message);

                await rxCharacteristic.WriteAsync(data);
                sendButton.Text = "Message sent";
                await Task.Delay(1500);
                sendButton.Text = "Send";
            }
            catch (Exception ex)
            {
                sendButton.Text = $"Send error: {ex.Message}";
                await Task.Delay(3000);
                sendButton.Text = "Send";
            }
        }
        public async void SendParam(string param, int value = -1)
        {
            if (rxCharacteristic == null)
            {
                sendButton.Text = "Not connected";
                await Task.Delay(3000);
                sendButton.Text = "Send";
                return;
            }

            try
            {
                string message;
                if (value != -1)
                {
                    message = (param + "=" + value.ToString()) ?? "";
                }
                else
                {
                    message = (param) + "=" ?? "";
                }
                byte[] data = Encoding.UTF8.GetBytes(message);

                await rxCharacteristic.WriteAsync(data);
                sendButton.Text = "Message sent";
                await Task.Delay(1500);
                sendButton.Text = "Send";
            }
            catch (Exception ex)
            {
                sendButton.Text = $"Send error: {ex.Message}";
                await Task.Delay(3000);
                sendButton.Text = "Send";
            }
        }

        public async void ReadParam(string param)
        {
            if (rxCharacteristic == null)
            {
                sendButton.Text = "Not connected";
                await Task.Delay(3000);
                sendButton.Text = "Send";
                return;
            }

            try
            {
                string message = (param) ?? "";
                byte[] data = Encoding.UTF8.GetBytes(message);

                await rxCharacteristic.WriteAsync(data);
                sendButton.Text = "Message sent";
                await Task.Delay(1500);
                sendButton.Text = "Send";
            }
            catch (Exception ex)
            {
                sendButton.Text = $"Send error: {ex.Message}";
                await Task.Delay(3000);
                sendButton.Text = "Send";
            }
        }
        public async void settingsButton_Clicked(object sender, EventArgs e)
        {
            // navigate to Settings page
            //await Navigation.PushAsync(new Settings());
            await Navigation.PushAsync(Application.Current.Handler.MauiContext.Services.GetService<Settings>());
        }

        public async void manualButton_Clicked(object sender, EventArgs e)
        {
            // navigate to Settings page
            await Navigation.PushAsync(Application.Current.Handler.MauiContext.Services.GetService<Manual>());
        }
    }
}

