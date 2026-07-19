#!/usr/bin/env python3
"""One-off generator: converts ColorMap/cmapdata.pas's 701 built-in
flam3-style gradients (a flat Pascal byte-array literal + a parallel name
array) into src/core/BuiltinGradients.cpp - a plain, mechanically-generated
C++ data file. Not part of the CMake build (no codegen step at build time,
matching this repo's existing convention for src/core/plugins/generated/*
- generated once, checked in as regular source). Re-run only if
cmapdata.pas itself is ever revisited.

Usage: python scripts/generate_builtin_gradients.py \
    <path to apophysis-7x-patched>/src/ColorMap/cmapdata.pas \
    src/core/BuiltinGradients.cpp
"""
import re
import sys

NUM_MAPS = 701
ENTRIES_PER_MAP = 256


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    src_path, out_path = sys.argv[1], sys.argv[2]

    text = open(src_path, "r", encoding="latin-1").read()

    cmaps_start = text.index("cmaps :")
    names_start = text.index("const CMapNames")
    cmaps_body = text[cmaps_start:names_start]

    # cmaps_body contains nothing but "// N name" comments and (r, g, b)
    # byte triples (verified: cmapdata.pas has exactly two top-level const
    # declarations, cmaps and CMapNames, so nothing else is interleaved) -
    # a flat regex over every triple is simpler and more robust than
    # tracking nested parens/line-wrapping by hand.
    triples = re.findall(r"\((\d+),\s*(\d+),\s*(\d+)\)", cmaps_body)
    expected = NUM_MAPS * ENTRIES_PER_MAP
    if len(triples) != expected:
        print(f"error: expected {expected} RGB triples, found {len(triples)}", file=sys.stderr)
        sys.exit(1)

    names_section = text[names_start:]
    names_section = names_section[: names_section.index(");")]
    names = re.findall(r"'([^']*)'", names_section)
    if len(names) != NUM_MAPS:
        print(f"error: expected {NUM_MAPS} names, found {len(names)}", file=sys.stderr)
        sys.exit(1)

    with open(out_path, "w", encoding="utf-8", newline="\n") as out:
        out.write(
            "// GENERATED FILE - do not hand-edit.\n"
            "// Produced by scripts/generate_builtin_gradients.py from the original\n"
            "// Apophysis 7X source's ColorMap/cmapdata.pas (701 flam3-style built-in\n"
            "// gradients, mechanically translated - see BuiltinGradients.h).\n\n"
            '#include "BuiltinGradients.h"\n\n'
            "#include <cstddef>\n"
            "#include <cstdint>\n\n"
            "namespace apo {\n\n"
            "namespace {\n\n"
        )

        out.write(f"constexpr int kBuiltinGradientCount = {NUM_MAPS};\n\n")

        out.write("// Flat R,G,B byte data, kBuiltinGradientCount * 256 entries.\n")
        out.write(f"constexpr std::uint8_t kBuiltinGradientData[{NUM_MAPS * ENTRIES_PER_MAP * 3}] = {{\n")
        flat = [b for triple in triples for b in triple]
        for i in range(0, len(flat), 30):
            out.write("    " + ",".join(flat[i : i + 30]) + ",\n")
        out.write("};\n\n")

        out.write(f"constexpr const char* kBuiltinGradientNames[{NUM_MAPS}] = {{\n")
        for name in names:
            escaped = name.replace("\\", "\\\\").replace('"', '\\"')
            out.write(f'    "{escaped}",\n')
        out.write("};\n\n")

        out.write(
            "} // namespace\n\n"
            "int builtinGradientCount() { return kBuiltinGradientCount; }\n\n"
            "const char* builtinGradientName(int index) { return kBuiltinGradientNames[index]; }\n\n"
            "ColorMap builtinGradient(int index) {\n"
            "    ColorMap cmap{};\n"
            "    const std::uint8_t* base = kBuiltinGradientData + static_cast<std::size_t>(index) * 256 * 3;\n"
            "    for (int i = 0; i < 256; ++i) {\n"
            "        cmap.entries[i][0] = base[i * 3 + 0];\n"
            "        cmap.entries[i][1] = base[i * 3 + 1];\n"
            "        cmap.entries[i][2] = base[i * 3 + 2];\n"
            "        cmap.entries[i][3] = 255;\n"
            "    }\n"
            "    return cmap;\n"
            "}\n\n"
            "} // namespace apo\n"
        )

    print(f"wrote {out_path}: {NUM_MAPS} gradients, {len(flat)} bytes of color data")


if __name__ == "__main__":
    main()
