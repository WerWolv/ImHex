#!/usr/bin/env python3
import json
import re
import os
import sys

SHOW_UNUSED_LANGS = "--unused" in sys.argv

# use a regex on code to get all "hex.lang.id"_lang occurences
def get_lang_occurrences_in_code(path):
    for dir, _, files in os.walk(path):
        for file in files:

            if not os.path.splitext(file)[1] in (".cpp", ".c", ".hpp", ".h"):
                continue

            filepath = os.path.join(dir, file)

            with open(filepath, encoding="utf8") as file:
                for line_num, line in enumerate(file):
                    for m in re.finditer('"([^"]*?)"_lang', line):
                        yield (filepath, line_num+1, m.group(1))

# Get langs in a specific json file
def get_langs(filepath) -> list[str]:
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

        return existing_langs

def check_langs(code_path, bonus_langs, specific_langs_path):
    print(f"--- Checking langs at {code_path}")
 
    specific_langs = get_langs(specific_langs_path)
    unused_langs = specific_langs.copy()
    ret = True

    for filepath, line, match in get_lang_occurrences_in_code(code_path):
        try:
            unused_langs.remove(match)
        except ValueError:
            pass

        if not match in bonus_langs + specific_langs:
            ret = False
            print(f"Problem: Lang '{match}' at {filepath}:{line} not found")
    
    if SHOW_UNUSED_LANGS and len(unused_langs) > 0:
        print(f"Unused langs in {specific_langs_path}:")
        for unused_lang in unused_langs:
            print(unused_lang)
    return ret

def check_languages_exist(languages_file_path: str):
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


ui_langs = get_langs("./plugins/ui/romfs/lang/en_US.json")

exit_ok = True
exit_ok &= check_langs("./main", ui_langs, None)

for plugin in os.listdir("./plugins"):
    if plugin == "ui": continue

    path = f"./plugins/{plugin}"
    if not os.path.isdir(path): continue

    exit_ok &= check_langs(path, ui_langs, f"./plugins/{plugin}/romfs/lang/en_US.json")
    exit_ok &= check_languages_exist(f"./plugins/{plugin}/romfs/lang/languages.json")

sys.exit(0 if exit_ok else 1)
