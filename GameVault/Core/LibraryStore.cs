using System.IO;
using Newtonsoft.Json;

namespace GameVault.Core;

public sealed class LibraryStore
{
    static readonly string Dir = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "GameVault");
    static readonly string FilePath = Path.Combine(Dir, "library.json");
    static readonly string CoverCacheDir = Path.Combine(Dir, "covers");

    public static string CoverCache => CoverCacheDir;

    List<GameEntry> _items = new();
    public IReadOnlyList<GameEntry> Items => _items;

    public event Action? Changed;

    public void Load()
    {
        try
        {
            if (File.Exists(FilePath))
                _items = JsonConvert.DeserializeObject<List<GameEntry>>(File.ReadAllText(FilePath)) ?? new();
        }
        catch { _items = new(); }
        Directory.CreateDirectory(CoverCacheDir);
    }

    public void Save()
    {
        Directory.CreateDirectory(Dir);
        var json = JsonConvert.SerializeObject(_items, Formatting.Indented);
        var tmp = FilePath + ".tmp";
        File.WriteAllText(tmp, json);          // атомарная запись: не побьём базу при вылете
        if (File.Exists(FilePath)) File.Replace(tmp, FilePath, null);
        else File.Move(tmp, FilePath);
    }

    public void Add(GameEntry g) { _items.Add(g); Save(); Changed?.Invoke(); }
    public void Remove(string id) { _items.RemoveAll(x => x.Id == id); Save(); Changed?.Invoke(); }
    public void Update() { Save(); Changed?.Invoke(); }

    public GameEntry? Find(Predicate<GameEntry> p) => _items.Find(p);
}
