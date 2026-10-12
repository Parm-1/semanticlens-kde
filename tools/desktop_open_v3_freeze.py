#!/usr/bin/env python3
"""Portable delivery of the bounded linked open attempt; reject sealed evidence."""
from pathlib import Path
import hashlib,json,shutil
P=Path(__file__).resolve().parents[1]; E=P/'evidence/desktop-open-v3'
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
assert not (E/'SHA256SUMS').exists(), 'sealed: use a linked successor'
def copy(f,t):
 t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
for f in P.glob('tools/desktop_open_v3*'):
 if f.is_file():copy(f,E/'candidate/tools'/f.name)
for name in ['libdesktop-open-v3-observer.so','desktop-open-v3-gio']:
 copy(P/'build-desktop-open-v3/build'/name,E/'candidate/bin'/name)
for name in ['desktop_layout_observer.cpp','desktop_layout_CMakeLists.txt']:
 copy(P/'evidence/desktop-boundary-v2/candidate/tools'/name,E/'candidate/inherited-tools'/name)
copy(P/'evidence/desktop-boundary-v2/candidate/bin/libdesktop-layout-observer.so',E/'candidate/bin/libdesktop-layout-observer.so')
stage=P/'local/clean-arch-v1/root.x86_64/checkout/local/stage-v5/usr'
stagepins={str(f.relative_to(stage)):sha(f) for f in sorted(stage.rglob('*')) if f.is_file()}
assert len(stagepins)==21
for f in (stage/'bin').iterdir():copy(f,E/'candidate/stage-bin'/f.name)
copy(stage/'lib/qt6/plugins/kf6/krunner/kde-index-runner.so',E/'candidate/stage-bin/kde-index-runner.so')
for f in (P/'src/ui').iterdir():
 if f.is_file():copy(f,E/'candidate/source'/f.relative_to(P))
for i in [1,2]:
 root=P/('local/desktop-open-v3-run'+str(i))
 for f in root.rglob('*'):
  if not f.is_file():continue
  rel=f.relative_to(root)
  if any(v in rel.parts for v in ['index','cache','home','runtime']):continue
  if rel.parts[0]=='data' and rel.parts[1]!='applications':continue
  if rel.parts[0]=='config' and f.name!='mimeapps.list':continue
  copy(f,E/('raw/run'+str(i))/rel)
for n in ['configure.log','build.log']:copy(P/'local/desktop-open-v3-source'/n,E/'raw/build'/n)
for name in ['qdesktopunixservices-v6.12.0.cpp','open-uri-v1.22.1.c','appchooser-kde-v6.7.5.cpp','installed-OpenURI.xml','installed-Request.xml','installed-AppChooser.xml','installed-kde-portals.conf','installed-kde.portal']:
 copy(P/'evidence/desktop-boundary-v2/primary'/name,E/'primary'/name)
for package,names in [('desktop-boundary-v2',['report.md','SHA256SUMS']),('clean-build-v5',['report.md','pins.sha256','stage-comparison.json'])]:
 for name in names:copy(P/'evidence'/package/name,E/'inherited'/(package+'-'+name))
r1=json.loads((E/'raw/run1/results.json').read_text());r=json.loads((E/'raw/run2/results.json').read_text())
s=json.loads((E/'raw/run2/app-observer'/(r['app_source_phase_receipt']+'.json')).read_text())
old=json.loads((E/'raw/run1/app-observer/source-ready-39164224250062.json').read_text())
selected=lambda n:[v for v in n if v.get('text') in ['reader.txt','Open file'] or v.get('class','').startswith('Main_') or v.get('actual_backend_state',{}).get('results')]
body=E/'raw/run2/corpus/reader.txt';capture=json.loads((E/'raw/run2'/r['receiving_capture']).read_text())
analysis={'stage_source_commit':'c394cca','stage_payload_sha256':stagepins,'stage_retained_v4_files_byte_identical':True,
 'run1':{'outer':json.loads((E/'raw/run1/outer.json').read_text()),'error':r1['error'],'calibration':r1['calibration'],'closure':r1['closure'],'negative_nodes':selected(old['snapshot']['nodes']),'actual_wire_events':r1['events'],'OpenFile_call_count':(E/'raw/run1/portal-monitor.log').read_text().count('member=OpenFile')},
 'run2':{'outer':json.loads((E/'raw/run2/outer.json').read_text()),'actual_wire_peers':r['ipc_peers'],'actual_query_completed_rows':[v for v in r['events'] if v['direction']=='reply' and v['raw'].get('results')],'source_phase_nodes':selected(s['snapshot']['nodes']),'actual_gio':r['gio_before_open'],'source':r['sources'][str(Path('/tmp/work/corpus/reader.txt'))],'source_unchanged':r['source_unchanged'],'source_sha256_readback':sha(body),'open_api_events':r['open_api_events'],'receiving_app':r['receiving_app'],'native_terminal_capture_fields':capture['terminals'],'visible_receiver_windows':[w for w in capture['windows'] if w['visible']],'OpenFile_call_count':r['portal_trace'].count('member=OpenFile'),'ChooseApplication_call_count':r['portal_trace'].count('member=ChooseApplication'),'closure':r['closure']},
 'resource_release':'run1 handle26143 terminal1; run2 handle26087 terminal0; exclusive namespaces exited; explicit native interval release sent before freeze',
 'measurement_scope':{'token_usage':'unknown','billing':'unknown','run2_diagnostic_wall_seconds':json.loads((E/'raw/run2/outer.json').read_text())['elapsed_seconds'],'not':'benchmark or total engineering cost'}}
(E/'analysis.json').write_text(json.dumps(analysis,indent=2)+'\n')
assert r['pass'] and analysis['run2']['OpenFile_call_count']==1 and analysis['run2']['ChooseApplication_call_count']==0
assert analysis['run1']['OpenFile_call_count']==0
native={}
for original in ['/usr/bin/konsole','/usr/bin/cat','/usr/lib/libkonsoleprivate.so.26.08.2','/usr/lib/libkonsoleapp.so.26.08.2']:
 f=Path(original);native[original]={'sha256':sha(f),'inode':f.stat().st_ino,'bytes':f.stat().st_size,'phase':'post-run readonly host identity collection; runtime maps pin corresponding loaded module path/inode'}
 copy(f,E/'candidate/native-bin'/f.name)
assert native['/usr/lib/libkonsoleprivate.so.26.08.2']['inode']==1622077
assert native['/usr/lib/libkonsoleapp.so.26.08.2']['inode']==1622076
for original in ['/usr/bin/konsole','/usr/bin/cat']:
 assert native[original]['sha256']==r['binaries'][original]
(E/'native-binary-identities.json').write_text(json.dumps(native,indent=2)+'\n')
print(json.dumps({'frozen_raw_files':sum(f.is_file() for f in E.rglob('*')),'stage_files':len(stagepins),'receiver_body_sha256':sha(body)}))
