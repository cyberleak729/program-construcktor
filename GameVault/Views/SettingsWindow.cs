using System.Windows;
using System.Windows.Input;
using GameVault.Core;

namespace GameVault.Views;

public partial class SettingsWindow : Window
{
    readonly ConfigStore _cfg;

    public SettingsWindow(ConfigStore cfg)
    {
        InitializeComponent();
        _cfg = cfg;
        DownBox.Text = cfg.Data.TorrentDownloadFolder;
        UpLimitBox.Text = cfg.Data.MaxUpSpeedKBs.ToString();
        DownLimitBox.Text = cfg.Data.MaxDownSpeedKBs.ToString();
        IndexersBox.Text = string.Join("\n", cfg.Data.TorznabIndexers);
    }

    void Save_Click(object sender, RoutedEventArgs e)
    {
        if (!string.IsNullOrWhiteSpace(DownBox.Text))
            _cfg.Data.TorrentDownloadFolder = DownBox.Text.Trim();
        if (int.TryParse(UpLimitBox.Text, out var up)) _cfg.Data.MaxUpSpeedKBs = up;
        if (int.TryParse(DownLimitBox.Text, out var dn)) _cfg.Data.MaxDownSpeedKBs = dn;

        _cfg.Data.TorznabIndexers = IndexersBox.Text
            .Split('\n', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Where(l => l.StartsWith("http"))
            .ToList();

        _cfg.Save();
        MessageBox.Show(this, "Сохранено. Лимиты скорости применятся после перезапуска лаунчера.", "Настройки");
        Close();
    }

    void PickFolder_Click(object sender, RoutedEventArgs e)
    {
        var dlg = new Microsoft.Win32.OpenFolderDialog { Title = "Папка загрузок торрентов" };
        if (dlg.ShowDialog() == true) DownBox.Text = dlg.FolderName;
    }
}
