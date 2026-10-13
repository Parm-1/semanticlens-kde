#!/usr/bin/env python3
"""Owned-film fixture controls through the real service. No default user paths."""
import argparse,sqlite3,hashlib,json,os,pathlib,shutil,socket,subprocess,time
p=argparse.ArgumentParser();p.add_argument('--output',required=True);p.add_argument('--build',required=True);p.add_argument('--fixtures',required=True);p.add_argument('--probe',required=True);p.add_argument('--models',required=True);a=p.parse_args()
out=pathlib.Path(a.output).resolve();out.mkdir(parents=True,exist_ok=True);build=pathlib.Path(a.build).resolve();probe=pathlib.Path(a.probe).resolve();models=pathlib.Path(a.models).resolve();fixtures=pathlib.Path(a.fixtures).resolve()
def digest(p):return hashlib.file_digest(open(p,'rb'),'sha256').hexdigest()
corpus=out/'corpus';shutil.copytree(fixtures,corpus);data=out/'data';sockpath=out/'service.sock';tokenizer=build/'semanticlens-film-tokenizer'
log=open(out/'service.log','w');env=os.environ.copy();capture=out.parent/'transport_capture.so'
capture_handle=open(capture,'rb') if capture.exists() else None
if capture_handle:env.update(LD_PRELOAD=f'/proc/{os.getpid()}/fd/{capture_handle.fileno()}',FILM_TRANSPORT_CAPTURE=str(out/'transport.log'))
argv=[str(build/'kde-index-service'),'--data-dir',str(data),'--socket',str(sockpath),'--probe',str(probe),'--probe-sha256',digest(probe),'--text-model',str(models/'embeddinggemma-2-Q8_0.gguf'),'--image-model',str(models/'mmproj-embeddinggemma-2-Q8_0.gguf'),'--threads','2','--film-tokenizer',str(tokenizer),'--film-tokenizer-sha256',digest(tokenizer)]
candidate_sha=digest(build/'kde-index-service');proc=subprocess.Popen(argv,stdout=log,stderr=log,env=env);records=[];checks=[];seq=0;buf=b'';client=None
start=time.monotonic()
def connect():
 s=socket.socket(socket.AF_UNIX);s.settimeout(20)
 for _ in range(200):
  try:s.connect(str(sockpath));return s
  except (FileNotFoundError,ConnectionRefusedError):time.sleep(.05)
 raise RuntimeError('service socket unavailable')
def call(op,**kw):
 global seq,buf
 seq+=1;r={'id':f'control-{seq}','op':op,**kw};client.sendall((json.dumps(r,ensure_ascii=False)+'\n').encode());
 while b'\n' not in buf:buf+=client.recv(1024*1024)
 line,buf=buf.split(b'\n',1);reply=json.loads(line);records.append({'request':r,'reply':reply});assert reply['id']==r['id'],(r,reply);return reply
def good(op,**kw):
 r=call(op,**kw);assert r['ok'],(op,r);return r
def check(label,condition):checks.append({'check':label,'passed':bool(condition)});assert condition,label
def job(op,rel,cid):
 sha=good('film_sidecar_digest',collection_id=cid,relative_path=rel)['sha256'];r=good(op,collection_id=cid,relative_path=rel,expected_sha256=sha);jid=r['job_id']
 for _ in range(500):
  state=good('film_status',job_id=jid)['status']['jobs'][0]
  if state['state'] in ['complete','failed','canceled','interrupted']:return state
  time.sleep(.02)
 raise RuntimeError('job timeout '+jid)
try:
 client=connect()
 for _ in range(1500):
  st=good('status')['status']
  if st.get('ready'):break
  time.sleep(.04)
 check('legacy ready with unchanged producer',st.get('ready') and st['identity']['producer_sha256']==digest(probe))
 cap=good('film_capabilities');check('film enabled',cap['ready'])
 cid=good('film_add_collection',path=str(corpus))['collection_id'];time.sleep(.8)
 for name in ['harbor','harbor-cut','orchard','crossing','archive','parcel-pdf']:
  state=job('film_import_manifest',f'{name}/manifest.json',cid);check('manifest '+name,state['state']=='complete')
 for name in ['harbor','harbor-cut','orchard','crossing']:
  state=job('film_import_analysis',f'{name}/analysis.json',cid);check('analysis '+name,state['state']=='complete')
 status=good('film_status')['status'];check('six distinct editions',status['counts']['editions']==6);check('four analyses',status['counts']['analyses']==4)
 queries=json.loads((fixtures/'queries.json').read_text());query_results=[]
 for query in queries:
  r=good('film_search',text=query['query'],mode='hybrid',collection_id=cid,spoiler_policy='allow');query_results.append({'query':query,'reply':r})
  if query.get('expected_film_id') and query.get('expected_match')=='literal':
   matches=[x for x in r['results'] if x['film_id']==query['expected_film_id'] and x['edition_id']==query['expected_edition_id']];check('literal film '+query['id'],bool(matches) and r['results'][0] in matches)
   if query['expected_lane']=='source':check('source literal '+query['id'],matches[0]['source_exact'])
   else:check('analysis literal not source exact',not matches[0]['source_exact'] and any(l['lane']=='analysis_lexical' for l in matches[0]['lanes']))
 (out/'queries.json').write_text(json.dumps(query_results,indent=2,ensure_ascii=False)+'\n')
 r=good('film_search',text='CLOCK-7320',mode='source',collection_id=cid);row=r['results'][0];check('server default hide',all(e['source_excerpt'] is None and e['analysis_text'] is None for x in r['results'] for e in x['evidence']))
 ref=row['evidence'][0]['reference'];rev=r['film_revision'];e=good('film_evidence',reference=ref,expected_film_revision=rev)['evidence'];check('evidence default hide',e['source_excerpt'] is None)
 e=good('film_evidence',reference=ref,expected_film_revision=rev,spoiler_policy='allow')['evidence'];check('explicit source reveal',e['source_excerpt']=='CLOCK-7320')
 check('navigation deny hidden',not call('film_navigation',reference=ref,expected_film_revision=rev,action='open')['ok'])
 nav=good('film_navigation',reference=ref,expected_film_revision=rev,action='open',spoiler_policy='allow')['navigation'];copy=pathlib.Path(nav['snapshot_path']);check('navigation hash-bound copy',copy.exists() and digest(copy)==ref['source_revision'] and (copy.stat().st_mode&0o777)==0o400)
 good('film_release_navigation',token=nav['token']);check('navigation release cleanup',not copy.exists())
 badref=dict(ref,source_revision='0'*64);check('wrong source revision rejected',not call('film_evidence',reference=badref,expected_film_revision=rev,spoiler_policy='allow')['ok'])
 badref=dict(ref,edition_id='edition:harbor:draft-1');check('cross-edition anchor rejected',not call('film_evidence',reference=badref,expected_film_revision=rev,spoiler_policy='allow')['ok'])
 check('traversal rejected',not call('film_sidecar_digest',collection_id=cid,relative_path='../outside.json')['ok'])
 (corpus/'link.json').symlink_to(corpus/'harbor/analysis.json');check('symlink rejected',not call('film_sidecar_digest',collection_id=cid,relative_path='link.json')['ok']);(corpus/'link.json').unlink();time.sleep(.8)
 # Preserve a real failed contradictory-anchor attempt.
 bad=json.loads((corpus/'harbor/analysis.json').read_text());bad['analysis_id']='analysis:contradictory';bad['claims'][0]['evidence'][0]['quote']='not the recorded source';(corpus/'bad-analysis.json').write_text(json.dumps(bad));time.sleep(.8)
 state=job('film_import_analysis','bad-analysis.json',cid);check('contradictory raw quote fails',state['state']=='failed');check('failed analysis leaves sources',good('film_status')['status']['counts']['sources']==6)
 # Native anchored analysis reveal and legitimate anchorless prior references.
 anchored=good('film_search',text='ANALYSIS-ONLY-K7',mode='hybrid',collection_id=cid,spoiler_policy='allow')
 ar=next(e['reference'] for e in anchored['results'][0]['evidence'] if e['reference'].get('analysis_id'))
 revealed=good('film_evidence',reference=ar,expected_film_revision=anchored['film_revision'],spoiler_policy='allow')['evidence']
 check('public anchored analysis reference reveals',bool(revealed['analysis_text']) and bool(revealed['source_excerpt']))
 check('declared provenance exact and unreviewed',revealed['generation']==json.loads((corpus/'harbor/analysis.json').read_text())['generation'] and revealed['facet']=='central_tension' and revealed['scope']=='scene' and revealed['semantic_review']=='unreviewed')
 prior=json.loads((corpus/'harbor/analysis.json').read_text());prior['analysis_id']='analysis:prior';prior['claims']=[dict(prior['claims'][0],claim_id='claim:prior',basis='unverified_prior',text='PRIOR-LANTERN-616 is an imported unverified interpretation.',evidence=[])]
 (corpus/'prior.json').write_text(json.dumps(prior));time.sleep(.8);state=job('film_import_analysis','prior.json',cid);check('anchorless prior accepted',state['state']=='complete')
 pr=good('film_search',text='PRIOR-LANTERN-616',mode='hybrid',collection_id=cid)
 pe=next(e for e in pr['results'][0]['evidence'] if e['reference'].get('analysis_id')=='analysis:prior');pref=pe['reference']
 check('prior reference has no invented source',pref.get('reference_kind')=='analysis_only' and 'source_id' not in pref and pe['source_excerpt'] is None)
 pe=good('film_evidence',reference=pref,expected_film_revision=pr['film_revision'])['evidence'];check('prior default hide',pe['analysis_text'] is None)
 pe=good('film_evidence',reference=pref,expected_film_revision=pr['film_revision'],spoiler_policy='allow')['evidence'];check('prior reveal honest label',pe['analysis_text'].startswith('PRIOR-LANTERN-616') and pe['label']=='Unverified prior; no source anchor' and pe['source_excerpt'] is None)
 check('prior navigation rejected',not call('film_navigation',reference=pref,expected_film_revision=pr['film_revision'],action='open',spoiler_policy='allow')['ok'])
 invented=dict(pref,source_id='source:harbor:script',source_revision=digest(corpus/'harbor/harbor.txt'),unit_id='line-1',start_codepoint=0,end_codepoint=1)
 check('prior fabricated source reference rejected',not call('film_evidence',reference=invented,expected_film_revision=pr['film_revision'],spoiler_policy='allow')['ok'])
 huge=json.loads((corpus/'harbor/analysis.json').read_text());huge['analysis_id']='analysis:huge-offset';huge['claims'][0]['evidence'][0]['start_codepoint']=2**53
 (corpus/'huge-offset.json').write_text(json.dumps(huge));time.sleep(.8);state=job('film_import_analysis','huge-offset.json',cid);check('contradictory oversized raw offset fails',state['state']=='failed')
 # Nullable metadata is a strict union, not an unchecked object/bool lane.
 for label,value,field in [('nullable-string-positive','declared-fixture-model','model_id'),('wrong-model-object',{},'model_id'),('wrong-provider-bool',False,'provider'),('wrong-seed-number',2.5,'seed')]:
  metadata=json.loads((corpus/'harbor/analysis.json').read_text());metadata['analysis_id']='analysis:'+label
  if field=='seed':metadata['generation']['settings'][field]=value
  else:metadata['generation'][field]=value
  rel=label+'.json';(corpus/rel).write_text(json.dumps(metadata));time.sleep(.4);state=job('film_import_analysis',rel,cid);check(label,state['state']==('complete' if label=='nullable-string-positive' else 'failed'))
 # Consume adapter v3 through actual ingestion and SQLite retained milliseconds.
 for label,body,fmt,wanted in [
  ('hours-overflow','WEBVTT\n\n999999999999999999999:00:00.000 --> 999999999999999999999:00:01.000\nBad.\n','vtt','failed'),
  ('ms-rounding','WEBVTT\n\n2501999793:00:00.000 --> 2501999793:00:01.000\nBad.\n','vtt','failed'),
  ('trailing-tokens','1\n00:00:00,000 --> 00:00:01,000 ignored\nBad.\n','srt','failed'),
  ('vtt-junk','WEBVTTjunk\n\n00:00.000 --> 00:01.000\nBad.\n','vtt','failed'),
  ('max-ms','WEBVTT\n\n2501999792:59:00.990 --> 2501999792:59:00.991\nMAX-MS-BOUNDARY.\n','vtt','complete')]:
  source=corpus/(label+'.'+fmt);source.write_text(body)
  manifest=json.loads((corpus/'orchard/manifest.json').read_text());manifest.update(film_id='film:'+label,edition_id='edition:'+label,display_title='Authored timestamp '+label);manifest['sources'][0].update(source_id='source:'+label,relative_path=source.name,sha256=digest(source))
  rel=label+'.json';(corpus/rel).write_text(json.dumps(manifest));time.sleep(.4);state=job('film_import_manifest',rel,cid);check('adapter v3 ingestion '+label,state['state']==wanted)
  if wanted=='complete':
   db=sqlite3.connect(data/'film.sqlite');stored=json.loads(db.execute("select data from sources where id=?",('source:'+label,)).fetchone()[0]);db.close();check('SQLite exact max timestamp retained',stored['units'][0]['cue_start_ms']==2**53-2 and stored['units'][0]['cue_end_ms']==2**53-1)
 # Duplicate/superseding imports and semantically misleading but structurally honest declarations.
 original=json.loads((corpus/'harbor/analysis.json').read_text())
 before=good('film_status')['status']['film_revision'];state=job('film_import_analysis','harbor/analysis.json',cid);check('duplicate analysis same bytes idempotent',state['state']=='complete' and good('film_status')['status']['film_revision']==before)
 conflict=json.loads(json.dumps(original));conflict['claims'][0]['text']='Conflicting same ID payload.';(corpus/'conflict.json').write_text(json.dumps(conflict));time.sleep(.4);check('same analysis ID changed bytes fails',job('film_import_analysis','conflict.json',cid)['state']=='failed')
 misleading=json.loads(json.dumps(original));misleading['analysis_id']='analysis:misleading';misleading['claims'][0]['text']='MISLEADING-WITNESS-801: Nobody ever helps strangers and everyone receives public thanks. This declared interpretation contradicts the quoted exchange.'
 (corpus/'misleading.json').write_text(json.dumps(misleading));time.sleep(.4);check('structurally valid misleading interpretation retained unreviewed',job('film_import_analysis','misleading.json',cid)['state']=='complete')
 mr=good('film_search',text='MISLEADING-WITNESS-801',mode='hybrid',collection_id=cid,spoiler_policy='allow');me=next(e for e in mr['results'][0]['evidence'] if e['reference'].get('analysis_id')=='analysis:misleading');check('misleading lexical match not source proof',not mr['results'][0]['source_exact'] and me['semantic_review']=='unreviewed' and me['source_excerpt']==original['claims'][0]['evidence'][0]['quote'])
 neg=good('film_search',text='nobody ever helps strangers',mode='hybrid',collection_id=cid);check('negation remains explicitly uncalibrated suggestion',not neg['results'][0]['source_exact'] and neg['results'][0]['suggestion'] and any('not a calibrated absence test' in w for w in neg['warnings']))
 beforeRank=mr['results'][0]['score'];verbose=json.loads(json.dumps(misleading));verbose.update(analysis_id='analysis:verbose',supersedes='analysis:misleading');verbose['claims']=[dict(verbose['claims'][0],claim_id='claim:repeat-'+str(i)) for i in range(4)]
 (corpus/'verbose.json').write_text(json.dumps(verbose));time.sleep(.4);check('declared superseding analysis promotes atomically',job('film_import_analysis','verbose.json',cid)['state']=='complete');vr=good('film_search',text='MISLEADING-WITNESS-801',mode='hybrid',collection_id=cid,spoiler_policy='allow');check('repeated claims do not add rank votes',vr['results'][0]['score']==beforeRank and sum(l['lane']=='analysis_vector' for l in vr['results'][0]['lanes'])==1)
 check('superseded analysis evidence inaccessible',not call('film_evidence',reference=me['reference'],expected_film_revision=vr['film_revision'],spoiler_policy='allow')['ok'])
 absent=good('film_search',text='quasar wilderness lottery 987654321',mode='hybrid',collection_id=cid);check('no-answer control no false literal claim',all(not row['source_exact'] and row['suggestion'] for row in absent['results']) and any('not a calibrated absence test' in w for w in absent['warnings']))
 # Revision/idempotence and withdrawal of dependent interpretation.
 before=good('film_status')['status']['film_revision'];state=job('film_import_manifest','harbor/manifest.json',cid);check('idempotent unchanged import',state['state']=='complete' and good('film_status')['status']['film_revision']==before)
 source=corpus/'harbor/harbor.txt';source.write_bytes(source.read_bytes()+b'\nChanged source revision.\n');time.sleep(.8);good('film_reconcile',collection_id=cid)
 for _ in range(1500):
  s=good('film_status')['status']
  if not s['busy']:break
  time.sleep(.02)
 r=good('film_search',text='ANALYSIS-ONLY-K7',mode='hybrid',collection_id=cid,spoiler_policy='allow');check('changed source withdraws dependent analysis',all(not any(l['lane']=='analysis_lexical' for l in x['lanes']) for x in r['results']))
 check('stale evidence rejected',not call('film_evidence',reference=ref,expected_film_revision=rev,spoiler_policy='allow')['ok'])
 good('film_remove_collection',collection_id=cid);check('collection removal empties active film content',good('film_status')['status']['counts']['sources']==0 and good('film_status')['status']['counts']['vectors']==0)
 check('legacy still ready',good('status')['status']['ready'])
 good('shutdown');client.close();client=None;proc.wait(timeout=15)
except Exception as e:
 checks.append({'check':'run completion','passed':False,'error':repr(e)});raise
finally:
 if client and proc.poll() is None:
  try:call('shutdown')
  except Exception:pass
 if client:client.close()
 if proc.poll() is None:
  try:proc.wait(timeout=15)
  except subprocess.TimeoutExpired:proc.terminate()
 try:proc.wait(timeout=15)
 except subprocess.TimeoutExpired:proc.kill();proc.wait(timeout=10)
 log.close();
 if capture_handle:capture_handle.close()
 (out/'runner-records.json').write_text(json.dumps(records,indent=2,ensure_ascii=False)+'\n');(out/'results.json').write_text(json.dumps({'checks':checks,'passed':all(c['passed'] for c in checks),'elapsed_seconds':time.monotonic()-start,'service_exit':proc.returncode,'process_closed':proc.poll() is not None,'argv':argv,'service_sha256_at_launch':candidate_sha,'probe_sha256':digest(probe),'tokenizer_sha256':digest(tokenizer)},indent=2)+'\n')
print(json.dumps({'checks':len(checks),'passed':all(c['passed'] for c in checks),'process_closed':True}))
