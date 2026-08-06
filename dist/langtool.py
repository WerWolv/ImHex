#!/usr/bin/env python3
"""ImHex language tool.

Manages translations and validates lang keys against C/C++ source code.
Run with --help to see available subcommands.
Terminology:
- langdir: a folder containing language JSON files (e.g. plugins/builtin/romfs/lang/)
- lang: a language (en_US, fr_FR..)
- lang key: a string key used to look up a translation in a JSON file (e.g. hex.ui.common.yes)
"""
import argparse
import json
import re
from collections.abc import Callable, Generator
from pathlib import Path

# This fixes a CJK full-width character input issue
# which makes left halves of deleted characters displayed on screen
import readline  # noqa: F401

DEFAULT_LANG = "en_US"
INVALID_TRANSLATION = ""

def resolve_lang_dirs() -> tuple[list[Path], list[Path]]:
    """Resolve langdir glob into (lang_dirs, source_roots).

    lang_dirs are the actual lang folders (e.g. plugins/builtin/romfs/lang/).
    source_roots are the corresponding C++ source roots (e.g. plugins/builtin/)."""
    lang_dirs = sorted(Path(".").glob("plugins/*/romfs/lang/"))
    source_roots = []
    for lang_dir in lang_dirs:
        # plugins/builtin/romfs/lang/ -> plugins/builtin/
        source_root = lang_dir.parent.parent
        if source_root not in source_roots:
            source_roots.append(source_root)
    return lang_dirs, source_roots


def load_json_data(filepath: Path) -> dict[str, str]:
    """Return the full JSON contents as a dict, or {} if it doesn't exist."""
    if not filepath.exists():
        return {}
    with filepath.open("r", encoding="utf-8-sig") as f:
        return json.load(f)


def write_json(filepath: Path, data: dict[str, str]) -> None:
    """Write a dict to a JSON file with standard formatting."""
    with filepath.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=4, sort_keys=False, ensure_ascii=False)
        f.write("\n")


def find_lang_keys_in_source(path: Path, mode: str) -> Generator[tuple[Path, int, str], None, None]:
    """Walk C/C++ files under path and yield (filepath, line_number, key).

    mode="lang": match "..."_lang patterns.
    mode="hex":  match plain "hex.*" string literals."""
    if mode == "lang":
        pattern = r'"([^"]*?)"_lang'
    elif mode == "hex":
        pattern = r'"(hex\.[a-zA-Z0-9_.]+)"'
    else:
        raise ValueError(f"Unknown mode: {mode}")

    for dirpath, _, files in path.walk():
        for filename in files:
            if Path(filename).suffix not in (".cpp", ".c", ".hpp", ".h"):
                continue

            filepath = dirpath / filename
            with filepath.open(encoding="utf-8") as f:
                for line_num, line in enumerate(f, 1):
                    for m in re.finditer(pattern, line):
                        yield (filepath, line_num, m.group(1))


def verify_language_files_exist(languages_file_path: Path) -> bool:
    """Check that a plugin's languages.json references files that exist on disk."""
    languages_folder = languages_file_path.parent
    if not languages_folder.exists():
        return True

    if not languages_file_path.exists():
        print(f"Error: Languages file '{languages_file_path}' does not exist.")
        return False

    try:
        data = json.loads(languages_file_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"Error: Languages file '{languages_file_path}' is not valid JSON: {e}")
        return False

    if not isinstance(data, list):
        print(f"Error: Languages file '{languages_file_path}' is not a JSON array.")
        return False

    for lang in data:
        lang_path = languages_folder.parent / lang["path"]
        if not lang_path.exists():
            print(f"Error: Language file '{lang['path']}' does not exist in '{languages_folder}'.")
            return False

    return True

def cmd_check(args: argparse.Namespace) -> int:
    """Check that all non-English translation files have every key from en_US."""
    ret = 0

    def check_callback(lang_data: dict[str, str], default_data: dict[str, str], path: Path) -> None:
        nonlocal ret
        for key, value in default_data.items():
            if key in lang_data and lang_data[key] != INVALID_TRANSLATION:
                continue
            print(f"Error: Translation {path} is missing translation for key '{key}'")
            ret = 1

    _for_each_lang_file(args.lang, check_callback)
    return ret


# Print one interactive translation prompt line with optional existing value.
def _print_translation_prompt(key: str, value: str, current_value: str | None = None) -> None:
    """Print a formatted prompt line for translation operations."""
    print(f"\033[1m'{key}' '{value}'\033[0m => ", end="")
    if current_value is not None:
        print(f" <= \033[1m'{current_value}'\033[0m")
    print()


def _run_translate(args: argparse.Namespace, retranslate: bool = False) -> int:
    """Run interactive translation, optionally limited to matching existing keys."""
    key_pattern = re.compile(args.keys)

    # Iterate keys and prompt for translated values.
    def translate_callback(lang_data: dict[str, str], default_data: dict[str, str], path: Path) -> dict[str, str]:
        for key, value in default_data.items():
            has_translation = key in lang_data and lang_data[key] != INVALID_TRANSLATION

            if retranslate:
                if not has_translation or not key_pattern.fullmatch(key):
                    continue
            elif has_translation:
                continue

            _print_translation_prompt(
                key,
                value,
                lang_data[key] if has_translation else None,
            )

            try:
                new_value = input("=> ")
                lang_data[key] = new_value
            except KeyboardInterrupt:
                break

        return lang_data

    _for_each_lang_file(args.lang, translate_callback)
    return 0


def _run_untranslate(args: argparse.Namespace) -> int:
    """Clear translated values for matching keys."""
    key_pattern = re.compile(args.keys)

    # Limit clearing to keys that are translated and regex-matched.
    def untranslate_callback(lang_data: dict[str, str], default_data: dict[str, str], path: Path) -> dict[str, str]:
        for key, value in default_data.items():
            has_translation = key in lang_data and lang_data[key] != INVALID_TRANSLATION
            if not has_translation or not key_pattern.fullmatch(key):
                continue

            _print_translation_prompt(key, value, lang_data[key])
            lang_data[key] = INVALID_TRANSLATION

        return lang_data

    _for_each_lang_file(args.lang, untranslate_callback)
    return 0


def cmd_sync_sublangs(args: argparse.Namespace) -> int:
    """Sync non-English files with en_US (add missing blank entries, remove orphans)."""

    def sync_callback(lang_data: dict[str, str], default_data: dict[str, str], path: Path) -> dict[str, str]:
        for key in default_data:
            if key not in lang_data:
                lang_data[key] = INVALID_TRANSLATION
        _remove_orphaned_keys(lang_data, default_data, path)
        return lang_data

    _for_each_lang_file(args.lang, sync_callback)
    return 0


def cmd_fmtzh(args: argparse.Namespace) -> int:
    """Fix CJK full-width punctuation in translations."""

    def fmtzh_callback(lang_data: dict[str, str], default_data: dict[str, str], path: Path) -> dict[str, str]:
        for key in default_data:
            if key not in lang_data or lang_data[key] == INVALID_TRANSLATION:
                continue
            lang_data[key] = _fmtzh(lang_data[key])
        return lang_data

    _for_each_lang_file(args.lang, fmtzh_callback)
    return 0

def cmd_check_nonexistent(args: argparse.Namespace) -> int:
    """Verify every "_lang" key in C++ source has a matching JSON entry.

    Matches tests/check_langs.py: keys are accepted if present in either the
    ui/builtin common en_US files or in the plugin's own en_US file."""
    lang_dirs, source_roots = resolve_lang_dirs()

    common_keys = set()
    for common_plugin in ("ui", "builtin"):
        common_path = Path(f"./plugins/{common_plugin}/romfs/lang/{DEFAULT_LANG}.json")
        if common_path.exists():
            common_keys.update(load_json_data(common_path).keys())

    plugin_keys: dict[Path, set[str]] = {}
    for lang_dir in lang_dirs:
        plugin_root = lang_dir.parent.parent
        plugin_keys[plugin_root] = set(load_json_data(lang_dir / f"{DEFAULT_LANG}.json").keys())

    ret = 0
    for source_root in source_roots:
        print(f"--- Checking nonexistent lang keys at {source_root}")
        allowed_keys = common_keys | plugin_keys.get(source_root, set())
        for filepath, line, key in find_lang_keys_in_source(source_root, "lang"):
            if key not in allowed_keys:
                ret = 1
                print(f"Problem: Lang '{key}' at {filepath}:{line} not found")

    return ret

def _handle_unused(remove: bool) -> int:
    """Shared logic for check-unused and remove-unused.

    Matches tests/check_langs.py: only a plugin's own en_US.json keys are checked
    against hex.* string literals found in that same plugin's source."""
    _, source_roots = resolve_lang_dirs()

    ret = 0
    for source_root in source_roots:
        lang_file = source_root / "romfs" / "lang" / f"{DEFAULT_LANG}.json"
        lang_data = load_json_data(lang_file)
        if not lang_data:
            continue

        hex_keys: set[str] = set()
        for _, _, key in find_lang_keys_in_source(source_root, "hex"):
            hex_keys.add(key)

        keys_to_handle = [k for k in lang_data if k not in hex_keys]
        if not keys_to_handle:
            continue

        if remove:
            print(f"--- Removing unused keys from {lang_file}")
            for key in keys_to_handle:
                del lang_data[key]
                print(f"  Removed '{key}'")
            write_json(lang_file, lang_data)
        else:
            ret = 1
            print(f"--- Unused lang keys in {lang_file}")
            for key in keys_to_handle:
                print(f"  {key}")

    return ret

def _for_each_lang_file(
    lang: str,
    callback: Callable[[dict[str, str], dict[str, str], Path], dict[str, str] | None],
) -> None:
    """Iterate over non-English lang files and apply a callback.

    Resolves lang dirs, loads the en_US default, and for each target lang file
    calls callback(lang_data, default_data, path). If the callback returns a
    dict, it is written back to the file."""
    lang_dirs, _ = resolve_lang_dirs()

    for lang_folder in lang_dirs:
        print(f"\nProcessing lang folder '{lang_folder}'")
        default_lang_path = lang_folder / f"{DEFAULT_LANG}.json"
        if not default_lang_path.exists():
            print(f"Error: Default language file {default_lang_path} does not exist")
            continue

        default_lang_data = load_json_data(default_lang_path)
        lang_files = _get_target_lang_files(lang_folder, lang)

        for lang_file_path in lang_files:
            print(f"Processing '{lang_file_path}'")

            lang_data = load_json_data(lang_file_path)
            if not isinstance(lang_data, dict):
                print(f"Skipping non-language file '{lang_file_path}'")
                continue

            result = callback(lang_data, default_lang_data, lang_file_path)

            if result is not None:
                write_json(lang_file_path, result)


def _get_target_lang_files(lang_folder: Path, lang: str) -> list[Path]:
    """Get the list of non-default lang files to process."""
    glob_pattern = f"{lang}.json" if lang else "*.json"
    results = []
    for p in sorted(lang_folder.glob(glob_pattern)):
        if p.stem == DEFAULT_LANG:
            continue
        results.append(p)
    return results


def _remove_orphaned_keys(lang_data: dict[str, str], default_data: dict[str, str], lang_file_path: Path) -> None:
    """Remove keys from lang_data that don't exist in default_data."""
    keys_to_remove = [k for k in lang_data if k not in default_data]
    for key in keys_to_remove:
        del lang_data[key]
        print(f"Removed unused key '{key}' from '{lang_file_path}'")


def _fmtzh(text: str) -> str:
    """Fix CJK full-width punctuation."""
    text = re.sub(r"(\.{3}|\.{6})", "……", text)
    text = text.replace("!", "！")
    text = re.sub(r"([^\.\na-zA-Z\d])\.$", r"\1。", text, flags=re.M)
    text = text.replace("?", "？")
    return text

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="langtool",
        description="ImHex language tool",
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    # Translation commands
    _add_translation_subparser(subparsers, "check", "Check non-English files have all translations")
    p_translate = _add_translation_subparser(
        subparsers,
        "translate",
        "Interactively translate keys (use --retranslate for already translated keys)",
    )
    p_translate.add_argument(
        "--retranslate",
        action="store_true",
        help="Only edit keys that already have a translation",
    )
    p_translate.add_argument(
        "-k", "--keys", default=".*",
        help="Regex pattern used with --retranslate (default: %(default)s)",
    )
    _add_translation_subparser(subparsers, "sync-sublangs", "Sync non-English files with en_US (Both adds and removes entries)")
    _add_translation_subparser(subparsers, "fmtzh", "Fix CJK punctuation in translations")

    # Untranslate needs --keys
    p_untranslate = _add_translation_subparser(subparsers, "untranslate", "Clear translations matching a regex")
    p_untranslate.add_argument("-k", "--keys", required=True, help="Regex pattern to match keys")

    # Source-checking commands (no --lang needed)
    subparsers.add_parser("check-nonexistent", help="Report _lang keys in C++ source that do not exist in JSON")
    subparsers.add_parser("check-unused", help="Report JSON keys not referenced in C++ source")
    subparsers.add_parser("remove-unused", help="Remove JSON keys not referenced in C++ source")

    return parser


def _add_translation_subparser(subparsers, name: str, help_text: str) -> argparse.ArgumentParser:
    """Add a subparser for a translation command with standard translation args."""
    p = subparsers.add_parser(name, help=help_text)
    p.add_argument("-l", "--lang", default="", help="Target language (e.g. de_DE)")
    return p


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    commands = {
        "check": cmd_check,
        "translate": lambda args: _run_translate(args, retranslate=args.retranslate),
        "untranslate": _run_untranslate,
        "sync-sublangs": cmd_sync_sublangs,
        "fmtzh": cmd_fmtzh,
        "check-nonexistent": cmd_check_nonexistent,
        "check-unused": lambda args: _handle_unused(remove=False),
        "remove-unused": lambda args: _handle_unused(remove=True),
    }

    return commands[args.command](args)


if __name__ == "__main__":
    exit(main())
