#!/usr/bin/env python3
"""Offline independent arithmetic/source association, no model or Baloo process.

Run: python recompute.py --output /new/path/results.json
Optional --repo observes closed synthetic runtime files read-only; not a rerun.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3

BASE = Path(__file__).resolve().parent
ORIG = BASE / 'originals'

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def load(p):
    return json.loads(p.read_text())

def verify(manifest, base):
    entries = []
    for line in manifest.read_text().splitlines():
        expected, name = line.split(maxsplit=1)
        assert digest(base / name) == expected, name
        entries.append(name)
    return len(entries)

def words(s):
    return ' '.join(s.split())

def analyze(spec, raw, service, pages):
    ids = [q['id'] for q in raw['queries']]
    expected_ids = [q['id'] for q in spec]
    assert len(ids) == len(set(ids)) == len(spec)
    assert set(ids) == set(expected_ids) and not raw.get('error')
    observed = {q['id']: q for q in raw['queries']}
    rows = []
    association_count = 0
    formula_error = 0
    for q in spec:
        r = observed[q['id']]
        assert r['query'] == q['text']
        names = [h['file'] for h in r['hits']]
        assert len(names) <= 5 and len(names) == len(set(names))
        assert all((ORIG/'benchmark/corpus'/n).is_file() for n in names)
        wanted = {v['file']:v['page'] for v in q['relevant']}
        positions = [i+1 for i,n in enumerate(names) if n in wanted]
        first = positions[0] if positions else None
        row = {'id':q['id'], 'group':q['group'], 'modality':q['modality'],
               'target_rank':first, 'hit1':first == 1, 'hit5':first is not None,
               'files':names, 'false_positives':len(names) if not wanted else None}
        if service:
            row['target_page_correct'] = any(h['file'] in wanted and h['page']==wanted[h['file']] for h in r['hits']) if q['modality']=='pdf' else None
            assert r['index_revision'] == raw['indexing']['status']['index_revision']
            keys = []
            for h in r['hits']:
                assert h['path'] == h['file']
                literal = h['exact']; terms = h['term_match']; dot = h['semantic_score']
                assert type(literal) is bool and type(terms) is bool and math.isfinite(dot)
                assert h['match_type'] == ('literal' if literal else 'terms' if terms else 'semantic')
                fraction = max(0,min(1,(dot+1)/2))
                score = 1 if literal else .90+.09*fraction if terms else .89*fraction
                formula_error = max(formula_error,abs(score-h['score']))
                assert abs(score-h['score']) < 1e-12
                keys.append((-int(literal),-int(terms),-dot,h['file']))
                p = ORIG/'benchmark/corpus'/h['file']
                expected_kind = {'.txt':'text','.pdf':'pdf','.png':'image'}[p.suffix]
                assert h['kind'] == expected_kind
                if p.suffix == '.txt':
                    assert h['page'] == 0 and words(h['text']) in words(p.read_text())
                    association_count += 1
                elif p.suffix == '.pdf':
                    assert words(h['text']) in words(pages[p.name][str(h['page'])])
                    association_count += 1
                else:
                    assert h['page'] == 0
                if literal:
                    assert q['text'].lower() in h['text'].lower() or q['text'].lower() in h['title'].lower()
                elif q['group']=='no_relevant':
                    assert not literal and not terms and h['match_scope']=='segment'
            assert keys == sorted(keys)
            if not wanted:
                row['raw_negative_fields'] = [{k:h[k] for k in ('file','page','score','semantic_score','exact','term_match','match_type')} for h in r['hits']]
        rows.append(row)
    buckets = {}
    for group,modalities in [('semantic',[None,'document','pdf','image']),('exact',[None]),('literal',[None])]:
        for modality in modalities:
            chosen = [r for r in rows if r['group']==group and (modality is None or r['modality']==modality)]
            buckets[group+(':'+modality if modality else '')] = {'n':len(chosen),'hit1':sum(r['hit1'] for r in chosen),'hit5':sum(r['hit5'] for r in chosen)}
    return {'aggregate':buckets,'rows':rows,'source_text_pdf_associations':association_count,'max_score_formula_error':formula_error}

def measured(raw, service):
    w=raw['warm_repeats']
    assert len(w)==185
    assert Counter((r['repeat'],r['id']) for r in w)==Counter((i,q['id']) for i in range(5) for q in raw['queries'])
    assert all(math.isfinite(r['elapsed_ms']) and r['elapsed_ms']>0 for r in w)
    if service:
        for r in raw['queries']+w+raw['cold_queries']:
            assert r['end_ns'] > r['begin_ns']
            assert abs((r['end_ns']-r['begin_ns'])/1e6-r['elapsed_ms']) < 1e-9
    ordered=sorted(r['elapsed_ms'] for r in w)
    p95=ordered[math.ceil(.95*len(w))-1]
    assert p95==raw['warm_p95_ms']
    index=raw['indexing']
    assert (index['end_ns']-index['begin_ns'])/1e9==index['elapsed_seconds']
    result={'warm_n':len(w),'p95_order_statistic':math.ceil(.95*len(w)),'warm_p95_ms':p95,'indexing_seconds':index['elapsed_seconds'],
            'reported_sampled_peak_rss_kib':raw['sampled_peak_rss_kib'],'reported_endpoint_apparent_index_bytes':raw['index_size_bytes']}
    if service:
        result['cold']=[{k:r[k] for k in ('id','begin_ns','end_ns','elapsed_ms','producer_absent_before')} for r in raw['cold_queries']]
        assert [r['id'] for r in raw['cold_queries']]==['s01','s09','s13']
        assert all(r['producer_absent_before'] for r in raw['cold_queries'])
        result['closure']=raw['closure']
    else:
        result['closure']={k:raw[k] for k in ('daemon_closed','daemon_exit_code','sandbox_closed','sandbox_exit_code')}
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--repo',type=Path)
    args=parser.parse_args()
    assert not args.output.exists(), 'choose new output; preserve frozen receipts'
    candidate=ORIG/'evidence/retrieval-v1'
    custody={'candidate_report_sha256':digest(candidate/'report.md'),'candidate_manifest_sha256':digest(candidate/'pins.sha256'),
             'benchmark_manifest_sha256':digest(ORIG/'benchmark/SHA256SUMS'),
             'candidate_pin_count':verify(candidate/'pins.sha256',candidate),'corpus_input_pin_count':verify(ORIG/'benchmark/SHA256SUMS',ORIG)}
    assert custody['candidate_report_sha256']=='11d6cc5454637345979c36d7d345e3d4f10f303a3f003efb9950914c0aaf8e73'
    assert custody['candidate_manifest_sha256']=='94b1bc5e635d64e7cfbe19257d8d055378c105775d8a85f3d291a0406b4e6ec8'
    assert custody['benchmark_manifest_sha256']=='2fdb07dba3b3191a7073b7fa1443e58c542fa3c03e3275d429e54058c8996872'
    spec=load(ORIG/'benchmark/queries.json')['queries'];s=load(candidate/'service/results.json');b=load(candidate/'baloo/results.json')
    assert s['benchmark_pins_sha256']==b['corpus_inventory_sha256']==custody['benchmark_manifest_sha256']
    assert s['service_sha256']=='717f97ce74e8c74e45821e6271e58ecb457b281acdb8a5815136d7dbf73e6eb1'
    assert s['probe_sha256']=='f34469aa625f5000a7a3bb24a04b3c0cc91cf63ddb4e4a17fdbbf0fa1f49a9bf'
    pages=load(BASE/'actual-pdf-pages.json')
    result={'custody':custody,'service':analyze(spec,s,True,pages),'baloo':analyze(spec,b,False,pages),
            'service_measurements':measured(s,True),'baloo_measurements':measured(b,False)}
    files={p.name for p in (ORIG/'benchmark/corpus').iterdir()}
    assert len(files)==28 and {r['file'] for r in b['content_status']}==files
    assert len(b['content_status'])==28 and all(r['status']=='done' and r['indexing']=='content' for r in b['content_status'])
    terms={}
    for name in files:
        t=re.search(r'^Plain Text Terms:(.*)$',b['extracted_terms'][name],re.M)
        assert t
        terms[name]=t.group(1).split()
        assert bool(terms[name])==(not name.endswith('.png'))
    result['baloo_content_receipts']={'content_done':28,'text_pdf_nonempty_plain_terms':20,'png_empty_plain_terms':8}
    result['semantic_gain_percentage_points']=100*(result['service']['aggregate']['semantic']['hit5']-result['baloo']['aggregate']['semantic']['hit5'])/20
    result['false_positive_score_range']=[min(h['score'] for r in s['queries'][-3:] for h in r['hits']),max(h['score'] for r in s['queries'][-3:] for h in r['hits'])]
    baseline=load(ORIG/'evidence/baloo-baseline-v1/results.json')
    raw_controls={r['query']:[h['file'] for h in r['hits']] for r in baseline['queries']}
    assert raw_controls['Domestic permission']==['alpha-record.txt'] and raw_controls['Domestic telescope']==[]
    assert set(raw_controls['Domestic OR telescope'])=={'alpha-record.txt','beta-record.txt'}
    assert raw_controls['"prior permission"']==['alpha-record.txt'] and not raw_controls['"permission prior"']
    assert raw_controls['content:permission']==['alpha-record.txt'] and not raw_controls['keeping a cat']
    result['baseline_raw_query_controls']=raw_controls
    if args.repo:
        repo=args.repo.resolve();post={}
        originals={p.name:digest(p) for p in (ORIG/'benchmark/corpus').iterdir()}
        for rel in ['local/retrieval-service-v1/corpus','local/baloo-retrieval-ik6jqt8u/home/corpus']:
            actual={p.name:digest(p) for p in (repo/rel).iterdir()}
            assert actual==originals
            post[rel]={'files':len(actual),'frozen_corpus_byte_equal':True}
        db=repo/'local/retrieval-service-v1/data/index.sqlite'
        con=sqlite3.connect(db.as_uri()+'?mode=ro&immutable=1',uri=True);con.row_factory=sqlite3.Row
        sql={}
        for table in ['meta','files','pages','segments']:
            sql[table]=[]
            for row in con.execute('select * from '+table):
                r=dict(row)
                for k in ['path','root']:
                    if k in r:r[k]=Path(r[k]).name if k=='path' else '<owned-synthetic-corpus>'
                for k,v in list(r.items()):
                    if isinstance(v,bytes):
                        if k=='vector':r[k]={'bytes':len(v),'sha256':hashlib.sha256(v).hexdigest()}
                        else:r[k]=v.decode()
                sql[table].append(r)
        con.close()
        assert len(sql['files'])==28 and len(sql['segments'])==38
        assert all(not r['error'] for r in sql['files'])
        assert all(r['vector']['bytes']==768*4 for r in sql['segments'])
        post['post_shutdown_sql']={'scope':'readonly closed-runtime corroboration; not original endpoint or new native retrieval','sha256':digest(db),'bytes':db.stat().st_size,'rows':sql}
        post['known_recorded_service_pids_present_now']=[pid for pid in s['known_pids'] if Path(f'/proc/{pid}').exists()]
        assert not post['known_recorded_service_pids_present_now']
        post['declared_service_socket_present_now']=Path(f"/tmp/kde-index-retrieval-{s['known_pids'][0]}").exists()
        assert not post['declared_service_socket_present_now']
        result['post_shutdown_corroboration']=post
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k in ('custody','service_measurements','baloo_measurements','semantic_gain_percentage_points','baloo_content_receipts','false_positive_score_range')},indent=2))

if __name__=='__main__':main()
