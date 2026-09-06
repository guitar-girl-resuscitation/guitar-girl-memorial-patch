"""Fail-closed allocator corrections for the pinned Dobby dependency only."""
import argparse
import hashlib
from pathlib import Path

POSIX = "source/UserMode/UnifiedInterface/platform-posix.cc"
ARENA = "source/MemoryAllocator/NearMemoryArena.cc"
EXPECTED = {
    POSIX: "a9aff48ea6957efb5ab1cddfdce0391b6716a701fa21ff5919211edd48f7c903",
    ARENA: "e941059980a171096ddd9672c6355dcf96db7f50eeac141fa38dfda44a27d9a2",
}

def transform(name, text):
    if name == POSIX:
        old = "  if (address != NULL) {\n    flags = flags | MAP_FIXED;\n  }\n"
        assert text.count(old) == 1
        text = text.replace(old, "  // GGFM: an address is a hint, never permission to replace live mappings.\n")
        old = "  if (result == MAP_FAILED)\n    return nullptr;\n\n  return result;"
        assert text.count(old) == 1
        return text.replace(old, "  if (result == MAP_FAILED)\n    return nullptr;\n"
            "  if (address != NULL && result != address) {\n"
            "    munmap(result, size);\n    return nullptr;\n  }\n\n  return result;")
    assert name == ARENA
    old = "  blank_chunk_addr = search_near_blank_memory_chunk(position, alloc_range, alloc_size);"
    assert text.count(old) == 1
    return text.replace(old, "  // GGFM: never reuse bytes owned by another live mapping.\n"
        "  // Failure is propagated instead of writing a trampoline into foreign memory.")

def harden(root, apply=False):
    pending = []
    for name, expected in EXPECTED.items():
        path = root / name
        raw = path.read_bytes().replace(b"\r\n", b"\n")
        if hashlib.sha256(raw).hexdigest() != expected:
            raise ValueError(f"Unsupported or already modified Dobby source: {name}")
        pending.append((path, transform(name, raw.decode("utf-8")).encode("utf-8")))
    # Validate every input before writing any member. Original files remain as backups.
    for path, _ in pending:
        if path.with_suffix(path.suffix + ".ggfm-original").exists():
            raise ValueError(f"Refusing to overwrite backup: {path.name}")
        print(f"Dobby allocator correction: {path}")
    if apply:
        for path, replacement in pending:
            with path.with_suffix(path.suffix + ".ggfm-original").open("xb") as backup:
                backup.write(path.read_bytes())
            path.write_bytes(replacement)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    harden(args.root.resolve(), args.apply)
