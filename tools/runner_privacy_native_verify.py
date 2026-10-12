#!/usr/bin/env python3
"""Check producer/consumer/native receipts, never equate QML visibility to pixels."""
import hashlib,json,sys
from pathlib import Path
from urllib.parse import quote
base=Path(sys.argv[1]);r=json.loads((base/'results.json').read_text())
assert r['status']=='frontends-observed' and 'error' not in r
assert r['indexed_status']['indexed']==1 and r['indexed_status']['errors']==1
assert [x['path'] for x in r['http_gets']]==['/positive-control','/positive-after-hover']
peer=next(p for p in r['ipc_peers'] if p['connection']==0)
assert peer['identity']['exe']=='/usr/bin/krunner' and '--runner kde-index-runner' in peer['identity']['cmdline']
wire=[e for e in r['events'] if e['connection']==0]
request=next(e for e in wire if e['direction']=='request');reply=next(e for e in wire if e['direction']=='reply')
assert request['raw']['text']=='DESKTOP-77331' and request['raw']['op']=='search'
row=next(x for x in reply['raw']['results'] if x['path']=='/tmp/work/corpus/<b>TITLE-77331.txt')
assert row['title']=='<b>TITLE-77331.txt' and '<img src="http://127.0.0.1:' in row['text']
assert '/passage" /> &lt;ENTITY&gt; R&D A & B ' in row['text']
source=base/'corpus/<b>TITLE-77331.txt'
assert hashlib.sha256(source.read_bytes()).hexdigest()==r['sources'][row['path']]
assert source.read_text().startswith(row['text'])
assert request['monotonic_ns']<=reply['monotonic_ns']<r['http_gets'][1]['monotonic_ns']
expected_title='＜b＞TITLE-77331.txt'
node_sets=[r['runner_visible_initial']['snapshot']['nodes'],r['runner_after_hover']['snapshot']['nodes']]
for nodes in node_sets:
    assert any(n.get('text')==expected_title and n.get('textFormat')=='AutoText' and n['class'].startswith('Label') for n in nodes)
    assert any('/tmp/stage/lib/qt6/plugins/kf6/krunner/kde-index-runner.so' in line for line in r['runner_visible_initial']['snapshot']['loaded_relevant_maps'])
passage_nodes=[n for n in node_sets[1] if n.get('text','').startswith('DESKTOP-77331 ＜img')]
assert any(n.get('textFormat')=='PlainText' for n in passage_nodes)
assert any(n.get('textFormat')=='AutoText' and n['class'].startswith('Label') for n in passage_nodes)
assert all('＆lt;ENTITY＆gt; R＆D A ＆ B' in n['text'] and not any(c in n['text'] for c in '<>&') for n in passage_nodes)
app=r['initial_result_nodes']['app']
assert any(n.get('text')==row['title'] and n.get('textFormat')=='PlainText' for n in app)
assert any(n.get('text')==row['text'] and n.get('textFormat')=='PlainText' for n in app)
assert r['runner_reveal_invocation'] is True
url='file://'+quote(row['path'],safe='/')
assert any(n['actual_view_url']=='file:///tmp/work/corpus' and n['actual_selected_urls']==[url] for n in r['runner_reveal_target'])
assert r['closure']['relay_threads_closed'] and r['closure']['canary_thread_closed']
assert r['closure']['owned_exits'][0]['exit']==0
assert all(n['exit'] is not None for n in r['closure']['owned_exits'])
# Preserve falsifiers: popup not visible, so no physical/default-viewport claim.
assert not any(n.get('visible') for n in passage_nodes if n['class'].startswith('ToolTip'))
if 'runner_geometry_controls' in r:
    assert [n['command']['property'] for n in r['runner_geometry_controls']]==['height','width']
    assert all(n['success'] for n in r['runner_geometry_controls'])
print('PASS: actual indexed source/raw reply; stock uninstrumented peer; 2 live-canary positives/0 passage GET; instantiated AutoText display fields; original app literals; native Dolphin original URL; closure. Popup/presentation remains unqualified.')
