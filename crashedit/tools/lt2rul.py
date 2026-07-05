#!/usr/bin/env python3
"""
lt2rul.py -- LanguageTool grammar.xml  ->  gramcheck .rul

Runs on the host (Linux/Mac/Windows), never on the target. Reads a
LanguageTool language pack's grammar.xml and emits the subset of rules
that map cleanly onto gramcheck's compact runtime.

What we DO extract:

  * <rule ...> blocks whose <pattern> is a single-token literal match
    followed by a <suggestion>literal</suggestion>, i.e. simple word
    confusions -> gramcheck PAIR entries.

  * <rule name="..."> with obvious punctuation-position patterns
    detected heuristically from tag id/name (uses id like
    "COMMA_WHITESPACE", "WHITESPACE_BEFORE_PUNCTUATION",
    "DOUBLE_PUNCTUATION" etc.). We do NOT try to run LT's regex
    engine; we just enable the corresponding gramcheck rule.

What we SKIP:

  * multi-token pattern rules
  * anything using postag=, chunk=, or exception blocks
  * disambiguator rules
  * everything with <phrases> refs

Usage:
    python3 lt2rul.py <path/to/grammar.xml> <lang_code>  > <lang>.rul

Example:
    python3 lt2rul.py \
        LanguageTool-6.0/org/languagetool/rules/en/grammar.xml en \
        > en.rul

Note on licensing:
    LanguageTool rules are LGPL. The .rul files produced from them
    inherit that license. Ship them alongside the LGPL notice. Do NOT
    mix them into the GPL-2+ source tree of gramcheck; keep them as
    data files.
"""

from __future__ import annotations

import sys
import re
import argparse
import xml.etree.ElementTree as ET


# ------------------------------------------------------------------
# ID-based mapping: rules we can flip on wholesale by their LT id.
# ------------------------------------------------------------------

ID_TO_DIRECTIVE = {
    # spacing / punctuation
    "COMMA_WHITESPACE":              ("PUNCT_SPACE_AFTER", ",;:"),
    "WHITESPACE_BEFORE_PUNCTUATION": ("PUNCT_NO_SPACE_BEFORE", ",.;:!?)"),
    "NO_SPACE_BEFORE_PUNCTUATION":   ("PUNCT_NO_SPACE_BEFORE", ",.;:!?)"),
    "DOUBLE_PUNCTUATION":            ("PUNCT_NO_DOUBLE", ".,;:!?"),
    "MULTIPLE_WHITESPACES":          ("SPACE_MULTIPLE", None),
    "WHITESPACE_RULE":               ("SPACE_MULTIPLE", None),
    "UPPERCASE_SENTENCE_START":      ("CAPITALIZE_SENTENCE", None),
    "SENTENCE_START":                ("CAPITALIZE_SENTENCE", None),
    "UNPAIRED_BRACKETS":             ("BRACKET_PAIRS", "()[]{}"),
    "FRENCH_WHITESPACE":             ("SPACE_BEFORE_QUOTE_FR", None),
}


# ------------------------------------------------------------------
# XML helpers
# ------------------------------------------------------------------

def get_local(tag: str) -> str:
    """Strip namespace prefix from an ElementTree tag."""
    return tag.split("}", 1)[-1] if "}" in tag else tag


def iter_children(elem, name: str):
    for c in elem:
        if get_local(c.tag) == name:
            yield c


def text_of(elem) -> str:
    return "".join(elem.itertext()).strip()


# ------------------------------------------------------------------
# Extraction of PAIR (simple confusion) rules
# ------------------------------------------------------------------

def extract_simple_pair(rule) -> tuple[str, str, str] | None:
    """
    Look for a <pattern> with exactly one <token> whose text is a plain
    literal word, and a <message><suggestion> whose text is also a plain
    literal word or a short phrase. Returns (src, dst, msg) or None.
    """
    pattern = None
    message = None
    for child in rule:
        n = get_local(child.tag)
        if n == "pattern":
            pattern = child
        elif n == "message":
            message = child

    if pattern is None or message is None:
        return None

    tokens = list(iter_children(pattern, "token"))
    if len(tokens) != 1:
        return None

    t = tokens[0]
    # skip if it has attributes we can't handle
    if any(k in t.attrib for k in ("postag", "chunk", "regexp", "skip", "min", "max")):
        return None
    if len(list(t)) > 0:  # inner elements (exception/etc)
        return None

    src = (t.text or "").strip()
    if not src or not re.match(r"^[A-Za-z\u00C0-\u017F']+(?:\s[A-Za-z\u00C0-\u017F']+)?$", src):
        return None

    # <suggestion> may have multiple, take the first plain-text one
    dst = None
    for sugg in iter_children(message, "suggestion"):
        st = text_of(sugg)
        # strip any <match .../> refs — if present, skip
        if list(iter_children(sugg, "match")):
            continue
        if st and re.match(r"^[A-Za-z0-9\u00C0-\u017F' \-]+$", st):
            dst = st
            break
    if not dst:
        return None

    # Compact message
    msg = text_of(message)
    msg = re.sub(r"\s+", " ", msg).strip()
    if len(msg) > 80:
        msg = msg[:77] + "..."

    return (src, dst, msg)


# ------------------------------------------------------------------
# Main
# ------------------------------------------------------------------

def parse_grammar(path: str):
    tree = ET.parse(path)
    root = tree.getroot()

    id_hits: dict[str, tuple[str, str | None]] = {}
    pairs: list[tuple[str, str, str]] = []
    seen_pairs: set[tuple[str, str]] = set()

    # LT rules live inside <category><rule id="...">...</rule></category>
    # and also inline <rule>. Walk everything.
    for rule in root.iter():
        if get_local(rule.tag) != "rule":
            continue

        rid = rule.attrib.get("id", "")
        if rid in ID_TO_DIRECTIVE:
            directive, data = ID_TO_DIRECTIVE[rid]
            id_hits[directive] = (directive, data)

        # try to derive a PAIR
        got = extract_simple_pair(rule)
        if got:
            src, dst, msg = got
            key = (src.lower(), dst.lower())
            if key not in seen_pairs:
                seen_pairs.add(key)
                pairs.append((src, dst, msg))

    return id_hits, pairs


def emit(id_hits, pairs, lang: str, out):
    print(f"# gramcheck rule pack -- generated from LanguageTool by lt2rul.py", file=out)
    print(f"# LanguageTool rules are LGPL; keep this file with its notice.", file=out)
    print(f"LANG   {lang}", file=out)
    print(f"NAME   {lang} (from LanguageTool)", file=out)
    print("", file=out)

    for directive, data in id_hits.values():
        if data:
            print(f"{directive} {data}", file=out)
        else:
            print(directive, file=out)

    print("", file=out)
    print(f"# {len(pairs)} PAIR entries", file=out)
    for src, dst, msg in sorted(pairs, key=lambda x: x[0].lower()):
        # dst may contain spaces -> quote it
        if " " in dst:
            dst_out = f'"{dst}"'
        else:
            dst_out = dst
        line = f"PAIR  {src.lower():<20s} {dst_out}"
        if msg:
            line += f"  info  # {msg}"
        print(line, file=out)


def main() -> int:
    ap = argparse.ArgumentParser(description="LanguageTool grammar.xml -> gramcheck .rul")
    ap.add_argument("grammar_xml")
    ap.add_argument("lang", help="ISO code, e.g. en, es, de, fr, pt, it")
    ap.add_argument("--out", "-o", default="-")
    ns = ap.parse_args()

    id_hits, pairs = parse_grammar(ns.grammar_xml)

    out = sys.stdout if ns.out == "-" else open(ns.out, "w", encoding="utf-8")
    try:
        emit(id_hits, pairs, ns.lang, out)
    finally:
        if out is not sys.stdout:
            out.close()

    print(f"lt2rul: {len(id_hits)} directives, {len(pairs)} pairs", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
