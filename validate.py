"""
Automated validation for the iMX93 sorting simulator.
Compares events.csv against expected.csv with deep field checks.

Usage:
    python validate.py ../logs/events.csv expected.csv
"""
import csv
import sys
import os


def load_csv(path):
    rows = []
    with open(path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            rows.append(row)
    return rows


def validate(events_path, expected_path):
    if not os.path.exists(events_path):
        print(f"FAIL: Events file not found: {events_path}")
        return 1

    events = load_csv(events_path)
    expected = load_csv(expected_path)

    print(f"Loaded {len(events)} events, {len(expected)} expected rows\n")

    structural_failed = False

    # Check row count
    if len(events) != len(expected):
        print(f"FAIL: Row count mismatch — events={len(events)} expected={len(expected)}")
        structural_failed = True
    else:
        print(f"Row count OK: {len(events)}")

    # Check for duplicate images
    seen = set()
    for e in events:
        img = e.get('image', '')
        if img in seen:
            print(f"FAIL: Duplicate image in events: {img}")
            structural_failed = True
        seen.add(img)

    print("=" * 72)

    # Fields to compare (strict equality required)
    check_fields = ['final_package_id', 'id_status', 'target_zone',
                    'current_zone', 'sort_status', 'risk_level',
                    'quality_level', 'action']

    passed = 0
    failed = 0
    total = len(expected)

    for i, exp in enumerate(expected):
        img = exp.get('image', f'row_{i}')

        # Find matching event by image name
        ev = None
        for e in events:
            if e.get('image', '') == img:
                ev = e
                break

        if ev is None:
            print(f"  [{img}] MISSING in events.csv")
            failed += 1
            continue

        msgs = []
        for field in check_fields:
            exp_val = exp.get(field, '').strip()
            act_val = ev.get(field, '').strip()
            if exp_val and act_val != exp_val:
                msgs.append(f"{field}: expected={exp_val} actual={act_val}")

        if not msgs:
            act_pkg = ev.get('final_package_id', '')
            act_status = ev.get('id_status', '')
            act_risk = ev.get('risk_level', '')
            print(f"  [{img}] PASS: {act_pkg} / {act_status} / {act_risk}")
            passed += 1
        else:
            print(f"  [{img}] FAIL: {'; '.join(msgs)}")
            failed += 1

    print("=" * 72)
    print(f"\nResult: {passed}/{total} passed, {failed}/{total} failed")

    # Statistics
    print("\n--- Statistics from events.csv ---")
    qr_count = sum(1 for e in events if e.get('id_status', '') == 'QR_SUCCESS')
    ocr_rcv = sum(1 for e in events if e.get('id_status', '') == 'OCR_RECOVERED')
    ocr_cor = sum(1 for e in events if e.get('id_status', '') == 'OCR_CORRECTED')
    unknown = sum(1 for e in events if e.get('id_status', '') == 'UNKNOWN_PACKAGE')
    label_err = sum(1 for e in events if e.get('id_status', '') == 'LABEL_ERROR')
    wrong_sort = sum(1 for e in events if e.get('sort_status', '') == 'WRONG_SORT')
    high_risk = sum(1 for e in events if e.get('risk_level', '') in ('LEVEL_3_HIGH', 'LEVEL_4_CRITICAL'))
    pass_cnt = sum(1 for e in events if e.get('action', '') == 'PASS')
    pass_log_cnt = sum(1 for e in events if e.get('action', '') == 'PASS_WITH_LOG')
    review_cnt = sum(1 for e in events if e.get('action', '') == 'REVIEW')
    block_cnt = sum(1 for e in events if e.get('action', '') == 'BLOCK')
    qual_good = sum(1 for e in events if e.get('quality_level', '') == 'GOOD')
    qual_warn = sum(1 for e in events if e.get('quality_level', '') == 'WARNING')
    qual_bad = sum(1 for e in events if e.get('quality_level', '') == 'BAD')

    total_ev = len(events)
    total_rec = qr_count + ocr_rcv + ocr_cor
    print(f"  Total: {total_ev}")
    print(f"  QR Success: {qr_count}")
    print(f"  OCR Recovered: {ocr_rcv}")
    print(f"  OCR Corrected: {ocr_cor}")
    print(f"  Unknown: {unknown}")
    print(f"  Label Error: {label_err}")
    print(f"  Wrong Sort: {wrong_sort}")
    print(f"  High Risk: {high_risk}")
    if total_ev > 0:
        print(f"  Recognition Rate: {100.0 * total_rec / total_ev:.1f}%")
    print(f"\n--- V3 QualityGate stats ---")
    print(f"  Quality-GOOD: {qual_good}  WARNING: {qual_warn}  BAD: {qual_bad}")
    print(f"  Action-PASS: {pass_cnt}  PASS_WITH_LOG: {pass_log_cnt}  REVIEW: {review_cnt}  BLOCK: {block_cnt}")

    return 1 if failed > 0 or structural_failed else 0


if __name__ == '__main__':
    events_path = sys.argv[1] if len(sys.argv) > 1 else '../logs/events.csv'
    expected_path = sys.argv[2] if len(sys.argv) > 2 else 'expected.csv'
    sys.exit(validate(events_path, expected_path))
