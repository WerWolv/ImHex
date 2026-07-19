#!/usr/bin/env python3
"""Check for unused and non-existent language keys across the codebase.

- Every "_lang" string in C/C++ source must have a matching key in an en_US.json file.
- Every key in an en_US.json file must be referenced somewhere in C/C++ source starting with hex.

Usage:
    python check_langs.py [--unused] [--nonexistent]

Exit code 1 on any mismatch."""
import json
import re
import os
import sys
from collections.abc import Generator

CHECK_UNUSED_LANGS = "--unused" in sys.argv
CHECK_NONEXISTENT_LANGS = "--nonexistent" in sys.argv


def find_lang_keys_in_source(path: str, mode: str) -> Generator[tuple[str, int, str], None, None]:
    """Walk all C/C++ files under path and yield (filepath, line_number, key).

    mode="lang": match "..."_lang patterns.
    mode="hex":  match plain "hex.*" string literals."""
    if mode == "lang":
        pattern = r'"([^"]*?)"_lang'
    elif mode == "hex":
        pattern = r'"(hex\.[a-zA-Z0-9_.]+)"'
    else:
        raise ValueError(f"Unknown mode: {mode}")

    for dir, _, files in os.walk(path):
        for file in files:

            if not os.path.splitext(file)[1] in (".cpp", ".c", ".hpp", ".h"):
                continue

            filepath = os.path.join(dir, file)

            with open(filepath, encoding="utf8") as file:
                for line_num, line in enumerate(file):
                    for m in re.finditer(pattern, line):
                        yield (filepath, line_num+1, m.group(1))


def load_json_lang_keys(filepath: str | None) -> list[str]:
    """Return the list of top-level keys from a lang JSON file, or [] if
    the file doesn't exist."""
    if filepath == None:
        return []
    elif not os.path.exists(filepath):
        print(f"Warning: no langs file found at {filepath}")
        return []

    with open(filepath, "r", encoding="utf8") as file:
        data = json.loads(file.read())
        existing_langs = []

        for key, _ in data.items():
            existing_langs.append(key)

        for lang in existing_langs:
            if not lang.startswith("hex."):
                print(f"Problem: Lang '{lang}' doesn't start with 'hex.'")

        return existing_langs


def check_nonexistent_langs(code_path: str, all_langs: list[str]) -> bool:
    """Check that every "_lang" key found in source exists in the JSON."""
    if not CHECK_NONEXISTENT_LANGS:
        return True

    print(f"--- Checking nonexistent langs at {code_path}")
    ret = True

    for filepath, line, match in find_lang_keys_in_source(code_path, "lang"):
        if not match in all_langs:
            ret = False
            print(f"Problem: Lang '{match}' at {filepath}:{line} not found")
    return ret


def check_unused_langs(unused_langs: list[str], langs_path: str) -> bool:
    """Check for keys in the JSON file that aren't referenced in code."""
    if not CHECK_UNUSED_LANGS or len(unused_langs) == 0:
        return True

    print(f"--- Checking unused langs at {langs_path}")
    for lang in unused_langs:
        print(f"Problem: Unused lang {lang}")
    return False


def verify_language_files_exist(languages_file_path: str) -> bool:
    """Check that a plugin's languages.json references language files that
    actually exist on disk."""
    languages_folder = os.path.dirname(languages_file_path)
    if not os.path.exists(languages_folder):
        return True

    if not os.path.exists(languages_file_path):
        print(f"Error: Languages file '{languages_file_path}' does not exist.")
        return False
    with open(languages_file_path, "r", encoding="utf8") as file:
        try:
            data = json.load(file)
            if not isinstance(data, list):
                print(f"Error: Languages file '{languages_file_path}' is not a valid JSON object.")
                return False

            for lang in data:
                if not os.path.exists(os.path.join(languages_folder, "..", lang['path'])):
                    print(f"Error: Language file '{lang['path']}' does not exist in '{languages_folder}'.")
                    return False
            return True

        except json.JSONDecodeError as e:
            print(f"Error: Languages file '{languages_file_path}' is not a valid JSON file. {e}")
            return False


common_langs = load_json_lang_keys("./plugins/ui/romfs/lang/en_US.json") + load_json_lang_keys("./plugins/builtin/romfs/lang/en_US.json")

exit_ok = True

for plugin in os.listdir("./plugins"):
    path = f"./plugins/{plugin}"
    if not os.path.isdir(path): continue

    specific_langs = load_json_lang_keys(f"./plugins/{plugin}/romfs/lang/en_US.json")

    # Collect hex-mode keys from source to detect unused JSON keys
    hex_keys = set()
    for _, _, key in find_lang_keys_in_source(path, "hex"):
        hex_keys.add(key)
    unused = [k for k in specific_langs if k not in hex_keys]

    exit_ok &= check_nonexistent_langs(path, common_langs + specific_langs)
    exit_ok &= check_unused_langs(unused, f"./plugins/{plugin}/romfs/lang/en_US.json")
    exit_ok &= verify_language_files_exist(f"./plugins/{plugin}/romfs/lang/languages.json")

sys.exit(0 if exit_ok else 1)
