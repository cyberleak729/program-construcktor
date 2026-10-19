using System.Windows;
using System.Windows.Input;
using GameVault.Services;

namespace GameVault.Views;

public partial class SearchWindow : Window
{
    public sealed class Row
    {
        public string Title { get; init; } = "";
        public string? Magnet { get; init; }
        public string SizeText { get; init; } = "";
        public string SeedText { get; init; } = "";
    }

    readonly SearchService _search;
    List<Row> _rows = new();

    public SearchWindow(SearchService search)
    {
        InitializeComponent();
        _search = search;
        QueryBox.GotFocus += (_, _) => { if (QueryBox.Text == "Введите название игры…") QueryBox.Text = ""; };
    }

    async void Search_Click(object sender, RoutedEventArgs e) => await RunSearch();
    async void QueryBox_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter) await RunSearch();
    }

    async System.Threading.Tasks.Task RunSearch()
    {
        var q = QueryBox.Text.Trim();
        if (q.Length < 2 || q == "Введите название игры…") return;
        HintBar.Text = "Ищу…";
        ResultsList.ItemsSource = null;
        try
        {
            var found = await _search.SearchAsync(q, CancellationToken.None);
            _rows = found.Select(r => new Row
            {
                Title = r.Title,
                Magnet = r.Magnet ?? BuildMagnet(r.InfoHash),
                SizeText = FmtSize(r.SizeBytes),
                SeedText = "S: " + r.Seeders
            }).Where(x => x.Magnet != null).ToList();
            ResultsList.ItemsSource = _rows;
            HintBar.Text = $"Найдено: {_rows.Count}. Двойной клик — скачать.";
        }
        catch (Exception ex) { HintBar.Text = "Ошибка поиска: " + ex.Message; }
    }

    static string? BuildMagnet(string? hash) =>
        string.IsNullOrEmpty(hash) ? null : "magnet:?xt=urn:btih:" + hash;

    static string FmtSize(long b) => b switch
    {
        <= 0 => "—",
        < 1073741824 => $"{b / 1048576.0:F0} МБ",
        _ => $"{b / 1073741824.0:F1} ГБ"
    };

    async void ResultsList_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (ResultsList.SelectedItem is not Row row || row.Magnet == null) return;
        if (App.MainVm == null) return;
        await App.MainVm.DownloadTorrentAsync(row.Magnet, CleanTitle(row.Title));
        HintBar.Text = "Добавлено в загрузки — смотрите вкладку «Загрузки».";
        Close();
    }

    static string CleanTitle(string t)
    {
        // "Game.Name.Repack-FLT" -> "Game Name Repack"
        var cut = t.Split(new[] { ' ', '[' }, 2)[0];
        var parts = cut.Split('-', '_', '.', ' ');
        return string.Join(" ", parts.Where(p => p.Length > 0 && !p.All(char.IsDigit)));
    }
}
