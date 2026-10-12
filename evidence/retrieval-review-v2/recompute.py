#!/usr/bin/env python3
"""Independent offline review; reads frozen evidence and closed synthetic DB only."""
import hashlib
import json
import math
from pathlib import Path
import re
import sqlite3
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1])
assert not OUT.exists(), 'use a fresh output'
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):
    return json.loads((ROOT / p).read_text())
def verify(p, base):
    entries = []
    for line in p.read_text().splitlines():
        expected, name = line.split(maxsplit=1)
        assert sha(base / name) == expected, name
        entries.append(name)
    return len(entries)

ev = ROOT / 'evidence/retrieval-v2'
bootstrap = ROOT / 'local/clean-arch-v1/root.x86_64/public-clean-v2'
pins = {'delivery': verify(ev/'pins.sha256', ev),
        'benchmark': verify(ROOT/'benchmark/SHA256SUMS', ROOT)}
candidate = []
for line in (ev/'candidate-pins.sha256').read_text().splitlines():
    expected, name = line.split(maxsplit=1)
    if name.startswith('/public-clean-v2/workspace/'):
        actual = ROOT / name.removeprefix('/public-clean-v2/workspace/')
    elif name.startswith('/public-clean-v2/'):
        actual = bootstrap / name.removeprefix('/public-clean-v2/')
    elif name.startswith('/models/'):
        actual = ROOT / name.lstrip('/')
    else:
        raise ValueError(name)
    assert sha(actual) == expected, name
    candidate.append({'recorded_path': name, 'sha256': expected})
pins['candidate_inputs'] = len(candidate)
for line in (ROOT/'evidence/public-clean-build-v2/candidate-source-hashes.txt').read_text().splitlines():
    expected, name = line.split(maxsplit=1)
    assert sha(ROOT/name) == expected and sha(bootstrap/'source/kde-index-source'/name) == expected
corpus = ROOT/'benchmark/corpus'
files = {p.name: sha(p) for p in corpus.iterdir()}
assert len(files) == 28
assert {p.name:sha(p) for p in (ev/'service/corpus').iterdir()} == files
spec = read('benchmark/queries.json')['queries']
service = read('evidence/retrieval-v2/service/results.json')
baloo = read('evidence/retrieval-v1/baloo/results.json')
assert service['service_sha256'] == candidate[0]['sha256']
assert service['probe_sha256'] == candidate[1]['sha256']
assert service['candidate_pins_sha256'] == sha(ev/'candidate-pins.sha256')
assert service['benchmark_pins_sha256'] == baloo['corpus_inventory_sha256'] == sha(ROOT/'benchmark/SHA256SUMS')
db = ev/'service/data/index.sqlite'
con = sqlite3.connect(db.as_uri()+'?mode=ro&immutable=1', uri=True)
con.row_factory = sqlite3.Row
page_rows = [dict(r) for r in con.execute('SELECT * FROM pages')]
pages = {(Path(r['path']).name,r['page']):r for r in page_rows}
assert len(pages) == len(page_rows) == 32
db_identity = json.loads(con.execute("SELECT value FROM meta WHERE key='identity'").fetchone()[0])
status = service['indexing']['status']
assert db_identity == status['identity']
assert status['indexed'] == 28 and status['segments'] == 38
assert not status['busy'] and not status['error'] and not status['errors'] and not status['failures']
assert con.execute('SELECT COUNT(*) FROM files').fetchone()[0] == 28
assert con.execute("SELECT COUNT(*) FROM files WHERE error != ''").fetchone()[0] == 0
assert con.execute('SELECT COUNT(*) FROM segments WHERE length(vector)=3072').fetchone()[0] == 38
pdf_pages = {}
for p in sorted(corpus.glob('*.pdf')):
    pdf_pages[p.name] = {}
    for page in (1,2):
        text = subprocess.check_output(['pdftotext','-f',str(page),'-l',str(page),str(p),'-'],text=True)
        pdf_pages[p.name][str(page)] = text
        assert ' '.join(text.split()) == ' '.join(pages[(p.name,page)]['text'].split())
for p in corpus.glob('*.txt'):
    assert p.read_text() == pages[(p.name,0)]['text']

def counts(rows):
    return {key: {'n':len(sel),'first':sum(r['rank']==1 for r in sel),'top5':sum(r['rank'] is not None for r in sel)}
            for key,sel in [(g,[r for r in rows if r['group']==g]) for g in ('semantic','exact','literal')]
            + [('semantic:'+m,[r for r in rows if r['group']=='semantic' and r['modality']==m]) for m in ('document','pdf','image')]}
def analyze(raw, native):
    assert not raw.get('error')
    assert [r['id'] for r in raw['queries']] == [q['id'] for q in spec]
    assert len(set(r['id'] for r in raw['queries'])) == 37
    result=[]; fields={'literal':0,'terms':0,'semantic':0}; associations=0
    for q,r in zip(spec,raw['queries']):
        assert q['text'] == r['query']
        names=[h['file'] for h in r['hits']]
        assert len(names)<=5 and len(names)==len(set(names)) and set(names)<=set(files)
        wanted={h['file']:h['page'] for h in q['relevant']}
        ranks=[i+1 for i,n in enumerate(names) if n in wanted]
        row={'id':q['id'],'group':q['group'],'modality':q['modality'],'rank':min(ranks) if ranks else None,
             'files':names,'returned':len(names)}
        if native:
            row['correct_page'] = any(h['file'] in wanted and h['page']==wanted[h['file']] for h in r['hits']) if q['modality']=='pdf' else None
            tokens=['"'+t.replace('"','""')+'"' for t in q['text'].split()]
            fts_ids={r[0] for r in con.execute('SELECT rowid FROM terms WHERE terms MATCH ?',(' AND '.join(tokens),))}
            keys=[]
            for h in r['hits']:
                assert h['file'] == h['path'] and h['title'] == h['file']
                p=pages[(h['file'],h['page'])]
                assert h['kind']==p['kind']
                assert ' '.join(h['text'].split()) in ' '.join(p['text'].split())
                associations += 1
                literal=q['text'].casefold() in p['text'].casefold() or q['text'].casefold() in p['title'].casefold()
                terms=p['id'] in fts_ids
                assert h['exact'] is literal and h['term_match'] is terms
                expected_type='literal' if literal else 'terms' if terms else 'semantic'
                assert h['match_type']==expected_type
                expected_scope='text' if q['text'].casefold() in p['text'].casefold() else 'title' if literal else 'page' if terms else 'segment'
                assert h['match_scope']==expected_scope
                dot=h['semantic_score']; fraction=max(0,min(1,(dot+1)/2))
                score=1.0 if literal else .90+.09*fraction if terms else .89*fraction
                assert math.isfinite(dot) and math.isfinite(h['score']) and abs(h['score']-score)<1e-12
                keys.append((-int(literal),-int(terms),-dot,h['file']))
                fields[expected_type]+=1
            assert keys==sorted(keys)
            assert r['index_revision']==status['index_revision']==32
            if not wanted:
                row['contradictory_no_match_fields']=r['hits']
        result.append(row)
    return {'counts':counts(result),'rows':result,'source_page_associations':associations,'tier_rows':fields}
def times(raw,native):
    warm=raw['warm_repeats']
    assert [(r['repeat'],r['id']) for r in warm]==[(repeat,q['id']) for repeat in range(5) for q in spec]
    assert all(math.isfinite(r['elapsed_ms']) and r['elapsed_ms']>0 for r in warm)
    if native:
        all_rows=raw['queries']+warm+raw['cold_queries']
        assert len(all_rows)==225
        for r in all_rows:
            assert r['end_ns']>r['begin_ns']
            assert (r['end_ns']-r['begin_ns'])/1e6==r['elapsed_ms']
        assert [r['id'] for r in raw['cold_queries']]==['s01','s09','s13']
        assert all(r['producer_absent_before'] for r in raw['cold_queries'])
        assert all(r['query']==next(q['text'] for q in spec if q['id']==r['id']) for r in raw['cold_queries'])
    index=raw['indexing']
    assert (index['end_ns']-index['begin_ns'])/1e9==index['elapsed_seconds']
    p95=sorted(r['elapsed_ms'] for r in warm)[175]
    assert p95==raw['warm_p95_ms']
    return {'warm_count':len(warm),'nearest_rank_ordinal':176,'p95_ms':p95,'index_seconds':index['elapsed_seconds'],
            'reported_sampled_RSS_KiB':raw['sampled_peak_rss_kib'],'reported_pre_shutdown_index_bytes':raw['index_size_bytes'],
            'cold':[{k:r[k] for k in ('id','begin_ns','end_ns','elapsed_ms','producer_absent_before')} for r in raw.get('cold_queries',[])]}
result={'identities':{'delivery_manifest':sha(ev/'pins.sha256'),'candidate_report':sha(ev/'report.md'),'service_raw':sha(ev/'service/results.json'),
                     'Baloo_raw':sha(ROOT/'evidence/retrieval-v1/baloo/results.json'),'candidate_inputs':candidate,'pin_counts':pins,
                     'runtime_identity':db_identity},
        'service':analyze(service,True),'Baloo':analyze(baloo,False),'service_times':times(service,True),'Baloo_times':times(baloo,False),
        'post_shutdown_database':{'sha256':sha(db),'bytes':db.stat().st_size,'files':28,'pages':32,'vectors':38,'vector_bytes_each':3072,
                                  'scope':'closed read-only corroboration; different storage endpoint from reported pre-shutdown DB/WAL sum'},
        'actual_pdf_pages':pdf_pages,'inner_namespace_closure':service['closure']}
assert service['closure']=={'service_exit':0,'all_known_pids_absent':True,'socket_absent':True}
assert baloo['daemon_closed'] and baloo['daemon_exit_code']==-15 and baloo['sandbox_closed'] and baloo['sandbox_exit_code']==0
assert len(baloo['content_status'])==28 and {r['file'] for r in baloo['content_status']}==set(files)
assert all(r['status']=='done' and r['indexing']=='content' for r in baloo['content_status'])
term_counts={'text_pdf_nonempty':0,'PNG_empty':0}
for f in files:
    plain=re.search(r'^Plain Text Terms:(.*)$',baloo['extracted_terms'][f],re.M).group(1).strip()
    assert bool(plain)==(not f.endswith('.png'))
    term_counts['PNG_empty' if f.endswith('.png') else 'text_pdf_nonempty']+=1
result['Baloo_content_controls']=term_counts
con.close()
OUT.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ('service_times','Baloo_times','post_shutdown_database','Baloo_content_controls')},indent=2))
print(json.dumps({'service':result['service']['counts'],'Baloo':result['Baloo']['counts'],'tier_rows':result['service']['tier_rows'],'source_associations':result['service']['source_page_associations']}))
