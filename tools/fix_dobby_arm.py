"""Checked ARM A32 branch relocation correction for our pinned Dobby source."""
import argparse
import hashlib
from pathlib import Path

SOURCE = "source/InstructionRelocation/arm/ARMInstructionRelocation.cc"
EXPECTED = "cf4aef9c4930a70d1edbc8a95d75ec6254e1875cdc120ec057fc943d7c244438"
START = "  // Branch, branch with link, and block data transfer\n"
END = "  // if the instr do not needed relocate, just rewrite the origin\n"
REPLACEMENT = '''  // GGFM: A32 B/BL and immediate BLX. Preserve the original condition/link
  // bits and sign-extend the displacement before computing the absolute target.
  if ((static_cast<uint32_t>(instr) & 0x0e000000u) == 0x0a000000u) {
    const uint32_t word = static_cast<uint32_t>(instr);
    const int32_t displacement = static_cast<int32_t>((word & 0x00ffffffu) << 8) >> 6;
    const bool exchange = (word >> 28) == 15;
    uint32_t target = from_pc + displacement;
    if (exchange) target = (target + ((word >> 23) & 2u)) | 1u;
    // At +0: conditional B/BL +0 targets the LDR at +8, never its literal.
    // For BL the callee returns to +4; the unconditional B skips the literal.
    // A false condition also falls through to +4. Neither path changes flags.
    _ EmitARMInst(exchange ? 0xeb000000u : (word & 0xff000000u));
    _ b(4);
    _ ldr(pc, MemOperand(pc, -4));
    _ EmitAddress(target);
    is_instr_relocated = true;
  }

'''


def transform(text):
    arm, separator, thumb = text.partition("// relocate thumb-1 instructions")
    if not separator or arm.count(START) != 1 or arm.count(END) != 1:
        raise ValueError("ARM relocation boundaries mismatch")
    start, end = arm.index(START), arm.index(END)
    if start >= end:
        raise ValueError("ARM relocation boundaries reversed")
    return text[:start] + REPLACEMENT + text[end:]


def fix(root, apply=False):
    path = root / SOURCE
    original = path.read_bytes()
    normalized = original.replace(b"\r\n", b"\n")
    if hashlib.sha256(normalized).hexdigest() != EXPECTED:
        raise ValueError("Unsupported or already modified Dobby ARM relocation source")
    backup = path.with_suffix(path.suffix + ".ggfm-original")
    if backup.exists():
        raise ValueError("Refusing to overwrite Dobby ARM source backup")
    replacement = transform(normalized.decode("utf-8"))
    print(f"Dobby ARM relocation correction: {path}")
    if apply:
        with backup.open("xb") as stream:
            stream.write(original)
        path.write_text(replacement, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    fix(args.root.resolve(), args.apply)
