#!/usr/bin/env python3
from pathlib import Path
import argparse
import json

DEFAULT_LANG = "en_US"
DEFAULT_LANG_PATH = "plugins/*/romfs/lang/"
INVALID_TRANSLATION = ""


def main():
    parser = argparse.ArgumentParser(
        prog="langtool",
        description="ImHex translate tool",
    )
    parser.add_argument("command", choices=["check", "translate", "update", "create"])
    parser.add_argument(
        "-c", "--langdir", default=DEFAULT_LANG_PATH, help="Language folder glob"
    )
    parser.add_argument("-l", "--lang", default="", help="Language to translate")
    parser.add_argument(
        "-r", "--reflang", default="", help="Language for reference when translating"
    )
    args = parser.parse_args()

    command = args.command
    lang = args.lang

    print(f"Running in {command} mode")
    lang_files_glob = f"{lang}.json" if lang != "" else "*.json"

    lang_folders = set(Path(".").glob(args.langdir))
    if len(lang_folders) == 0:
        print(f"Error: {args.langdir} matches nothing")
        return 1

    for lang_folder in lang_folders:
        if not lang_folder.is_dir():
            print(f"Error: {lang_folder} is not a folder")
            return 1

        default_lang_data = {}
        default_lang_path = lang_folder / Path(DEFAULT_LANG + ".json")
        if not default_lang_path.exists():
            print(
                f"Error: Default language file {default_lang_path} does not exist in {lang_folder}"
            )
            return 1
        with default_lang_path.open("r", encoding="utf-8") as file:
            default_lang_data = json.load(file)

        reference_lang_data = None
        reference_lang_path = lang_folder / Path(args.reflang + ".json")
        if reference_lang_path.exists():
            with reference_lang_path.open("r", encoding="utf-8") as file:
                reference_lang_data = json.load(file)

        if command == "create" and lang != "":
            lang_file_path = lang_folder / Path(lang + ".json")
            if lang_file_path.exists():
                continue

            exist_lang_data = None
            for lang_folder1 in lang_folders:
                lang_file_path1 = lang_folder1 / Path(lang + ".json")
                if lang_file_path1.exists():
                    with lang_file_path1.open("r", encoding="utf-8") as file:
                        exist_lang_data = json.load(file)
                    break

            print(f"Creating new language file '{lang_file_path}'")

            with lang_file_path.open("w", encoding="utf-8") as new_lang_file:
                new_lang_data = {
                    "code": lang,
                    "language": (
                        exist_lang_data["language"]
                        if exist_lang_data
                        else input("Enter language name: ")
                    ),
                    "country": (
                        exist_lang_data["country"]
                        if exist_lang_data
                        else input("Enter country name: ")
                    ),
                    "translations": {},
                }
                json.dump(new_lang_data, new_lang_file, indent=4, ensure_ascii=False)

        lang_files = set(lang_folder.glob(lang_files_glob))
        if len(lang_files) == 0:
            print(f"Warn: Language file for '{lang}' does not exist in '{lang_folder}'")
        for lang_file_path in lang_files:
            if (
                lang_file_path.stem == f"{DEFAULT_LANG}.json"
                or lang_file_path.stem == f"{args.reflang}.json"
            ):
                continue

            print(f"\nProcessing '{lang_file_path}'\n----------------------------\n")

            with lang_file_path.open("r+", encoding="utf-8") as target_lang_file:
                lang_data = json.load(target_lang_file)

                for key, value in default_lang_data["translations"].items():
                    if (
                        key in lang_data["translations"]
                        and lang_data["translations"][key] != INVALID_TRANSLATION
                    ):
                        continue
                    if command == "check":
                        print(
                            f"Error: Translation {lang_data['code']} is missing translation for key '{key}'"
                        )
                        exit(2)
                    elif command == "translate":
                        print(f"Key \033[1m'{key}'\033[0m to {lang_data['language']}")
                        print(f"Text: \033[1m{value}\033[0m")
                        reference_tranlsation = (
                            reference_lang_data["translations"][key]
                            if reference_lang_data
                            else None
                        )
                        if reference_tranlsation:
                            print(f"Reference: \033[1m{reference_tranlsation}\033[0m")
                        new_value = input("Enter translation: ")
                        lang_data["translations"][key] = new_value
                    elif command == "update" or command == 'create':
                        lang_data["translations"][key] = INVALID_TRANSLATION

                keys_to_remove = []
                for key, value in lang_data["translations"].items():
                    if key not in default_lang_data["translations"]:
                        keys_to_remove.append(key)
                for key in keys_to_remove:
                    lang_data["translations"].pop(key)
                    print(
                        f"Removed unused key '{key}' from translation '{lang_data['code']}'"
                    )

                target_lang_file.seek(0)
                target_lang_file.truncate()
                json.dump(
                    lang_data,
                    target_lang_file,
                    indent=4,
                    sort_keys=True,
                    ensure_ascii=False,
                )


if __name__ == "__main__":
    exit(main())
