using System.Net.Http;
using System.Text;
using System.Xml.Linq;

namespace GameVault.Services;

public sealed record TorrentResult(string Title, long SizeBytes, string? InfoHash, string? Magnet, List<string> Files, int Seeders);

/// <summary>
/// Поиск торрентов: Torznab-Prowlarr/Jackett-индексаторы (пользователь вписывает свои endpoints в настройках)
/// + публичный JSON-каталог как fallback без настройки.
/// </summary>
public sealed class SearchService
{
    static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(20) };

    public List<string> Indexers { get; set; } = new(); // пример: http://192.168.1.10:9117/api/v2.0/indexers/all/results/torznab/api?apikey=XXX&t=search&cat=5000&q={query}

    public async Task<List<TorrentResult>> SearchAsync(string query, CancellationToken ct)
    {
        var results = new List<TorrentResult>();
        var q = Uri.EscapeDataString(query);

        foreach (var raw in Indexers.Where(x => !string.IsNullOrWhiteSpace(x)))
        {
            try
            {
                var url = raw.Contains("{query}") ? raw.Replace("{query}", q) : raw + "&q=" + q;
                var xml = await Http.GetStringAsync(url, ct);
                var doc = XDocument.Parse(xml);
                foreach (var item in doc.Descendants("item"))
                {
                    var title = item.Element("title")?.Value ?? "";
                    var link = item.Element("link")?.Value ?? "";
                    var size = item.Attribute(XNamespace.Get("http://newznab.com/rss/newznab") + "size")?.Value;
                    var seeds = item.Attribute(XNamespace.Get("http://torznab.com/schemas/2015/feed") + "seeders")?.Value;
                    var files = item.Elements(XNamespace.Get("http://torznab.com/schemas/2015/feed") + "attr")
                        .Where(a => a.Attribute("name")?.Value == "files")
                        .Select(a => a.Attribute("value")?.Value ?? "").ToList();
                    if (string.IsNullOrEmpty(title)) continue;
                    long.TryParse(size, out var sz);
                    int.TryParse(seeds, out var sd);
                    results.Add(new TorrentResult(title, sz, ExtractHash(link), link.StartsWith("magnet") ? link : null, files, sd));
                }
            }
            catch { /* индексатор недоступен — идём дальше */ }
        }

        // Fallback: открытый каталог GameVault-демо (замените на свой JSON-индекс при желании)
        if (results.Count == 0)
        {
            try
            {
                const string catalog = "https://gist.githubusercontent.com/gamevault-index/games/raw/catalog.json";
                var json = await Http.GetStringAsync(catalog, ct);
                foreach (var r in Newtonsoft.Json.JsonConvert.DeserializeObject<List<TorrentResult>>(json) ?? new())
                    if (r.Title.Contains(query, StringComparison.OrdinalIgnoreCase))
                        results.Add(r);
            }
            catch { }
        }

        return results.OrderByDescending(r => r.Seeders).ThenByDescending(r => r.SizeBytes).Take(100).ToList();
    }

    static string? ExtractHash(string link)
    {
        var i = link.IndexOf("xt=urn:btih:", StringComparison.OrdinalIgnoreCase);
        if (i < 0) return null;
        var sb = new StringBuilder();
        for (int j = i + 11; j < link.Length && Uri.IsHexDigit(link[j]); j++) sb.Append(char.ToLowerInvariant(link[j]));
        return sb.Length is 40 or 32 ? sb.ToString() : null;
    }
}
