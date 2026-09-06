"""Match audited methods across IL2CPP builds; output is evidence, not a release profile.

Input dumps stay local. Matches require both metadata name and normalized full
signature. No address scaling, nearest-name fallback or guessed overloads.
"""
import argparse
import collections
import json
from pathlib import Path
import re


def identity(method):
    # Il2CppDumper specializes MethodInfo type names with the method's address.
    signature = re.sub(r"\bMethodInfo_[0-9A-Fa-f]+\b", "MethodInfo", method["Signature"])
    return method["Name"], signature, method["TypeSignature"]


def audit(manifest, source, destination):
    by_address = collections.defaultdict(list)
    by_identity = collections.defaultdict(list)
    for method in source["ScriptMethod"]:
        by_address[method["Address"]].append(method)
    for method in destination["ScriptMethod"]:
        by_identity[identity(method)].append(method)
    rows = []
    for entry in manifest["il2cppHooks"] + manifest["il2cppDependencies"]:
        source_methods = by_address[int(entry["rva"], 0)]
        candidates = []
        if len(source_methods) == 1:
            candidates = by_identity[identity(source_methods[0])]
        rows.append({
            "name": entry["name"], "sourceRva": entry["rva"],
            "sourceCount": len(source_methods), "candidateCount": len(candidates),
            "destinationRva": hex(candidates[0]["Address"]) if len(candidates) == 1 else None,
            "status": "metadata-match-only" if len(candidates) == 1 else "unresolved",
        })
    return {"schema": 1, "releaseReady": False,
            "requiredReview": ["instruction-state", "function-boundaries", "calling-convention",
                               "object-layout", "device-execution"],
            "matched": sum(row["status"] == "metadata-match-only" for row in rows),
            "total": len(rows), "methods": rows}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("source_dump", type=Path)
    parser.add_argument("destination_dump", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(*(json.loads(path.read_text(encoding="utf-8")) for path in
                     (args.manifest, args.source_dump, args.destination_dump)))
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, indent=2)
        stream.write("\n")
    print(f"Matched {result['matched']}/{result['total']}; releaseReady=false")
    if result["matched"] != result["total"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
