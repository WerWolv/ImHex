from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._c_m_a_p import CmapSubtable
import argparse

# Default PUAs
SOURCE_PUA_START = 0xEA00
SOURCE_PUA_END = 0x100F2
TARGET_PUA_START = 0xF0000

def move_pua_glyphs(input_font_path, output_font_path):
    font = TTFont(input_font_path)

    cmap_table = font['cmap']
    glyph_set = font.getGlyphSet()

    # Track moved glyphs
    moved = 0
    new_mapping = {}

    # Collect original mappings in the PUA
    for cmap in cmap_table.tables:
        if cmap.isUnicode():
            for codepoint, glyph_name in cmap.cmap.items():
                if SOURCE_PUA_START <= codepoint <= SOURCE_PUA_END:
                    offset = codepoint - SOURCE_PUA_START
                    new_codepoint = TARGET_PUA_START + offset
                    new_mapping[new_codepoint] = glyph_name
                    moved += 1

    if moved == 0:
        print("No glyphs found in the source Private Use Area.")
        return

    # Remove old PUA entries from existing cmap subtables
    for cmap in cmap_table.tables:
        if cmap.isUnicode():
            cmap.cmap = {
                cp: gn for cp, gn in cmap.cmap.items()
                if not (SOURCE_PUA_START <= cp <= SOURCE_PUA_END)
            }

    # Create or update a format 12 cmap subtable
    found_format12 = False
    for cmap in cmap_table.tables:
        if cmap.format == 12 and cmap.platformID == 3 and cmap.platEncID in (10, 1):
            cmap.cmap.update(new_mapping)
            found_format12 = True
            break

    if not found_format12:
        # Create a new format 12 subtable
        cmap12 = CmapSubtable.newSubtable(12)
        cmap12.platformID = 3
        cmap12.platEncID = 10  # UCS-4
        cmap12.language = 0
        cmap12.cmap = new_mapping
        cmap_table.tables.append(cmap12)

    print(f"Moved {moved} glyphs from U+{SOURCE_PUA_START:X}–U+{SOURCE_PUA_END:X} to U+{TARGET_PUA_START:X}+")
    font.save(output_font_path)
    print(f"Saved modified font to {output_font_path}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Move PUA glyphs in a TTF file to another Unicode range.")
    parser.add_argument("input", help="Input TTF file path")
    parser.add_argument("output", help="Output TTF file path")

    args = parser.parse_args()
    move_pua_glyphs(args.input, args.output)
