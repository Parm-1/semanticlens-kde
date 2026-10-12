#!/usr/bin/env python3
"""Score predeclared labels against distinct-file native top-five results."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def score(specification, result, page_applicable):
    expected = {q['id'] for q in specification['queries']}
    observed = [q['id'] for q in result['queries']]
    if set(observed) != expected or len(observed) != len(expected) or result.get('error'):
        raise ValueError('incomplete, duplicate or failed query set')
    by_id = {q['id']: q for q in result['queries']}
    rows = []
    for query in specification['queries']:
        raw = by_id[query['id']]
        if raw['query'] != query['text']:
            raise ValueError('typed query was changed')
        hits = raw['hits']
        names = [h['file'] for h in hits]
        if len(names) > 5 or len(set(names)) != len(names):
            raise ValueError('not distinct-file limit-five output')
        wanted = {r['file'] for r in query['relevant']}
        rank = next((i + 1 for i, name in enumerate(names) if name in wanted), None)
        page_hit = None
        if page_applicable and query['modality'] == 'pdf':
            page_hit = any(h['file'] == r['file'] and h.get('page') == r['page']
                           for h in hits for r in query['relevant'])
        rows.append({'id': query['id'], 'group': query['group'], 'modality': query['modality'],
                     'rank': rank, 'hit1': rank == 1, 'hit5': rank is not None,
                     'page_hit5': page_hit, 'returned': len(names),
                     'false_positive_scores': [h.get('score') for h in hits] if not wanted else []})

    def aggregate(group, modality=None):
        selected = [r for r in rows if r['group'] == group and
                    (modality is None or r['modality'] == modality)]
        return {'queries': len(selected), 'hit1': sum(r['hit1'] for r in selected),
                'hit5': sum(r['hit5'] for r in selected),
                'hit5_percent': 100 * sum(r['hit5'] for r in selected) / len(selected)}

    return {'queries': rows, 'semantic': aggregate('semantic'),
            'modalities': {m: aggregate('semantic', m) for m in ('document', 'pdf', 'image')},
            'exact': aggregate('exact'), 'literal': aggregate('literal'),
            'no_relevant': [r for r in rows if r['group'] == 'no_relevant']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--service', type=Path, required=True)
    parser.add_argument('--baloo', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('preserve prior results; choose a new output')
    specification = json.loads((ROOT / 'benchmark/queries.json').read_text())
    service_raw, baloo_raw = json.loads(args.service.read_text()), json.loads(args.baloo.read_text())
    inventory = hashlib.sha256((ROOT / 'benchmark/SHA256SUMS').read_bytes()).hexdigest()
    if (service_raw.get('benchmark_pins_sha256') != inventory or
            baloo_raw.get('corpus_inventory_sha256') != inventory):
        parser.error('results are not bound to the frozen corpus inventory')
    service = score(specification, service_raw, True)
    baloo = score(specification, baloo_raw, False)
    gain = service['semantic']['hit5_percent'] - baloo['semantic']['hit5_percent']
    report = {'scope': 'frozen authored synthetic v1; not a user population estimate',
              'benchmark_pins_sha256': inventory,
              'service_results_sha256': hashlib.sha256(args.service.read_bytes()).hexdigest(),
              'baloo_results_sha256': hashlib.sha256(args.baloo.read_bytes()).hexdigest(),
              'service': service, 'baloo': baloo, 'semantic_gain_percentage_points': gain,
              'release_retrieval_targets': {
                  'semantic_at_least_80_percent': service['semantic']['hit5_percent'] >= 80,
                  'gain_at_least_15_points': gain >= 15,
                  'all_unique_identifiers_hit5': service['exact']['hit5'] == service['exact']['queries']},
              'no_match_cutoff': 'none validated; returned no-relevant candidates are false positives'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('service', 'baloo')}, indent=2))


if __name__ == '__main__':
    main()
