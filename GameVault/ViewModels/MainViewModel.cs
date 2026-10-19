using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Media.Imaging;
using GameVault.Core;
using GameVault.Services;

namespace GameVault.ViewModels;

public sealed class GameCardVM : INotifyPropertyChanged
{
    public GameEntry Model { get; }
    public string Title => Model.Title;
    public string PlatformDisplay => Model.PlatformDisplay;
    public bool IsFavorite
    {
        get => Model.Favorite;
        set { Model.Favorite = value; On(); App.Library.Update(); }
    }

    BitmapImage? _cover;
    public BitmapImage? Cover
    {
        get => _cover;
        set { _cover = value; On(); }
    }

    public GameCardVM(GameEntry g)
    {
        Model = g;
        Cover = CoverCacheService.LoadCached(g.CachedHeader ?? g.CachedLogo);
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    void On([CallerMemberName] string? n = null) => PropertyChanged?.Invoke(this, new(n));
}

public sealed class MainViewModel : INotifyPropertyChanged
{
    public ObservableCollection<GameCardVM> Games { get; } = new();
    public ObservableCollection<TorrentItemStatus> Downloads { get; } = new();

    GameCardVM? _selected;
    public GameCardVM? Selected { get => _selected; set { _selected = value; On(); On(nameof(HasSelection)); } }

    public bool HasSelection => _selected != null;

    string _searchText = "";
    public string SearchText { get => _searchText; set { _searchText = value; On(); Rebuild(); } }

    string _status = "Готово";
    public string Status { get => _status; set { _status = value; On(); } }

    readonly LaunchService _launch = new(App.Library);

    public MainViewModel()
    {
        App.Library.Changed += () => Application.Current?.Dispatcher.Invoke(Rebuild);
        if (App.Torrents != null)
            App.Torrents.SnapshotChanged += () => Application.Current?.Dispatcher.Invoke(SyncDownloads);
        Rebuild();
    }

    public void Rebuild()
    {
        var q = SearchText.Trim().ToLowerInvariant();
        Games.Clear();
        foreach (var g in App.Library.Items
                     .Where(g => q.Length == 0 || g.Title.ToLowerInvariant().Contains(q))
                     .OrderByDescending(g => g.Favorite).ThenBy(g => g.Title))
            Games.Add(new GameCardVM(g));
    }

    void SyncDownloads()
    {
        if (App.Torrents == null) return;
        var live = App.Torrents.Active;
        foreach (var d in live.Where(x => !Downloads.Contains(x))) Downloads.Add(d);
        foreach (var d in Downloads.Where(x => live.All(y => y.InfoHashHex != x.InfoHashHex)).ToList())
            Downloads.Remove(d);
    }

    // ---------- КОМАНДЫ (вызываются из XAML через code-behind — без сторонних MVVM-библиотек) ----------

    public async System.Threading.Tasks.Task ScanPlatformsAsync()
    {
        Status = "Сканирую Steam/Epic/GOG/Amazon…";
        await System.Threading.Tasks.Task.Run(() =>
        {
            foreach (var s in PlatformScanner.ScanSteam())
            {
                if (App.Library.Find(g => g.Platform == GamePlatform.Steam && g.AppId == s.AppId.ToString()) != null) continue;
                var e = new GameEntry { Title = s.Name, Platform = GamePlatform.Steam, AppId = s.AppId.ToString(), InstallFolder = s.InstallDir };
                App.Library.Add(e);
                _ = PlatformScanner.DownloadSteamCoversAsync(e).ContinueWith(
                    _ => Application.Current?.Dispatcher.Invoke(Rebuild));
            }

            foreach (var ep in PlatformScanner.ScanEpic())
            {
                if (App.Library.Find(g => g.Platform == GamePlatform.Epic && g.EpicAppName == ep.AppName) != null) continue;
                var e = new GameEntry
                {
                    Title = ep.DisplayName, Platform = GamePlatform.Epic,
                    EpicAppName = ep.AppName, EpicCatalogItemId = string.IsNullOrEmpty(ep.CatalogItemId) ? null : ep.CatalogItemId,
                    InstallFolder = ep.InstallLocation
                };
                App.Library.Add(e);
                _ = PlatformScanner.DownloadEpicCoversAsync(e).ContinueWith(
                    _ => Application.Current?.Dispatcher.Invoke(Rebuild));
            }

            foreach (var gg in PlatformScanner.ScanGog())
            {
                if (App.Library.Find(g => g.Platform == GamePlatform.Gog && g.AppId == gg.Id) != null) continue;
                App.Library.Add(new GameEntry { Title = gg.Title, Platform = GamePlatform.Gog, AppId = gg.Id });
            }

            foreach (var am in PlatformScanner.ScanAmazonGames())
            {
                if (App.Library.Find(g => g.Platform == GamePlatform.AmazonGames && g.AppId == am.AppName) != null) continue;
                App.Library.Add(new GameEntry { Title = am.Title, Platform = GamePlatform.AmazonGames, AppId = am.AppName });
            }
        });
        Rebuild();
        Status = $"Сканирование завершено: {App.Library.Items.Count} игр в библиотеке";
    }

    public async System.Threading.Tasks.Task LaunchAsync(GameEntry? g)
    {
        if (g == null) return;
        var (ok, err) = await _launch.LaunchAsync(g);
        Status = ok ? $"Запуск: {g.Title}" : $"Ошибка запуска: {err}";
        Rebuild();
    }

    public void AddLocalGame()
    {
        var dlg = new Microsoft.Win32.OpenFileDialog
        {
            Title = "Выберите exe игры или index.html",
            Filter = "Игры|*.exe;*.html|Все файлы|*.*"
        };
        if (dlg.ShowDialog() != true) return;
        var path = dlg.FileName;
        var isHtml = path.EndsWith(".html", StringComparison.OrdinalIgnoreCase);
        var e = new GameEntry
        {
            Title = Path.GetFileNameWithoutExtension(path),
            Platform = isHtml ? GamePlatform.Html : GamePlatform.LocalExe,
            HtmlPath = isHtml ? path : null,
            ExePath = isHtml ? null : path,
            WorkingDirectory = Path.GetDirectoryName(path)
        };
        App.Library.Add(e);
        Rebuild();
        Status = "Добавлено: " + e.Title;
    }

    public void AddHtmlFolder()
    {
        var dlg = new Microsoft.Win32.OpenFolderDialog { Title = "Папка с HTML-игрой (index.html внутри)" };
        if (dlg.ShowDialog() != true) return;
        var idx = Directory.GetFiles(dlg.FolderName, "*.html", SearchOption.AllDirectories).FirstOrDefault();
        if (idx == null) { Status = "В папке не найден HTML"; return; }
        var e = new GameEntry
        {
            Title = Path.GetFileName(dlg.FolderName),
            Platform = GamePlatform.Html,
            HtmlPath = idx,
            InstallFolder = dlg.FolderName
        };
        App.Library.Add(e);
        Rebuild();
    }

    public async System.Threading.Tasks.Task DownloadTorrentAsync(string magnet, string titleHint)
    {
        if (App.Torrents == null) return;
        var st = await App.Torrents.AddMagnetAsync(magnet, titleHint);
        if (st != null)
        {
            Status = "Торрент добавлен в загрузку: " + st.Title;
            SyncDownloads();
        }
        else Status = "Не удалось разобрать магнет-ссылку";
    }

    public async System.Threading.Tasks.Task RemoveGameAsync(GameEntry? g)
    {
        if (g == null) return;
        App.Library.Remove(g.Id);
        try
        {
            foreach (var f in new[] { g.CachedHeader, g.CachedLogo })
                if (f != null && File.Exists(f)) File.Delete(f);
        }
        catch { }
        Rebuild();
    }

    public async System.Threading.Tasks.Task PauseDownloadAsync(TorrentItemStatus? d)
    {
        if (d != null && App.Torrents != null) await App.Torrents.PauseAsync(d.InfoHashHex);
    }
    public async System.Threading.Tasks.Task ResumeDownloadAsync(TorrentItemStatus? d)
    {
        if (d != null && App.Torrents != null) await App.Torrents.ResumeAsync(d.InfoHashHex);
    }
    public async System.Threading.Tasks.Task CancelDownloadAsync(TorrentItemStatus? d)
    {
        if (d != null && App.Torrents != null)
        {
            await App.Torrents.RemoveAsync(d.InfoHashHex);
            Downloads.Remove(d);
        }
    }

    /// <summary>После завершения загрузки торрента — регистрируем игру в библиотеке.</summary>
    public async System.Threading.Tasks.Task ImportDownloadedGameAsync(TorrentItemStatus d)
    {
        if (App.Torrents == null) return;
        var root = App.Torrents.LocateExtractedGameRoot(d.Title);
        var html = Directory.EnumerateFiles(root, "*.html", SearchOption.AllDirectories)
                            .OrderBy(f => f.Length).FirstOrDefault();
        var exe = Directory.EnumerateFiles(root, "*.exe", SearchOption.TopDirectoryOnly)
                           .FirstOrDefault(f => !Path.GetFileName(f).ToLowerInvariant().Contains("unins"));

        GameEntry e;
        if (html != null && (exe == null || new FileInfo(html).LastWriteTime > new FileInfo(exe).LastWriteTime))
            e = new GameEntry { Title = d.Title, Platform = GamePlatform.Html, HtmlPath = html, InstallFolder = root, MagnetLink = "magnet:?xt=urn:btih:" + d.InfoHashHex };
        else if (exe != null)
            e = new GameEntry { Title = d.Title, Platform = GamePlatform.LocalExe, ExePath = exe, WorkingDirectory = root, InstallFolder = root, MagnetLink = "magnet:?xt=urn:btih:" + d.InfoHashHex };
        else
        {
            e = new GameEntry { Title = d.Title, Platform = GamePlatform.LocalExe, ExePath = root, InstallFolder = root };
            e.ExePath = null; e.Platform = GamePlatform.Html; e.HtmlPath = root;
        }
        App.Library.Add(e);
        await CoverCacheService.DownloadToCacheAsync(e);
        Rebuild();
        Status = "Игра добавлена в библиотеку: " + e.Title;
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    void On([CallerMemberName] string? n = null) => PropertyChanged?.Invoke(this, new(n));
}
