using System.Text.Json.Serialization;

namespace GameVault.Core;

public enum GamePlatform
{
    Html,        // локальная HTML5-игра (WebView2)
    Steam,       // steam://rungameid/<appid>
    Epic,        // com.epicgames.launcher://apps/<name>?action=launch
    Gog,         // goggame-<hash>.bat / гог запускается своим протоколом
    AmazonGames, // amzn-games://play/launch/<appName>
    LocalExe     // любая .exe / папка с index.html
}

public sealed class GameEntry
{
    public string Id { get; set; } = Guid.NewGuid().ToString("N");
    public string Title { get; set; } = "";
    public GamePlatform Platform { get; set; }

    // Steam
    public string? AppId { get; set; }

    // Epic: displayName из манифеста + InstallDirectory name
    public string? EpicAppName { get; set; }
    public string? EpicCatalogItemId { get; set; }

    // Local exe
    public string? ExePath { get; set; }
    public string? Arguments { get; set; }
    public string? WorkingDirectory { get; set; }

    // HTML-игра: путь к index.html или http-ссылка
    public string? HtmlPath { get; set; }

    // Обложки: логотип + шапка (header). Скачиваются в кэш, здесь относительные пути кэша
    public string? LogoUrl { get; set; }      // оригинальная ссылка (для повторной закачки)
    public string? HeaderUrl { get; set; }
    public string? CachedLogo { get; set; }   // локальный файл кэша
    public string? CachedHeader { get; set; }

    // Торрент-происхождение
    public string? MagnetLink { get; set; }
    public string? InstallFolder { get; set; } // куда распаковали

    public DateTime AddedUtc { get; set; } = DateTime.UtcNow;
    public bool Favorite { get; set; }
    public int PlayCount { get; set; }
    public DateTime? LastPlayedUtc { get; set; }

    [JsonIgnore]
    public string PlatformDisplay => Platform switch
    {
        GamePlatform.Html => "HTML",
        GamePlatform.Steam => "Steam",
        GamePlatform.Epic => "Epic Games",
        GamePlatform.Gog => "GOG",
        GamePlatform.AmazonGames => "Amazon Games",
        _ => "Локальная"
    };
}
