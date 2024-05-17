using ImHex;

public static class Library
{
    public static void Initialize()
    {
        Logger.RedirectConsole();
    }
}

public interface IScript {

    static void Main() { }
    static void OnLoad() { }

}