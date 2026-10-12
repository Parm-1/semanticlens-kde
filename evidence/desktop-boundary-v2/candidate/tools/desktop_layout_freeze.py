#!/usr/bin/env python3
"""Freeze the sole v2 matched pair. Reject already sealed evidence."""
from pathlib import Path
import hashlib,json,shutil
P=Path(__file__).resolve().parents[1];E=P/'evidence/desktop-boundary-v2';R=P/'local/desktop-boundary-v2-run1';sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
if (E/'SHA256SUMS').exists():raise SystemExit('Sealed evidence: use a linked new version')
for n in ['candidate/tools','candidate/bin','candidate/source','raw','prior','inherited']:(E/n).mkdir(parents=True,exist_ok=True)
for f in P.glob('tools/desktop_layout*'):
 if f.is_file():shutil.copy2(f,E/'candidate/tools'/f.name)
shutil.copy2(R/'libdesktop-layout-observer.so',E/'candidate/bin/libdesktop-layout-observer.so')
shutil.copy2(P/'evidence/clean-build-v4/kde-index-runner.so',E/'candidate/bin/kde-index-runner.so')
for f in [*P.glob('src/runner-v5/*'),*P.glob('src/ui/*')]:
 if f.is_file():t=E/'candidate/source'/f.relative_to(P);t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
for f in R.rglob('*'):
 if not f.is_file():continue
 rel=f.relative_to(R)
 if any(n in rel.parts for n in ['cache','index','home','runtime']):continue
 if rel.parts[0]=='data' and rel.parts[1]!='applications':continue
 if rel.parts[0]=='config' and f.name!='mimeapps.list':continue
 t=E/'raw'/rel;t.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,t)
for n in ['configure.log','build.log']:shutil.copy2(P/'local/desktop-boundary-v2-source'/n,E/'raw'/n)
for n in ['SHA256SUMS','report.md']:shutil.copy2(P/'evidence/desktop-integration-v1'/n,E/'inherited'/('desktop-v1-'+n))
shutil.copy2(P/'evidence/desktop-integration-v1/raw/run8/runner-observer/runner-frame.png',E/'prior/v4-querybar-only.png')
for n in ['runner-pre-geometry.json','runner-set-height.json','runner-set-list-width.json','runner-current.json','runner-tooltip.json','runner-frame.png']:
 shutil.copy2(P/'evidence/runner-privacy-v1/native-run2/runner-observer'/n,E/'prior'/('v5-forced-'+n))
for src,target in [('evidence/runner-privacy-v1/report.md','runner-privacy-v1-report.md'),('evidence/runner-privacy-v1/SHA256SUMS','runner-privacy-v1-SHA256SUMS'),('evidence/clean-build-v4/report.md','clean-build-v4-report.md'),('evidence/clean-build-v4/pins.sha256','clean-build-v4-pins.sha256')]:shutil.copy2(P/src,E/'inherited'/target)
# Primary source used without changes in the original package is re-used byte-for-byte.
for n in ['RunCommand-v6.7.5.qml','krunner-view-v6.7.5.cpp','krunner-main-v6.7.5.cpp','installed-ResultDelegate.qml','installed-ResultsView.qml','kf-resultsmodel-v6.30.0.cpp','runnerresultsmodel-v6.30.0.cpp','qdesktopunixservices-v6.12.0.cpp']:
 shutil.copy2(P/'evidence/desktop-integration-v1/primary'/n,E/'primary'/n)
shutil.copy2('/usr/share/xdg-desktop-portal/kde-portals.conf',E/'primary/installed-kde-portals.conf');shutil.copy2('/usr/share/xdg-desktop-portal/portals/kde.portal',E/'primary/installed-kde.portal')
r=json.loads((R/'results.json').read_text());outer=json.loads((R/'outer.json').read_text());cases=[]
for name,expected in [('builtin-completed','3'),('plugin-completed','ordinary-layout-66442.txt')]:
 full=json.loads((R/'runner-observer'/(name+'.json')).read_text());case={'name':name,'expected_source_label':expected,'actual_query_completed_phase':full['query_completed_phase'],'actual_loaded_maps':full['actual_loaded_maps'],'images':{'first':full.get('first_readback'),'second':full.get('second_readback')}};phaseout={}
 for phase in ['pre_readback','post_first_readback','next_event_turn','post_second_readback']:
  snap=full.get(phase,{})
  phaseout[phase]={'elapsed_ns':snap.get('elapsed_ns'),'nodes':[n for n in snap.get('nodes',[]) if n['class']=='View' or n['class'].startswith(('RunCommand','ResultsView','ScrollView','QQuickLayoutAttached')) or n.get('text')==expected or n.get('text','').startswith('NATIVE-LAYOUT-66442 plain')]}
 case['phases']=phaseout;cases.append(case)
analysis={'cases':cases,'source':r['source'],'actual_binaries':r['binaries'],'stock_plugin_discovery_exit':r['plugin_discovery_exit'],'actual_source_peers':r['ipc_peers'],'actual_search_reply_rows':[e for e in r['events'] if e['direction']=='reply' and e['raw'].get('results')],'portal':{'actual_api':r['open_api_events'],'endpoint':r['portal_endpoint'],'actual_default_association':(R/'config/mimeapps.list').read_text(),'actual_handler_desktop_entry':(R/'data/applications/desktop-boundary-text-opener.desktop').read_text(),'backend_trace_reference':'raw/dbus-monitor.log','source_debug_reference':'raw/session.log','recommended_choices_observed':[],'fresh_permission_store_observed':'No entry for text/plain','response_or_loaded_consumer':'not qualified; chooser was not accepted'},'closure':r['closure'],'outer':outer,'resource_release':'handle6431 completed exit0, no matching host pair process remained; native interval explicitly released to parent before offline freeze','measurement_scope':{'token_usage':'unknown','billing':'unknown','diagnostic_outer_seconds':outer['elapsed_seconds'],'meaning':'sole private matched pair+portal trace only; not total engineering cost or latency benchmark'}}
(E/'analysis.json').write_text(json.dumps(analysis,indent=2)+'\n');print(json.dumps({'cases':len(cases),'fresh_source_inode':analysis['source']['inode'],'outer_exit':outer['namespace_exit'],'files':sum(f.is_file() for f in E.rglob('*'))}))
