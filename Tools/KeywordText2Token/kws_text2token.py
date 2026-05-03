#!/usr/bin/env python3
"""Convert wake-word text to sherpa-onnx KWS keyword token lines.

This script targets the bundled
sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20 model. It mirrors the
official `phone+ppinyin` keyword format:

    L AY1 T AH1 P @LIGHT_UP
    n ǚ ér @女儿

English words are converted with the bundled `en.phone` lexicon. Chinese text
requires `pypinyin` so pinyin tone marks match the model tokens.
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path


LATIN_WORD_RE = re.compile(r"[A-Za-z']+")
CJK_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]")
SPLIT_RE = re.compile(r"[\u3400-\u4dbf\u4e00-\u9fff]+|[A-Za-z']+")
PINYIN_INITIALS = (
    "zh",
    "ch",
    "sh",
    "b",
    "p",
    "m",
    "f",
    "d",
    "t",
    "n",
    "l",
    "g",
    "k",
    "h",
    "j",
    "q",
    "x",
    "r",
    "z",
    "c",
    "s",
    "y",
    "w",
)


def default_model_dir() -> Path:
    return (
        Path(__file__).resolve().parents[2]
        / "Content"
        / "Models"
        / "sherpa-onnx-kws-zipformer-zh-en-3M-2025-12-20"
    )


def load_tokens(tokens_path: Path) -> set[str]:
    tokens: set[str] = set()
    with tokens_path.open("r", encoding="utf-8") as handle:
        for line in handle:
            parts = line.strip().split()
            if parts:
                tokens.add(parts[0])
    return tokens


def load_lexicon(lexicon_path: Path) -> dict[str, list[str]]:
    lexicon: dict[str, list[str]] = {}
    with lexicon_path.open("r", encoding="utf-8") as handle:
        for line in handle:
            parts = line.strip().split()
            if len(parts) >= 2:
                lexicon.setdefault(parts[0].upper(), parts[1:])
    return lexicon


def normalize_display(text: str) -> str:
    collapsed = re.sub(r"\s+", "_", text.strip())
    collapsed = re.sub(r"_+", "_", collapsed)
    return collapsed.upper() if LATIN_WORD_RE.fullmatch(text.replace(" ", "")) else collapsed


def parse_raw_line(line: str) -> tuple[str, str]:
    text, sep, display = line.partition("@")
    text = text.strip()
    display = display.strip() if sep else normalize_display(text)
    if not text:
        raise ValueError("empty keyword text")
    if not display:
        raise ValueError(f"empty display label for keyword: {line}")
    return text, display


def pinyin_to_tokens(syllable: str) -> list[str]:
    syllable = syllable.strip().lower()
    if not syllable:
        return []

    for initial in PINYIN_INITIALS:
        if syllable.startswith(initial) and len(syllable) > len(initial):
            return [initial, syllable[len(initial) :]]
    return [syllable]


def chinese_text_to_tokens(text: str) -> list[str]:
    try:
        from pypinyin import Style, lazy_pinyin
    except ImportError as exc:
        raise RuntimeError(
            "Chinese keyword conversion requires pypinyin. Install it with: "
            "python -m pip install pypinyin"
        ) from exc

    tokens: list[str] = []
    for syllable in lazy_pinyin(text, style=Style.TONE, errors="default"):
        tokens.extend(pinyin_to_tokens(syllable))
    return tokens


def english_word_to_tokens(word: str, lexicon: dict[str, list[str]]) -> list[str]:
    normalized = word.upper().strip("'")
    if normalized in lexicon:
        return lexicon[normalized]
    raise ValueError(f"English word is missing from en.phone lexicon: {word}")


def text_to_tokens(text: str, lexicon: dict[str, list[str]]) -> list[str]:
    tokens: list[str] = []
    consumed = "".join(SPLIT_RE.findall(text))
    if not consumed:
        raise ValueError(f"keyword contains no supported text: {text}")

    for part in SPLIT_RE.findall(text):
        if CJK_RE.search(part):
            tokens.extend(chinese_text_to_tokens(part))
        else:
            tokens.extend(english_word_to_tokens(part, lexicon))
    return tokens


def validate_keyword_tokens(keyword_tokens: list[str], known_tokens: set[str], text: str) -> None:
    missing = [token for token in keyword_tokens if token not in known_tokens]
    if missing:
        joined = ", ".join(dict.fromkeys(missing))
        raise ValueError(f"keyword '{text}' generated tokens not present in tokens.txt: {joined}")


def convert_lines(lines: list[str], model_dir: Path) -> list[str]:
    known_tokens = load_tokens(model_dir / "tokens.txt")
    lexicon = load_lexicon(model_dir / "en.phone")

    converted: list[str] = []
    for raw_line in lines:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        text, display = parse_raw_line(line)
        keyword_tokens = text_to_tokens(text, lexicon)
        validate_keyword_tokens(keyword_tokens, known_tokens, text)
        converted.append(f"{' '.join(keyword_tokens)} @{display}")

    return converted


def collect_lines(args: argparse.Namespace) -> list[str]:
    lines: list[str] = []
    if args.input:
        lines.extend(Path(args.input).read_text(encoding="utf-8").splitlines())
    lines.extend(args.text or [])
    if not lines:
        raise ValueError("provide --text or --input")
    return lines


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert plain wake-word text to sherpa-onnx KWS keywords.txt token lines."
    )
    parser.add_argument("-t", "--text", action="append", help="Keyword text. Can be repeated.")
    parser.add_argument("-i", "--input", help="UTF-8 text file. One keyword per line.")
    parser.add_argument("-o", "--output", help="Output keywords.txt path. Defaults to stdout.")
    parser.add_argument("--model-dir", default=str(default_model_dir()), help="Model directory containing tokens.txt and en.phone.")
    args = parser.parse_args()

    try:
        model_dir = Path(args.model_dir).resolve()
        converted = convert_lines(collect_lines(args), model_dir)
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    output = "\n".join(converted) + "\n"
    if args.output:
        Path(args.output).write_text(output, encoding="utf-8")
    else:
        sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
