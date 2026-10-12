#!/usr/bin/env python3
"""Explicit public-model download with pinned integrity checks; no file indexing."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

REVISION = "bfcd298762cc34d0357ece5ebdd31791a3a374d8"
FILES = [
    ("embeddinggemma-2-Q8_0.gguf", 309855456, "2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135"),
    ("mmproj-embeddinggemma-2-Q8_0.gguf", 554821024, "c4a8a52691ecef40618438928bdf9e68379b854e24166f292592353db0aab64f"),
]


def emit(**fields):
    print(json.dumps(fields), flush=True)


def verified(path, size, digest):
    if not path.is_file() or path.is_symlink() or path.stat().st_size != size:
        return False
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(8 << 20), b""):
            hasher.update(chunk)
    return hasher.hexdigest() == digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    destination = args.destination.expanduser().resolve()
    if not args.verify_only:
        destination.mkdir(parents=True, exist_ok=True, mode=0o700)
    completed = 0
    for name, size, digest in FILES:
        target = destination / name
        if verified(target, size, digest):
            completed += size
            emit(state="verified", file=name, received=completed, total=sum(f[1] for f in FILES))
            continue
        if args.verify_only:
            raise ValueError(f"Model integrity check failed: {name}")
        partial = target.with_suffix(".gguf.part")
        if target.is_symlink() or partial.is_symlink():
            raise ValueError(f"Refusing a model destination symlink: {name}")
        url = f"https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/{REVISION}/{name}"
        emit(state="downloading", file=name, received=completed, total=sum(f[1] for f in FILES))
        last = 0
        received = 0
        with urllib.request.urlopen(url, timeout=60) as response, partial.open("wb") as output:
            while chunk := response.read(1 << 20):
                received += len(chunk)
                if received > size:
                    raise ValueError(f"Download exceeded pinned size: {name}")
                output.write(chunk)
                if received - last >= 8 << 20:
                    emit(state="downloading", file=name, received=completed + received, total=sum(f[1] for f in FILES))
                    last = received
        if not verified(partial, size, digest):
            raise ValueError(f"Downloaded model integrity check failed: {name}")
        partial.replace(target)
        completed += size
        emit(state="verified", file=name, received=completed, total=sum(f[1] for f in FILES))
    emit(state="complete", received=completed, total=completed)


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        emit(state="error", error=str(error))
        raise SystemExit(1)
