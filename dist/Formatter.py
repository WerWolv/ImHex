#!/usr/bin/env python3
import json
import os
import sys

print("\n=== 🌍 JSON Translation Synchronization (FLAT STRUCTURE) ===")
print("The folder must contain `en_US.json` and a target file (e.g., `pl_PL.json`).")

base_dir = os.getcwd()
src = os.path.join(base_dir, "en_US.json")

# Check if the source file exists
if not os.path.isfile(src):
    print(f"❌ Missing en_US.json in: {base_dir}")
    sys.exit(1)
print("✅ Found source file: en_US.json")

# Ask for the target language file
lang_code = input("📥 Enter target file code (e.g., pl_PL): ").strip()
dst = os.path.join(base_dir, f"{lang_code}.json")
if not os.path.isfile(dst):
    print(f"❌ Target file not found: {dst}")
    sys.exit(1)

def read_flat(path):
    """Read a JSON file and ensure it has a flat structure (dict at root)."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        if not isinstance(data, dict):
            raise ValueError("The JSON root must be a flat object (dict).")
        return data
    except json.JSONDecodeError as e:
        print(f"❌ JSON parse error in {os.path.basename(path)}: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"❌ {e}")
        sys.exit(1)

# Load source and target
en = read_flat(src)
lang = read_flat(dst)

ordered = {}
translated = empty = added = 0

# Reorder keys according to en_US.json
for k in en.keys():
    if k in lang:
        v = lang[k]
        if isinstance(v, str):
            if v.strip() == "":
                empty += 1
            else:
                translated += 1
        else:
            translated += 1  # treat non-string values as "translated"
    else:
        v = ""
        added += 1
    ordered[k] = v

# Keys that exist only in the target (obsolete)
removed = sorted(set(lang.keys()) - set(en.keys()))

# Create output directory
out_dir = os.path.join(base_dir, "output")
os.makedirs(out_dir, exist_ok=True)
out_path = os.path.join(out_dir, f"{lang_code}_ordered.json")

# Write result to ./output/<lang_code>_ordered.json
INDENT = 4  # number of spaces for JSON indentation
with open(out_path, "w", encoding="utf-8") as f:
    json.dump(ordered, f, ensure_ascii=False, indent=INDENT)
    f.write("\n")

# Final report
print("\n=== 🧾 TRANSLATION REPORT ===")
print(f"✅ Existing translations kept: {translated}")
print(f"❌ Empty translations: {empty}")
print(f"➕ Missing keys added: {added}")
print(f"🗑️ Obsolete keys (only in target): {len(removed)}")
print(f"📁 Output saved to: {out_path}")
