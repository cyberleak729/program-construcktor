using System.IO;
using System.Net.Http;
using System.Windows.Media.Imaging;
using GameVault.Core;

namespace GameVault.Services;

/// <summary>Скачивание и кэш обложек. Путь кэша: %LOCALAPPDATA%\GameVault\covers\&lt;id&gt;.jpg</summary>
public sealed class CoverCacheService
{
    static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(20) };

    public static async Task DownloadToCacheAsync(GameEntry g)
    {
        try
        {
            if (!string.IsNullOrEmpty(g.HeaderUrl))
                g.CachedHeader = await FetchImageAsync(g.HeaderUrl, Path.Combine(LibraryStore.CoverCache, g.Id + "_h.jpg"));
            if (!string.IsNullOrEmpty(g.LogoUrl))
                g.CachedLogo = await FetchImageAsync(g.LogoUrl, Path.Combine(LibraryStore.CoverCache, g.Id + "_l.jpg"));
        }
        catch { /* нет обложки — UI покажет заглушку */ }
    }

    static async Task<string?> FetchImageAsync(string url, string dest)
    {
        if (File.Exists(dest) && new FileInfo(dest).Length > 0) return dest;
        try
        {
            using var resp = await Http.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);
            if (!resp.IsSuccessStatusCode) return null;
            await using var fs = File.Create(dest);
            await resp.Content.CopyToAsync(fs);
            return dest;
        }
        catch
        {
            try { if (File.Exists(dest)) File.Delete(dest); } catch { }
            return null;
        }
    }

    /// <summary>Загрузка из кэша в BitmapImage с заморозкой (потокобезопасно для VirtualizingStackPanel).</summary>
    public static BitmapImage? LoadCached(string? path)
    {
        if (string.IsNullOrEmpty(path) || !File.Exists(path)) return null;
        try
        {
            var bmp = new BitmapImage();
            bmp.BeginInit();
            bmp.CacheOption = BitmapCacheOption.OnLoad;   // читаем сразу, файл не держим
            bmp.UriSource = new Uri(path, UriKind.Absolute);
            bmp.DecodePixelWidth = 460;                   // экономия памяти на сетке обложек
            bmp.EndInit();
            bmp.Freeze();
            return bmp;
        }
        catch { return null; }
    }
}
