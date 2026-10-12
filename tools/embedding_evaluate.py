#!/usr/bin/env python3
"""Evaluate only the frozen vector references and retrieval labels; never rewrite them."""
import json,math,sys
from pathlib import Path
p=Path(sys.argv[1] if len(sys.argv)>1 else 'evidence/embedding-v1')
if (p/'results.json').exists(): raise SystemExit('Results already exist. Use a successor evidence version.')
rows=[json.loads(x) for x in (p/'run.jsonl').read_text().splitlines()]
r={x['id']:x for x in rows}
reference=json.loads((p/'inputs/vectors.json').read_text())
texts=json.loads((p/'inputs/texts.json').read_text())
refs=reference['vectors']
def dot(a,b):
 assert len(a)==len(b)==768 and all(math.isfinite(x) for x in a+b)
 return sum(x*y for x,y in zip(a,b))
def cosine(a,b): return dot(a,b)/math.sqrt(dot(a,a)*dot(b,b))
actual=dict(zip(texts,r['reference-texts']['vectors']))
actual.update({name:r[name]['vector'] for name in ('screen-280','photo-140','frame-70')})
parity=[dict(name=name,cosine=cosine(v,refs[name]),dot=dot(v,refs[name]),norm=math.sqrt(dot(v,v)),pass_threshold=dot(v,refs[name])>=.999) for name,v in actual.items()]
negative=[]
for name,ref in [('negative-unprompted-query','query'),('negative-screen-budget70','screen-280')]:
 v=r[name].get('vector',r[name].get('vectors',[None])[0]);negative.append(dict(name=name,cosine=cosine(v,refs[ref]),below_parity_threshold=cosine(v,refs[ref])<.999))
errors=[dict(name=name,ok=r[name]['ok'],error=r[name].get('error'),vector_absent='vector' not in r[name] and 'vectors' not in r[name]) for name in ('negative-invalid-budget','negative-missing-file','negative-wrong-budget-type','negative-missing-text')]
fixture=json.loads((p/'inputs/retrieval.json').read_text())
rankings=[]
for q in fixture['queries']:
 scores=sorted([(doc['id'],dot(r[q['id']]['vector'],r['doc-'+doc['id']]['vector'])) for doc in fixture['corpus']],key=lambda x:(-x[1],x[0]))
 rankings.append(dict(id=q['id'],relevant=q['relevant'],scores=scores,rank=next((i+1 for i,(name,_) in enumerate(scores) if name in q['relevant']),None),seconds=r[q['id']]['seconds']))
semantic=[x for x in rankings if x['relevant'] and x['id']!='identifier-q']
summary={'reference_made_with':reference['made_with'],'parity':parity,'negatives':negative,'error_cases':errors,'recovery_query_cosine':cosine(r['reference-query-after-errors']['vector'],refs['query']),'retrieval':rankings,'retrieval_semantic_image':{'queries':len(semantic),'top1':sum(x['rank']<=1 for x in semantic),'top5':sum(x['rank']<=5 for x in semantic)},'info':r['info']['info'],'request_count':len(rows),'notes':['Diagnostic synthetic corpus; no release retrieval/Baloo benchmark or abstention proof.','Seconds use C++ steady_clock around request execution; load_seconds includes model/media warmup and two text warmups. RSS uses Linux resource.getrusage for the sole child in a fresh Python runner.']}
queryOnly=json.loads((p/'query-only.jsonl').read_text())
summary['text_only_query_cosine']=cosine(queryOnly['vector'],refs['query'])
(p/'results.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2)+'\n')
assert len(actual)==8 and all(x['pass_threshold'] for x in parity)
assert all(x['below_parity_threshold'] for x in negative)
assert all(not x['ok'] and x['vector_absent'] for x in errors)
assert summary['recovery_query_cosine']>=.999 and summary['text_only_query_cosine']>=.999
print(json.dumps({k:v for k,v in summary.items() if k not in ('retrieval','info')},indent=2))
