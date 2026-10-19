using System.Windows;

namespace GameVault;

public partial class App : Application
{
    public static Core.ConfigStore Config { get; } = new();
    public static Core.LibraryStore Library { get; } = new();
    public static Services.TorrentEngine? Torrents { get; set; }
    public static ViewModels.MainViewModel? MainVm { get; set; }

    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        Config.Load();
        Library.Load();
        Torrents = new Services.TorrentEngine(Config.Data);
    }

    protected override void OnExit(ExitEventArgs e)
    {
        try
        {
            Config.Save();
            Library.Save();
            Torrents?.DisposeAsync().AsTask().Wait(2000);
        }
        catch { }
        base.OnExit(e);
    }
}
