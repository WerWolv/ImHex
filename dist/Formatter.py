#!/usr/bin/env python3
import json
import os

print("\n=== 🌍 JSON Translation Synchronization ===")
print("The directory must contain the source file `en_US.json` and a target translation file (e.g., `pl_PL.json`)")

# Current directory
base_dir = os.getcwd()

# Required source file
input_en = os.path.join(base_dir, "en_US.json")

# Check if en_US.json exists
if not os.path.isfile(input_en):
    print(f"❌ Source file `en_US.json` not found in: {base_dir}")
    exit(1)

print("✅ Found source file: en_US.json")

# Ask for target language code
lang_code = input("📥 Enter translation code (e.g., pl_PL, de_DE, fr_FR): ").strip()
input_lang = os.path.join(base_dir, f"{lang_code}.json")

# Check if target translation file exists
if not os.path.isfile(input_lang):
    print(f"❌ Translation file not found: {input_lang}")
    exit(1)

# Output directory
output_dir = os.path.join(base_dir, "output")
os.makedirs(output_dir, exist_ok=True)

output_file = os.path.join(output_dir, f"{lang_code}_ordered.json")

# Load JSON files
try:
    with open(input_en, "r", encoding="utf-8") as f:
        en_data = json.load(f)
except json.JSONDecodeError:
    print("❌ Failed to parse `en_US.json`. Invalid JSON syntax.")
    exit(1)

try:
    with open(input_lang, "r", encoding="utf-8") as f:
        lang_data = json.load(f)
except json.JSONDecodeError:
    print(f"❌ Failed to parse `{lang_code}.json`. Invalid JSON syntax.")
    exit(1)

en_translations = en_data.get("translations", {})
lang_translations = lang_data.get("translations", {})

ordered_translations = {}
translated = 0
empty = 0
added = 0

for key in en_translations:
    if key in lang_translations:
        value = lang_translations[key]
        if value.strip() == "":
            empty += 1
        else:
            translated += 1
    else:
        value = ""
        added += 1
    ordered_translations[key] = value

# Detect removed keys (present in target but missing in source)
removed_keys = set(lang_translations.keys()) - set(en_translations.keys())

# Final output structure
final_data = {
    "code": lang_data.get("code", lang_code),
    "language": lang_data.get("language", lang_code),
    "country": lang_data.get("country", "Unknown"),
    "fallback": lang_data.get("fallback", True),
    "translations": ordered_translations
}

# Write to output file
with open(output_file, "w", encoding="utf-8") as f:
    json.dump(final_data, f, ensure_ascii=False, indent=4)

# Summary
print("\n=== 🧾 TRANSLATION REPORT ===")
print(f"✅ Existing translations copied: {translated}")
print(f"❌ Empty translations: {empty}")
print(f"➕ Missing keys added: {added}")
print(f"🗑️ Obsolete keys removed: {len(removed_keys)}")
print(f"📁 Output saved to: {output_file}")

