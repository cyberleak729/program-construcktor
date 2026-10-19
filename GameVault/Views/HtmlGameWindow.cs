using System.Diagnostics;
using System.IO;
using System.Windows.Controls;
using GameVault.Core;

namespace GameVault.Views;

/// <summary>Окно встроенного браузера для HTML-игр на WebView2 (Chromium: WebGL, WebAudio, gamepad).</summary>
public partial class HtmlGameWindow : Window
{
    public HtmlGameWindow(string title, string htmlPathOrUrl)
    {
        InitializeComponent();
        Title = "Игра · " + title;

        Loaded += async (_, _) =>
        {
            try
            {
                var userData = Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GameVault", "WebView2");
                Directory.CreateDirectory(userData);

                // явный environment с изолированным user-data folder — без реестра, переносимый кэш
                var env = await Microsoft.Web.WebView2.Core.CoreWebView2Environment.CreateAsync(null, userData);
                await Wv.EnsureCoreWebView2Async(env);
                Wv.CoreWebView2.Settings.AreDefaultContextMenusEnabled = false;
                Wv.CoreWebView2.Settings.IsStatusBarEnabled = false;

                var src = htmlPathOrUrl.StartsWith("http", StringComparison.OrdinalIgnoreCase)
                    ? htmlPathOrUrl
                    : new Uri(htmlPathOrUrl).AbsoluteUri;   // file:///... корректные слеши/UTF-8
                Wv.CoreWebView2.Navigate(src);

                // F11 — полноэкранный режим игры
                PreviewKeyDown += (_, e) =>
                {
                    if (e.Key == System.Windows.Input.Key.F11)
                        WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
                };
            }
            catch (Exception ex)
            {
                ErrorText.Text = "WebView2 не доступен: " + ex.Message +
                                 "\nОткрываю игру в системном браузере.";
                ErrorText.Visibility = Visibility.Visible;
                try
                {
                    Process.Start(new ProcessStartInfo(htmlPathOrUrl) { UseShellExecute = true });
                }
                catch { }
            }
        };
    }

    public static void OpenFor(string title, string path)
    {
        var w = new HtmlGameWindow(title, path)
        {
            Width = 1280, Height = 800,
            WindowStartupLocation = WindowStartupLocation.CenterScreen
        };
        w.Show();
    }
}
