#if ANDROID
using Android.App;
#endif
using System.Text;
namespace bluetoothTest;

public partial class Manual : ContentPage
{
    const int MAX_MANUAL = 10;
    public Manual(MainPage service)
	{
		InitializeComponent();
        _service = service;
        V12button.IsVisible = false;
        V7Button.IsVisible = false;
        checkVbutton.IsEnabled = false;
        
    }

    private readonly MainPage _service;

    private void manualButton_Clicked(object sender, EventArgs e)
    {
        _service.SendParam("MN", _service.TH);
    }

    private void jogButton_Pressed(object sender, EventArgs e)
    {
        _service.SendParam("MN", MAX_MANUAL * 10);
    }

    private void jogButton_Released(object sender, EventArgs e)
    {
        _service.SendParam("MN", 0);
    }

    private CancellationTokenSource _cts;

    protected override void OnAppearing()
    {
        base.OnAppearing();

        _cts = new CancellationTokenSource();
        RunLoop(_cts.Token);
    }

    private async void RunLoop(CancellationToken token)
    {

        try
        {
            while (!token.IsCancellationRequested)
            {
                if (_service.PW == -1)
                    _service.ReadParam("PW");
                else if (_service.TD == -1)
                    _service.ReadParam("TD");
                else if (_service.TP == -1)
                    _service.ReadParam("TP");
                else if (_service.TO == -1)
                    _service.ReadParam("TO");
                else if (_service.TH == -1)
                    _service.ReadParam("TH");
                
                    
                if (_service.TH != -1)
                {
                        manualDbutton.IsEnabled = true;
                        checkVbutton.IsEnabled = true;
                    //manualDbutton.Text = "manual " + _service.TH.ToString();
                    manualDbutton.Text = "manual " + (_service.TH / 10.0).ToString("0.0") + " s";   
                    

                }
                if (!_service.received.Contains("Ping"))
                    receivedLabel.Text = _service.received;
                if (!_service.deviceAlive)
                {
                    LedBTalive.Color = Colors.Red;
                    manualDbutton.IsEnabled = false;
                    jogButton.IsEnabled = false;
                }
                else
                {
                    LedBTalive.Color = Colors.LightGreen;
                    jogButton.IsEnabled = true;
                }

                await Task.Delay(800, token);
            }
        }
        catch
        {
            // normal by leaving the page, ignore cancellation exceptions
        }
    }

    private void V12button_Clicked(object sender, EventArgs e)
    {
        _service.SendParam("V12");
    }

    private void V7Button_Clicked(object sender, EventArgs e)
    {
        _service.SendParam("V07");
    }

    private void checkVbutton_Clicked(object sender, EventArgs e)
    {
        V7Button.IsVisible = true;
        V12button.IsVisible = true;
        _service.SendParam("VOO");
    }
}