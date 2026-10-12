from pathlib import Path
import hashlib,json,re,shutil
root=Path.cwd();out=root/'evidence/runner-privacy-review-v1';author=root/'evidence/runner-privacy-v1';original=root/'evidence/desktop-integration-v1'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def pins(path,base):
 lines=path.read_text().splitlines();bad=[]
 for line in lines:
  digest,name=line.split('  ',1);p=base/name
  if not p.is_file() or sha(p)!=digest:bad.append(name)
 assert not bad,bad
 return {'count':len(lines),'manifest_sha256':sha(path),'all_match':True}
r={'candidate':{p.name:sha(p) for p in (author/'candidate').iterdir()},'author':pins(author/'SHA256SUMS',author),'root':pins(author/'root-source-pins.sha256',root),'original':pins(original/'SHA256SUMS',original)}
assert r['candidate']['kde-index-runner.so']=='522baa458d9b2b20de311b21e2454c576573b2eba6fde512f0704beefbed25c1'
for f in ['runner.cpp','CMakeLists.txt','kde-index-runner.json']:assert (root/'src/runner-v5'/f).read_bytes()==(author/'candidate'/f).read_bytes()
for f in ['CMakeLists.txt','kde-index-runner.json']:assert (root/'src/runner-v5'/f).read_bytes()==(root/'src/runner-v4'/f).read_bytes()
consumers={'ToolTip.qml':Path('/usr/lib/qt6/qml/org/kde/plasma/components/ToolTip.qml'),'ResultDelegate.qml':Path('/usr/lib/qt6/qml/org/kde/milou/ResultDelegate.qml'),'ResultsView.qml':Path('/usr/lib/qt6/qml/org/kde/milou/ResultsView.qml')}
r['current_consumer_matches']={f:sha(p)==sha(author/'primary'/('installed-'+f)) for f,p in consumers.items()};assert all(r['current_consumer_matches'].values())
rows=[json.loads(x) for x in (author/'manager-v5.jsonl').read_text().splitlines()];v4={x['case']:x for x in [json.loads(x) for x in (author/'manager-v4.jsonl').read_text().splitlines()]}
transform=lambda s:s.translate(str.maketrans({'<':'＜','>':'＞','&':'＆'}))
clip=lambda s,n: (lambda t:t if len(t)<=n else t[:n-1]+'…')(re.sub(r'\s+',' ',s))
r['fixture_fields']=[];rich_negative=0
for row in rows:
 m=row['matches'][0];before=v4[row['case']]['matches'][0]
 assert len(row['matches'])==len(v4[row['case']]['matches'])==1
 assert row['request']==v4[row['case']]['request'] and row['response']==v4[row['case']]['response']
 for field in ['title','subtext']:
  a,b=m[field]['text'],before[field]['text'];assert a==transform(b) and not any(c in a for c in '<>&')
  rich_negative+=int(before[field]['mightBeRichText'])
  r['fixture_fields'].append({'case':row['case'],'field':field,'v4_raw':b,'v5_actual':a})
 for field in ['id','data','urls','relevance']:assert m[field]==before[field]
 assert row['peer_observed_close'] and v4[row['case']]['peer_observed_close']
r['fixture_counts']={'cases':len(rows),'fields':len(r['fixture_fields']),'v4_rich_display_fields':rich_negative,'raw_action_maps_and_urls':len([x for x in rows if x['case']!='error'])}
r['native']=[]
for n in ['native-run1','native-run2']:
 b=author/n;d=json.loads((b/'results.json').read_text());assert d['status']=='frontends-observed' and 'error' not in d
 request=next(x for x in d['events'] if x['connection']==0 and x['direction']=='request');reply=next(x for x in d['events'] if x['connection']==0 and x['direction']=='reply');raw=reply['raw']['results'][0]
 assert raw['title']=='<b>TITLE-77331.txt';assert (b/'corpus'/raw['title']).read_text().startswith(raw['text']);assert sha(b/'corpus'/raw['title'])==d['sources'][raw['path']]
 assert d['stage_binary_sha256']['/tmp/stage/lib/qt6/plugins/kf6/krunner/kde-index-runner.so']==r['candidate']['kde-index-runner.so']
 assert request['monotonic_ns']<reply['monotonic_ns']<d['http_gets'][-1]['monotonic_ns'];assert [x['path'] for x in d['http_gets']]==['/positive-control','/positive-after-hover']
 assert d['ipc_peers'][0]['identity']['exe']=='/usr/bin/krunner'
 nodes=d['runner_after_hover']['snapshot']['nodes'];observed=[x for x in nodes if x.get('text','').startswith('DESKTOP-77331 ＜img') and x.get('textFormat') in ['AutoText','PlainText']]
 assert len(observed)>=2 and all(x['text']==transform(clip(raw['text'],240)) for x in observed)
 assert any(x['class'].startswith('Label') and x['textFormat']=='AutoText' for x in observed)
 r['native'].append({'name':n,'raw_results_sha256':sha(b/'results.json'),'raw_request':request,'raw_reply':reply,'live_http_gets':d['http_gets'],'observed_passage_fields':observed,'revealed_original_source':d['runner_reveal_target'],'closure':d['closure'],'unqualified_geometry':{k:d.get(k) for k in ['runner_geometry_controls']}})
orig=json.loads((original/'raw/run8/results.json').read_text());r['original_fetch']={'results_sha256':sha(original/'raw/run8/results.json'),'http_gets':orig['http_gets']}
c=json.loads((out/'consumer.json').read_text());assert c['phases'][0]['gets_cumulative']==0 and c['phases'][1]['gets_cumulative']==1 and len(c['gets'])==1;assert all(not x['tip_visible'] for x in c['phases']);assert c['server_closed'] and c['socket_children_after_deferred_delete']==0
r['independent_native_consumer']={'phase_names':[x['name'] for x in c['phases']],'candidate_gets':0,'raw_source_gets':1,'hidden_both':True,'exit':json.loads((out/'consumer-run.json').read_text())['exit']}
(out/'analysis.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n')
shutil.copy2(author/'manager-v5.jsonl',out/'consumer-input.jsonl');shutil.copy2(root/'local/build-review/runner-privacy-review-v1/consumer',out/'consumer-binary')
print(json.dumps({k:r[k] for k in ['author','root','original','current_consumer_matches','fixture_counts','independent_native_consumer']},indent=2))
