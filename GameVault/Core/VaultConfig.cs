using System.IO;
using Newtonsoft.Json;

namespace GameVault.Core;

public sealed class VaultConfig
{
    public string TorrentDownloadFolder { get; set; } =
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Downloads", "GameVault");
    public string GameInstallFolder { get; set; } =
        Path.Combine(RootDrive(), "Games", "GameVault");
    public List<string> TorznabIndexers { get; set; } = new();
    public int MaxUpSpeedKBs { get; set; } = 512;
    public int MaxDownSpeedKBs { get; set; } = 0;      // 0 = без лимита
    public int MaxConnections { get; set; } = 80;
    public bool StartMinimized { get; set; }
    public string HtmlRuntime { get; set; } = "webview2"; // webview2 | browser (системный)

    static string RootDrive()
    {
        var d = Path.GetPathRoot(Environment.GetFolderPath(Environment.SpecialFolder.System));
        return string.IsNullOrEmpty(d) ? @"C:\" : d.TrimEnd('\\');
    }
}

public sealed class ConfigStore
{
    static readonly string Dir = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GameVault");
    static readonly string FilePath = Path.Combine(Dir, "config.json");

    public VaultConfig Data { get; private set; } = new();

    public void Load()
    {
        try
        {
            if (File.Exists(FilePath))
                Data = JsonConvert.DeserializeObject<VaultConfig>(File.ReadAllText(FilePath)) ?? new VaultConfig();
        }
        catch { Data = new VaultConfig(); }

        Directory.CreateDirectory(Data.TorrentDownloadFolder);
    }

    public void Save()
    {
        Directory.CreateDirectory(Dir);
        File.WriteAllText(FilePath, JsonConvert.SerializeObject(Data, Formatting.Indented));
    }
}
