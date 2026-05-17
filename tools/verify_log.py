#!/usr/bin/env python3
"""
V5 Hash Chain Verification Tool.

Verifies the integrity of hash_chain.csv. Detects tampering.
Usage:
    python verify_log.py ../logs/hash_chain.csv     # verify
    python verify_log.py ../logs/hash_chain.csv --show   # verify + display chain
"""
import csv, hashlib, sys, os


def sha256(data: str) -> str:
    return hashlib.sha256(data.encode()).hexdigest()


def load_chain(path):
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def serialize(rec):
    return (
        f"{rec.get('event_id','')}|{rec.get('timestamp','')}|{rec.get('image','')}|"
        f"{rec.get('final_package_id','')}|{rec.get('id_status','')}|"
        f"{rec.get('sort_status','')}|{rec.get('risk_level','')}|"
        f"{rec.get('quality_level','')}|{rec.get('confidence','')}|"
        f"{rec.get('rule_version','')}|{rec.get('model_version','')}|"
        f"{rec.get('prev_hash','')}"
    )


def verify(chain):
    events = []
    for i, rec in enumerate(chain):
        # Verify prev_hash chaining
        expected_prev = (
            "0000000000000000000000000000000000000000000000000000000000000000"
            if i == 0
            else events[-1]["computed_hash"]
        )
        actual_prev = rec.get("prev_hash", "").strip()
        chain_ok = actual_prev == expected_prev

        # Verify current_hash matches recomputed
        data = serialize(rec)
        computed = sha256(data)
        actual_current = rec.get("current_hash", "").strip()
        hash_ok = computed == actual_current

        events.append({
            "event_id": rec.get("event_id", str(i)),
            "chain_ok": chain_ok,
            "hash_ok": hash_ok,
            "computed_hash": computed,
            "expected_prev": expected_prev[:16] + "...",
            "actual_prev": actual_prev[:16] + "...",
            "expected_hash": computed[:16] + "...",
            "actual_hash": actual_current[:16] + "...",
        })
    return events


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "../logs/hash_chain.csv"
    show = "--show" in sys.argv

    if not os.path.exists(path):
        print(f"FAIL: File not found: {path}")
        return 1

    chain = load_chain(path)
    if not chain:
        print("FAIL: Empty chain")
        return 1

    events = verify(chain)

    all_ok = all(e["chain_ok"] and e["hash_ok"] for e in events)

    print("=" * 60)
    print(" Hash Chain Verification")
    print("=" * 60)
    print(f"Chain: {path}")
    print(f"Events: {len(events)}")
    print(f"Result: {'PASS' if all_ok else '*** HASH BROKEN ***'}")

    if show or not all_ok:
        print("-" * 60)
        for e in events:
            status = "OK" if (e["chain_ok"] and e["hash_ok"]) else "BROKEN"
            marker = "  " if status == "OK" else "!!"
            print(f"{marker} Event {e['event_id']}: {status}")
            if not e["chain_ok"]:
                print(f"    prev_hash mismatch!")
            if not e["hash_ok"]:
                print(f"    Data tampered! hash mismatch")

    # Summary
    broken = sum(1 for e in events if not (e["chain_ok"] and e["hash_ok"]))
    if broken > 0:
        print(f"\n*** WARNING: {broken}/{len(events)} events have been tampered! ***")
        return 1
    else:
        print("\nAll events verified. Hash chain is intact.")
        return 0


if __name__ == "__main__":
    sys.exit(main())
