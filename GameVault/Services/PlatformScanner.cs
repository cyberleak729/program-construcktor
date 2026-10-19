using System.IO;
using System.Net.Http;
using System.Text.Json;

namespace GameVault.Services;

/// <summary>Автоматическое обнаружение установленных игр Steam/Epic/GOG/Amazon + обложки из CDN.</summary>
public sealed class PlatformScanner
{
    static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(15) };

    // ---------- STEAM ----------
    public static string? FindSteamAppsFolder()
    {
        foreach (var pf in ProgramFilesCandidates())
        {
            var p = Path.Combine(pf, "steam", "steamapps");
            if (Directory.Exists(p)) return p;
        }
        // реестр HKCU\Software\Valve\Steam -> SteamPath
        try
        {
            using var k = Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam");
            var sp = k?.GetValue("SteamPath") as string;
            if (!string.IsNullOrEmpty(sp))
            {
                var p = Path.Combine(sp!, "steamapps");
                if (Directory.Exists(p)) return p.Replace('/', '\\');
            }
        }
        catch { }
        return null;
    }

    static IEnumerable<string> ProgramFilesCandidates()
    {
        yield return Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86);
        yield return Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
        yield return @"D:\Steam";
        yield return @"C:\Steam";
    }

    public sealed record SteamGame(uint AppId, string Name, string InstallDir);

    public static List<SteamGame> ScanSteam()
    {
        var result = new List<SteamGame>();
        var apps = FindSteamAppsFolder();
        if (apps == null) return result;

        // libraryfolders.vdf — все библиотеки Steam
        var folders = new List<string> { apps };
        var vdf = Path.Combine(apps, "libraryfolders.vdf");
        if (File.Exists(vdf))
        {
            var rx = new System.Text.RegularExpressions.Regex("\"path\"\\s+\"([^\"]+)\"");
            foreach (System.Text.RegularExpressions.Match m in rx.Matches(File.ReadAllText(vdf)))
            {
                var f = m.Groups[1].Value.Replace("\\\\", "\\");
                var full = Path.Combine(f, "steamapps");
                if (Directory.Exists(full)) folders.Add(full);
            }
        }

        foreach (var folder in folders.Distinct())
        foreach (var acf in Directory.GetFiles(folder, "*.acf"))
        {
            try
            {
                var txt = File.ReadAllText(acf);
                var appId = VdfGet(txt, "appid");
                var name = VdfGet(txt, "name");
                var inst = VdfGet(txt, "installdir");
                if (appId == null || name == null) continue;
                result.Add(new SteamGame(uint.Parse(appId), name,
                    Path.Combine(folder, "common", inst ?? "")));
            }
            catch { }
        }
        return result;
    }

    static string? VdfGet(string content, string key)
    {
        var m = System.Text.RegularExpressions.Regex.Match(
            content, $"\"{key}\"\\s+\"([^\"]+)\"", System.Text.RegularExpressions.RegexOptions.IgnoreCase);
        return m.Success ? m.Groups[1].Value : null;
    }

    /// <summary>Обложки Steam CDN: header 600x338 / capsule 231x87 / logo.</summary>
    public static async Task DownloadSteamCoversAsync(Core.GameEntry g)
    {
        if (string.IsNullOrEmpty(g.AppId)) return;
        g.LogoUrl   ??= $"https://cdn.cloudflare.steamstatic.com/steam/apps/{g.AppId}/capsule_231x87.jpg";
        g.HeaderUrl ??= $"https://cdn.cloudflare.steamstatic.com/steam/apps/{g.AppId}/header.jpg";
        await CoverCacheService.DownloadToCacheAsync(g);
    }

    // ---------- EPIC ----------
    public static string EpicManifestDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                     "Epic", "EpicGamesLauncher", "Data", "Manifests");

    public sealed record EpicGame(string AppName, string CatalogItemId, string DisplayName, string InstallLocation);

    public static List<EpicGame> ScanEpic()
    {
        var list = new List<EpicGame>();
        if (!Directory.Exists(EpicManifestDir)) return list;
        foreach (var mf in Directory.GetFiles(EpicManifestDir, "*.manifest"))
        {
            try
            {
                var txt = File.ReadAllText(mf);
                var appName = IniGet(txt, "AppName");
                var catalog = IniGet(txt, "CatalogNamespace") != null ? IniGet(txt, "CatalogItemId") : null;
                var title = IniGet(txt, "DisplayName");
                var loc = IniGet(txt, "InstallLocation");
                if (appName == null || title == null || loc == null) continue;
                if (appName.StartsWith("UE_") || title.Contains("Unreal Launcher") ||
                    (catalog == null && appName.Length == 32 && char.IsLower(appName[0]) && !char.IsLetter(appName[^1])))
                    continue; // скипаем DLC/компоненты движка по эвристике
                list.Add(new EpicGame(appName, catalog ?? "", title, loc));
            }
            catch { }
        }
        return list;
    }

    static string? IniGet(string content, string key)
    {
        var m = System.Text.RegularExpressions.Regex.Match(content, $"^{key}=(.*)$", System.Text.RegularExpressions.RegexOptions.Multiline);
        return m.Success && m.Groups[1].Value.Length > 0 ? m.Groups[1].Value.Trim('"') : null;
    }

    /// <summary>Epic Artifacts CDN по catalogItemId. Если нет — генерируем цветную заглушку.</summary>
    public static async Task DownloadEpicCoversAsync(Core.GameEntry g)
    {
        if (!string.IsNullOrEmpty(g.EpicCatalogItemId))
        {
            g.HeaderUrl ??= $"https://cdn1.epicgames.com/epic/ads-{g.EpicCatalogItemId}-1440x810-abcde-1440x810";
            try
            {
                // официальный ключ изображений — через lightbox API не нужен; пробуем прямой cdnn IMAGE_BASE
                var json = await Http.GetStringAsync(
                    $"https://searchcontent.epicgames.com/content/api/v4/pages/category-page/gridTile/{g.EpicCatalogItemId}?locale=ru&country=RU");
                using var d = JsonDocument.Parse(json);
                if (d.RootElement.TryGetProperty("images", out var imgs))
                {
                    foreach (var img in imgs.EnumerateArray())
                    {
                        var type = img.TryGetProperty("type", out var t) ? t.GetString() : "";
                        var url = img.TryGetProperty("url", out var u) ? u.GetString() : null;
                        if (url == null) continue;
                        if (type == "OfferImageWide") g.HeaderUrl = url;
                        else if (type == "DieselStoreFrontWide" || type == "Thumbnail") g.LogoUrl ??= url;
                    }
                }
            }
            catch { /* остаётся заглушка */ }
        }
        await CoverCacheService.DownloadToCacheAsync(g);
    }

    // ---------- GOG / AMAZON ----------
    public static string GogStorageDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GOG.com", "Galaxy", "storage");

    public static List<(string Id, string Title)> ScanGog()
    {
        var list = new List<(string, string)>();
        if (!Directory.Exists(GogStorageDir)) return list;
        foreach (var file in Directory.GetFiles(GogStorageDir, "*.json"))
        {
            try
            {
                using var d = JsonDocument.Parse(File.ReadAllText(file));
                if (d.RootElement.TryGetProperty("installedGames", out var games))
                    foreach (var gg in games.EnumerateArray())
                    {
                        var id = gg.TryGetProperty("gameId", out var i) ? i.ToString() : "";
                        var title = gg.TryGetProperty("title", out var t) ? t.GetString() ?? "" : "";
                        if (!string.IsNullOrEmpty(title)) list.Add((id, title));
                    }
            }
            catch { }
        }
        return list;
    }

    public static string AmazonGamesDbDir =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "AWS", "AGS", "LaunchDb");

    public static List<(string AppName, string Title)> ScanAmazonGames()
    {
        var list = new List<(string, string)>();
        if (!Directory.Exists(AmazonGamesDbDir)) return list;
        foreach (var file in Directory.GetFiles(AmazonGamesDbDir, "*.json"))
        {
            try
            {
                using var d = JsonDocument.Parse(File.ReadAllText(file));
                var root = d.RootElement;
                var appName = root.TryGetProperty("appName", out var a) ? a.GetString() ?? "" : "";
                var title = root.TryGetProperty("gameTitle", out var t) ? t.GetString() ?? "" : appName;
                if (!string.IsNullOrEmpty(appName)) list.Add((appName, title));
            }
            catch { }
        }
        return list;
    }
}
