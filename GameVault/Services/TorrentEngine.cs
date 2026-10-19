using System.IO;
using MonoTorrent;
using MonoTorrent.Client;

namespace GameVault.Services;

public sealed class TorrentItemStatus : System.ComponentModel.INotifyPropertyChanged
{
    public string Title { get; set; } = "";
    double _progress; long _downloaded, _total; double _down, _up; int _peers; string _state = "Очередь";
    public double Progress { get => _progress; set { _progress = value; On(); On(nameof(ProgressText)); } }
    public long DownloadedBytes { get => _downloaded; set { _downloaded = value; On(); } }
    public long TotalBytes { get => _total; set { _total = value; On(); } }
    public double DownSpeedBps { get => _down; set { _down = value; On(); On(nameof(ProgressText)); } }
    public double UpSpeedBps { get => _up; set { _up = value; On(); } }
    public int Peers { get => _peers; set { _peers = value; On(); On(nameof(ProgressText)); } }
    public string State { get => _state; set { _state = value; On(); } }
    public string? SavePath { get; set; }
    public string InfoHashHex { get; set; } = "";
    public string ProgressText => $"{Progress:P0} · {Fmt(DownSpeedBps)}/s ↓ · пиры: {Peers}";
    void On([System.Runtime.CompilerServices.CallerMemberName] string? n = null)
        => PropertyChanged?.Invoke(this, new(n));
    public event System.ComponentModel.PropertyChangedEventHandler? PropertyChanged;
    static string Fmt(double bps) => bps > 1048576 ? $"{bps / 1048576:F1}М" : $"{bps / 1024:F0}К";
}

/// <summary>Встроенный торрент-движок на MonoTorrent 3.x: магнет-ссылки, DHT/PEX, лимиты скорости.</summary>
public sealed class TorrentEngine : IAsyncDisposable
{
    readonly ClientEngine _engine;
    readonly Dictionary<TorrentInfoHash, TorrentItemStatus> _map = new();
    readonly string _downloadDir;
    bool _stop;

    public event Action? SnapshotChanged;

    public TorrentEngine(Core.VaultConfig cfg)
    {
        _downloadDir = cfg.TorrentDownloadFolder;
        Directory.CreateDirectory(_downloadDir);

        var builder = new EngineSettingsBuilder(ClientEngine.DefaultSettings)
        {
            AllowPortForwarding = true,
            AutoSaveLoadDhtCache = true,
            DiskCacheBytes = 64 * 1024 * 1024,
            DefaultSavePath = _downloadDir,
            MaximumOpenFiles = 512
        };
        if (cfg.MaxUpSpeedKBs > 0) builder.UploadLimit = cfg.MaxUpSpeedKBs * 1024;
        if (cfg.MaxDownSpeedKBs > 0) builder.DownloadLimit = cfg.MaxDownSpeedKBs * 1024;
        builder.MaximumConnections = cfg.MaxConnections;

        _engine = new ClientEngine(builder.ToSettings());
        _ = PollLoopAsync();
    }

    public async Task<TorrentItemStatus?> AddMagnetAsync(string magnet, string? titleOverride = null)
    {
        try
        {
            var link = MagnetLink.Parse(magnet);
            var mgr = await _engine.TransferManager.AddAsync(link, _downloadDir, autoStart: true);

            var status = new TorrentItemStatus
            {
                Title = titleOverride ?? SafeName(link),
                SavePath = _downloadDir,
                InfoHashHex = Hex(link.InfoHash),
                State = "Загрузка метаданных…"
            };
            _map[link.InfoHash] = status;
            SnapshotChanged?.Invoke();
            return status;
        }
        catch { return null; }
    }

    static string SafeName(MagnetLink l)
    {
        try { return !string.IsNullOrEmpty(l.DisplayName) ? l.DisplayName! : Hex(l.InfoHash)[..12]; }
        catch { return "torrent"; }
    }

    static string Hex(TorrentInfoHash h)
    {
        try { return h.ToString() ?? "unknown"; } catch { return "unknown"; }
    }

    async Task PollLoopAsync()
    {
        while (!_stop)
        {
            try
            {
                foreach (var (hash, st) in _map.ToArray())
                {
                    var mgr = FindMgr(hash);
                    if (mgr == null) continue;

                    var s = await mgr.Monitoring.UpdateAsync();
                    st.Progress = s.Progress;
                    st.DownloadedBytes = s.Monitor.DataReceived;
                    if (s.Progress > 0.001)
                        st.TotalBytes = Math.Max(st.TotalBytes, (long)(s.Monitor.DataReceived / s.Progress));
                    st.DownSpeedBps = s.Monitor.RateDown;
                    st.UpSpeedBps = s.Monitor.RateUp;
                    st.Peers = s.Peers.PeerCount;
                    st.State = s.State switch
                    {
                        Managers.TorrentState.Downloading => "Загрузка",
                        Managers.TorrentState.Seeding => "Раздача (seed)",
                        Managers.TorrentState.Hashing => "Проверка файлов",
                        Managers.TorrentState.Paused => "Пауза",
                        Managers.TorrentState.Stopped when s.Progress >= 0.999 => "Готово",
                        Managers.TorrentState.Stopped => "Остановлен",
                        _ => st.State
                    };
                }
                SnapshotChanged?.Invoke();
            }
            catch { }
            await Task.Delay(1000);
        }
    }

    TorrentManager? FindMgr(TorrentInfoHash hash)
    {
        try
        {
            foreach (var m in _engine.TransferManager.TrackerManagers())
                if (m.InfoHash.Equals(hash)) return m;
        }
        catch { }
        return null;
    }

    public async Task PauseAsync(string hex)
    {
        var m = FindByHex(hex); if (m != null) await m.StopAsync();
    }
    public async Task ResumeAsync(string hex)
    {
        var m = FindByHex(hex); if (m != null) await m.StartAsync();
    }
    public async Task RemoveAsync(string hex)
    {
        var m = FindByHex(hex);
        if (m != null)
        {
            try { await _engine.TransferManager.RemoveAsync(m.InfoHash, removeData: false); } catch { }
            _map.Remove(m.InfoHash);
        }
        SnapshotChanged?.Invoke();
    }

    TorrentManager? FindByHex(string hex)
    {
        try
        {
            foreach (var m in _engine.TransferManager.TrackerManagers())
                if (string.Equals(m.InfoHash.ToString(), hex, StringComparison.OrdinalIgnoreCase)) return m;
        }
        catch { }
        return null;
    }

    /// <summary>Находит скачанный корень игры в папке загрузок (по подсказке имени или самый свежий).</summary>
    public string LocateExtractedGameRoot(string titleHint)
    {
        try
        {
            var hint = new string(titleHint.Where(char.IsLetterOrDigit).ToArray()).ToLowerInvariant();
            foreach (var dir in Directory.GetDirectories(_downloadDir))
            {
                var dname = new string(Path.GetFileName(dir).Where(char.IsLetterOrDigit).ToArray()).ToLowerInvariant();
                if (dname.Length > 3 && hint.Length > 3 &&
                    (dname.Contains(hint[..Math.Min(hint.Length, 12)]) || hint.Contains(dname)))
                    return dir;
            }
            return Directory.GetDirectories(_downloadDir)
                .OrderByDescending(Directory.GetCreationTimeUtc).FirstOrDefault() ?? _downloadDir;
        }
        catch { return _downloadDir; }
    }

    public async ValueTask DisposeAsync()
    {
        _stop = true;
        try { await _engine.StopAllAsync(); } catch { }
        _engine.Dispose();
    }
}
