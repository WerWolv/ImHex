import argparse
from fontTools.ttLib import TTFont
import os

def unicode_to_utf8_escape(codepoint):
    return ''.join([f'\\x{b:02x}' for b in chr(codepoint).encode('utf-8')])

def format_macro_name(prefix, glyph_name):
    # Convert names like 'repo-forked' -> 'ICON_VS_REPO_FORKED'
    return "ICON_" + prefix + "_" + glyph_name.upper().replace('-', '_')

def generate_font_header(font_path, output_path, font_macro_name, font_file_macro):
    font = TTFont(font_path)

    # Use cmap to get Unicode to glyph mapping
    codepoint_to_names = {}
    for table in font["cmap"].tables:
        if table.isUnicode():
            for codepoint, glyph_name in table.cmap.items():
                codepoint_to_names.setdefault(codepoint, []).append(glyph_name)

    if not codepoint_to_names:
        print("No Unicode-mapped glyphs found in the font.")
        return

    # Remove any glyph that is lower than 0xFF
    codepoint_to_names = {cp: names for cp, names in codepoint_to_names.items() if cp >= 0xFF}

    min_cp = min(codepoint_to_names)
    max_cp = max(codepoint_to_names)

    with open(output_path, "w", encoding="utf-8") as out:
        out.write("#pragma once\n\n")
        out.write(f'#define FONT_ICON_FILE_NAME_{font_macro_name} "{font_file_macro}"\n\n')
        out.write(f"#define ICON_MIN_{font_macro_name} 0x{min_cp:04x}\n")
        out.write(f"#define ICON_MAX_16_{font_macro_name} 0x{max_cp:04x}\n")
        out.write(f"#define ICON_MAX_{font_macro_name} 0x{max_cp:04x}\n")

        written = set()
        for codepoint in sorted(codepoint_to_names):
            utf8 = unicode_to_utf8_escape(codepoint)
            comment = f"// U+{codepoint:04X}"
            glyph_names = sorted(set(codepoint_to_names[codepoint]))
            for i, glyph_name in enumerate(glyph_names):
                macro = format_macro_name(font_macro_name, glyph_name)
                if macro in written:
                    continue
                out.write(f"#define {macro} \"{utf8}\"\t{comment}\n")
                written.add(macro)

    print(f"Header generated at {output_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate C header file from TTF glyphs.")
    parser.add_argument("font", help="Input .ttf font file")
    parser.add_argument("output", help="Output .h file")
    parser.add_argument("macro_name", help="Macro prefix")
    args = parser.parse_args()

    generate_font_header(args.font, args.output, args.macro_name, os.path.basename(args.font))
