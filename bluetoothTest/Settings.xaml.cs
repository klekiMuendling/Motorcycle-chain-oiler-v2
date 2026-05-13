using System.Globalization;
using System.Text;
using System.Globalization;

namespace bluetoothTest;

public partial class Settings : ContentPage
{
    bool defaultSet = false;
	public Settings(MainPage service)
	{
		InitializeComponent();
        _service = service;
    }
    private readonly MainPage _service;

    

    public async void setPWMclicked(object sender, EventArgs e)
    {
        _service.SendParam("PW", int.Parse(newPWvalue.Text));
    }
    public async void setTDclicked(object sender, EventArgs e)
    {
        _service.SendParam("TD", int.Parse(newTDvalue.Text));
    }
    public async void setTPclicked(object sender, EventArgs e)
    {
        _service.SendParam("TP", int.Parse(newTPvalue.Text));
    }
    public async void setTOclicked(object sender, EventArgs e)
    {
        //_service.SendParam("TO", int.Parse(newTOvalue.Text));
        double seconds = double.Parse(newTOvalue.Text, CultureInfo.CurrentCulture);
        int value = (int)Math.Round(seconds * 10);
        _service.SendParam("TO", value);
    }
    public async void setTHclicked(object sender, EventArgs e)
    {
        //_service.SendParam("TH", int.Parse(newTHvalue.Text));
        double seconds = double.Parse(newTHvalue.Text, CultureInfo.CurrentCulture);
        int value = (int)Math.Round(seconds * 10);
        _service.SendParam("TH", value);
    }
    public async void helpBtn_Clicked(object sender, EventArgs e)
    {
        await Navigation.PushAsync(new help());
    }
    private CancellationTokenSource _cts;

    protected override void OnAppearing()
    {
        base.OnAppearing();

        _cts = new CancellationTokenSource();
        RunLoop(_cts.Token);
    }

    protected override void OnDisappearing()
    {
        _cts.Cancel();
        base.OnDisappearing();
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
                if (_service.PW != -1)

                if (_service.PW != -1) 
                {
                    pwValue.Text = _service.PW.ToString() + " %"; 
                }
                if (_service.TD != -1) 
                {
                    tdValue.Text = _service.TD.ToString() + " s"; 
                }
                if (_service.TP != -1) 
                {
                    tpValue.Text = _service.TP.ToString() + " s"; 
                }
                if (_service.TO != -1)
                {
                    //toValue.Text = _service.TO.ToString() + " s";
                    toValue.Text = (_service.TO / 10.0).ToString("0.0") + " s";
                }
                if (_service.TH != -1)
                {
                    //thValue.Text = _service.TH.ToString() + " s";
                    thValue.Text = (_service.TH / 10.0).ToString("0.0") + " s";
                }
                if (!_service.received.Contains("Ping"))
                    receivedLabel.Text = _service.received;
                if (!_service.deviceAlive)
                {
                    LedBTalive.Color = Colors.Red;
                }
                else
                {
                    LedBTalive.Color = Colors.LightGreen;
                }
                if ((!defaultSet)
                    && (_service.PW != -1)
                    && (_service.TD != -1)
                    && (_service.TP != -1)
                    && (_service.TO != -1)
                    && (_service.TH != -1))

                {
                    newPWvalue.Text = _service.PW.ToString();
                    newTDvalue.Text = _service.TD.ToString();
                    newTPvalue.Text = _service.TP.ToString();
                    newTOvalue.Text = _service.TO.ToString();
                    newTHvalue.Text = _service.TH.ToString();
                    defaultSet = true;
                }
                if (!_service.deviceAlive)
                {
                    defaultSet = false;
                }
                await Task.Delay(800, token);
            }
        }
        catch
        {
            // normal by leaving the page, ignore cancellation exceptions
        }
    }


}
