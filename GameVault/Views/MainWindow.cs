using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using GameVault.Services;
using GameVault.ViewModels;

namespace GameVault.Views;

public partial class MainWindow : Window
{
    readonly MainViewModel _vm = new();
    readonly SearchService _search = new();

    public MainWindow()
    {
        InitializeComponent();
        DataContext = _vm;
        App.MainVm = _vm;
        _search.Indexers = App.Config.Data.TorznabIndexers;

        StatusText.SetBinding(TextBlock.TextProperty,
            new System.Windows.Data.Binding("Status") { Source = _vm });

        Loaded += async (_, _) => await _vm.ScanPlatformsAsync();
    }

    // ---- вкладки ----
    void TabLibrary_Click(object sender, RoutedEventArgs e)
    {
        LibraryView.Visibility = Visibility.Visible;
        DownloadsView.Visibility = Visibility.Collapsed;
    }
    void TabDownloads_Click(object sender, RoutedEventArgs e)
    {
        LibraryView.Visibility = Visibility.Collapsed;
        DownloadsView.Visibility = Visibility.Visible;
    }

    void SearchBox_TextChanged(object sender, TextChangedEventArgs e) => _vm.SearchText = SearchBox.Text;

    // ---- карточка игры ----
    void Card_Click(object sender, MouseButtonEventArgs e)
    {
        if (sender is not FrameworkElement fe || fe.DataContext is not GameCardVM card) return;
        _vm.Selected = card;
        DetailCover.Source = card.Cover;
        DetailCover.Visibility = Visibility.Visible;
        DetailTitle.Text = card.Title;
        DetailTitle.Visibility = Visibility.Visible;
        DetailPlatform.Text = $"{card.PlatformDisplay} · сыграно раз: {card.Model.PlayCount}";
        DetailPlatform.Visibility = Visibility.Visible;
        PlayBtn.Visibility = Visibility.Visible;
        RemoveBtn.Visibility = Visibility.Visible;
    }

    void Fav_Click(object sender, MouseButtonEventArgs e)
    {
        e.Handled = true;
        if (sender is FrameworkElement fe && fe.DataContext is GameCardVM card)
            card.IsFavorite = !card.IsFavorite;
    }

    async void Play_Click(object sender, RoutedEventArgs e)
    {
        if (_vm.Selected != null)
            await _vm.LaunchAsync(_vm.Selected.Model);
    }

    async void Remove_Click(object sender, RoutedEventArgs e)
    {
        if (_vm.Selected == null) return;
        var r = MessageBox.Show(this, $"Удалить «{_vm.Selected.Title}» из библиотеки?", "GameVault",
            MessageBoxButton.YesNo, MessageBoxImage.Question);
        if (r == MessageBoxResult.Yes)
        {
            await _vm.RemoveGameAsync(_vm.Selected.Model);
            DetailCover.Visibility = DetailTitle.Visibility = DetailPlatform.Visibility =
                PlayBtn.Visibility = RemoveBtn.Visibility = Visibility.Collapsed;
        }
    }

    // ---- ручное добавление ----
    void AddExe_Click(object sender, RoutedEventArgs e) => _vm.AddLocalGame();
    void AddHtml_Click(object sender, RoutedEventArgs e) => _vm.AddHtmlFolder();

    // ---- торрент-поиск / настройки ----
    void TorrentSearch_Click(object sender, RoutedEventArgs e)
    {
        new SearchWindow(_search) { Owner = this }.Show();
    }

    void Settings_Click(object sender, RoutedEventArgs e)
    {
        new SettingsWindow(App.Config) { Owner = this }.ShowDialog();
        _search.Indexers = App.Config.Data.TorznabIndexers;
    }

    // ---- загрузки ----
    async void PauseDl_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: TorrentItemStatus d }) await _vm.PauseDownloadAsync(d);
    }
    async void ResumeDl_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: TorrentItemStatus d }) await _vm.ResumeDownloadAsync(d);
    }
    async void CancelDl_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: TorrentItemStatus d }) await _vm.CancelDownloadAsync(d);
    }
    async void ImportDl_Click(object sender, RoutedEventArgs e)
    {
        if (sender is FrameworkElement { DataContext: TorrentItemStatus d }) await _vm.ImportDownloadedGameAsync(d);
    }
}
