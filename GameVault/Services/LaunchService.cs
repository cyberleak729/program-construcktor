using System.Diagnostics;
using System.IO;
using GameVault.Core;

namespace GameVault.Services;

/// <summary>Запуск игр: HTML через WebView2-окно, Steam/Epic/GOG/Amazon — через URL-протоколы, exe напрямую.</summary>
public sealed class LaunchService
{
    readonly LibraryStore _library;

    public LaunchService(LibraryStore library) => _library = library;

    public async Task<(bool ok, string error)> LaunchAsync(GameEntry g)
    {
        try
        {
            switch (g.Platform)
            {
                case GamePlatform.Html:
                    var html = ResolveHtml(g);
                    if (html == null) return (false, "Не найден index.html игры");
                    // Окно с WebView2 открывает MainViewModel
                    Views.HtmlGameWindow.OpenFor(g.Title, html);
                    break;

                case GamePlatform.Steam:
                    if (string.IsNullOrEmpty(g.AppId)) return (false, "Нет AppId");
                    Shell($"steam://rungameid/{g.AppId}");
                    break;

                case GamePlatform.Epic:
                    if (string.IsNullOrEmpty(g.EpicAppName)) return (false, "Нет EpicAppName");
                    // 1) пробуем прямой exe из InstallLocation — быстрее и не будит Epic Online Services UI
                    var exe = FindEpicExe(g);
                    if (exe != null) Process.Start(new ProcessStartInfo(exe) { UseShellExecute = true });
                    else Shell($"com.epicgames.launcher://apps/{g.EpicAppName}?action=launch&silent=true");
                    break;

                case GamePlatform.Gog:
                    Shell($"gog-game-id://{g.AppId}"); // GOG Galaxy перехватывает свой протокол
                    break;

                case GamePlatform.AmazonGames:
                    Shell($"amzn-games://play/launch/{g.AppId}");
                    break;

                case GamePlatform.LocalExe:
                    if (string.IsNullOrEmpty(g.ExePath) || !File.Exists(g.ExePath))
                        return (false, "Файл не найден: " + g.ExePath);
                    Process.Start(new ProcessStartInfo(g.ExePath)
                    {
                        Arguments = g.Arguments ?? "",
                        WorkingDirectory = g.WorkingDirectory ?? Path.GetDirectoryName(g.ExePath),
                        UseShellExecute = true
                    });
                    break;
            }

            g.PlayCount++;
            g.LastPlayedUtc = DateTime.UtcNow;
            _library.Update();
            return (true, "");
        }
        catch (Exception ex)
        {
            return (false, ex.Message);
        }
    }

    static string? ResolveHtml(GameEntry g)
    {
        if (string.IsNullOrEmpty(g.HtmlPath)) return null;
        if (g.HtmlPath.StartsWith("http", StringComparison.OrdinalIgnoreCase)) return g.HtmlPath;
        if (File.Exists(g.HtmlPath)) return g.HtmlPath;
        var idx = Path.Combine(g.HtmlPath, "index.html");
        if (File.Exists(idx)) return idx;
        var any = Directory.Exists(g.HtmlPath)
            ? Directory.GetFiles(g.HtmlPath, "*.html", SearchOption.TopDirectoryOnly).FirstOrDefault()
            : null;
        return any;
    }

    static string? FindEpicExe(GameEntry g)
    {
        if (string.IsNullOrEmpty(g.InstallFolder)) return null;
        try
        {
            foreach (var f in Directory.EnumerateFiles(g.InstallFolder, "*.exe", SearchOption.AllDirectories))
            {
                var name = Path.GetFileName(f).ToLowerInvariant();
                if (name.Contains("unins") || name.Contains("redist") || name.Contains("vc") ||
                    name.StartsWith("epic") || name.Contains("launcher") || name.Contains("prereq")) continue;
                // эвристика: главный exe обычно лежит в корне установки
                if (Path.GetDirectoryName(f)!.Equals(g.InstallFolder, StringComparison.OrdinalIgnoreCase))
                    return f;
            }
        }
        catch { }
        return null;
    }

    static void Shell(string uri) =>
        Process.Start(new ProcessStartInfo(uri) { UseShellExecute = true });
}
