#!/usr/bin/env python3
"""Generate a random integer list for median / benchmark experiments."""

from __future__ import annotations

import argparse
import array
import random
import struct
import sys
from pathlib import Path

BINARY_MAGIC = b"PSUC"
DEFAULT_CHUNK_SIZE = 1_000_000


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Generate a random integer list.")
    parser.add_argument(
        "-n",
        "--size",
        type=int,
        default=1_000_000,
        help="Number of elements (default: 1_000_000)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("results/random_list_1e6.txt"),
        help="Output file path (default: results/random_list_1e6.txt)",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="RNG seed for reproducible output (default: 42)",
    )
    parser.add_argument(
        "--min",
        type=int,
        default=0,
        dest="value_min",
        help="Minimum value inclusive (default: 0)",
    )
    parser.add_argument(
        "--max",
        type=int,
        default=10_000_000,
        dest="value_max",
        help="Maximum value inclusive (default: 10_000_000)",
    )
    parser.add_argument(
        "--binary",
        action="store_true",
        help="Write compact binary format (recommended for large lists)",
    )
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=DEFAULT_CHUNK_SIZE,
        help=f"Elements per generation chunk (default: {DEFAULT_CHUNK_SIZE})",
    )
    return parser.parse_args()


def validate_args(size: int, value_min: int, value_max: int, chunk_size: int) -> None:
    if size <= 0:
        raise ValueError("size must be positive")
    if value_min > value_max:
        raise ValueError("min must be <= max")
    if chunk_size <= 0:
        raise ValueError("chunk-size must be positive")


def generate_list(size: int, seed: int, value_min: int, value_max: int) -> list[int]:
    rng = random.Random(seed)
    return [rng.randint(value_min, value_max) for _ in range(size)]


def write_text_list(values: list[int], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as file:
        for value in values:
            file.write(f"{value}\n")


def write_binary_list(
    size: int,
    seed: int,
    value_min: int,
    value_max: int,
    output_path: Path,
    chunk_size: int,
) -> tuple[int, int]:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    rng = random.Random(seed)

    first_values: list[int] = []
    last_values: list[int] = []

    with output_path.open("wb") as file:
        file.write(BINARY_MAGIC)
        file.write(struct.pack("<Q", size))

        written = 0
        while written < size:
            current_chunk = min(chunk_size, size - written)
            chunk = array.array(
                "i",
                (rng.randint(value_min, value_max) for _ in range(current_chunk)),
            )
            file.write(chunk.tobytes())

            if written == 0:
                first_values = list(chunk[:5])
            if written + current_chunk >= size:
                last_values = list(chunk[-5:])
            elif current_chunk >= 5:
                pass

            written += current_chunk
            if written % 50_000_000 == 0 or written == size:
                print(f"  generated {written:,}/{size:,}", flush=True)

    if not last_values and size > 0:
        last_values = first_values[-5:]

    return first_values, last_values


def main() -> int:
    args = parse_args()

    try:
        validate_args(args.size, args.value_min, args.value_max, args.chunk_size)
        if args.binary:
            first_values, last_values = write_binary_list(
                args.size,
                args.seed,
                args.value_min,
                args.value_max,
                args.output,
                args.chunk_size,
            )
            print(f"Wrote {args.size:,} values to {args.output} (binary)")
            print(f"seed={args.seed}  range=[{args.value_min}, {args.value_max}]")
            print(f"first 5: {first_values}")
            print(f"last 5:  {last_values}")
        else:
            if args.size > 10_000_000:
                print(
                    "warning: text format is slow for large lists; use --binary",
                    file=sys.stderr,
                )
            values = generate_list(args.size, args.seed, args.value_min, args.value_max)
            write_text_list(values, args.output)
            print(f"Wrote {len(values):,} values to {args.output}")
            print(f"seed={args.seed}  range=[{args.value_min}, {args.value_max}]")
            print(f"first 5: {values[:5]}")
            print(f"last 5:  {values[-5:]}")
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
