#!/usr/bin/env python3
"""Independent offline audit of frozen observations; launches no candidate process."""
import ast
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
P = ROOT / 'evidence/desktop-open-v3'
OUT = Path(__file__).resolve().parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
read = lambda p: json.loads(p.read_text())
facts = {}

# Recheck sealed identities, without rebuilding the unchanged inventory.
manifest = P / 'SHA256SUMS'
assert sha(manifest) == 'a85f50e9f8af44ae4756384b3cf971b94ad77fa4316a66ac93b75b7e3ea8f8e0'
assert sha(P/'report.md') == 'bfc65577a25b39dad828aeff44643dab54b7c4e5ccfd09699adeef4f5424be83'
pins = [line.split('  ', 1) for line in manifest.read_text().splitlines()]
for digest, name in pins:
    assert sha(P/name) == digest, name
facts['identity'] = {'manifest_sha256': sha(manifest), 'report_sha256': sha(P/'report.md'), 'verified_pins': len(pins)}
r1 = read(P/'raw/run1/results.json')
r2 = read(P/'raw/run2/results.json')
for item in r1['binaries']:
    assert r1['binaries'][item] == r2['binaries'][item]
for run in ('run1', 'run2'):
    for name in ('receiver-observer.so', 'app-observer.so'):
        assert sha(P/'raw'/run/name) == r2['binaries']['/tmp/work/'+name]
assert sha(ROOT/'evidence/desktop-boundary-v2/candidate/bin/libdesktop-layout-observer.so') == r2['binaries']['/tmp/work/app-observer.so']
repair_targets = [P/'candidate/tools/desktop_open_v3_run2.py', P/'experiment-repair2.md',
                  P/'candidate/tools/desktop_open_v3_observer.cpp', P/'candidate/bin/libdesktop-open-v3-observer.so',
                  P/'candidate/bin/libdesktop-layout-observer.so']
for line, target in zip((P/'repair2-prelaunch.sha256').read_text().splitlines(), repair_targets, strict=True):
    assert sha(target) == line.split()[0], target
facts['identity']['repair2_prelaunch_pins'] = len(repair_targets)
markers = [(P/'raw/run2/corpus/reader.txt').read_bytes().strip(), (P/'raw/run1/calibration/contrast.txt').read_bytes().strip()]
for f in [P/'candidate/bin/libdesktop-open-v3-observer.so', P/'candidate/bin/libdesktop-layout-observer.so']:
    assert all(marker not in f.read_bytes() for marker in markers)
facts['identity']['observer_body_literals_absent'] = True

def payload(receipt):
    assert len(receipt['terminals']) == 1
    t = receipt['terminals'][0]
    assert t['class'] == 'Konsole::TerminalDisplay'
    assert t['accessible_cpp_type'] == 'N7Konsole25TerminalDisplayAccessibleE'
    assert t['accessible_valid'] and t['text_interface_present'] and t['accessible_role'] == 129
    assert t['actual_native_text'] == t['accessible_value']
    # Check all non-padding screen content, stronger than author's first-line gate.
    lines = t['actual_native_text'].replace('\r\n', '\n').replace('\r', '\n').split('\n')
    while lines and not lines[-1].rstrip(' '):
        lines.pop()
    return ('\n'.join(s.rstrip(' ') for s in lines)+'\n').encode() if lines else b''

source = (P/'raw/run2/corpus/reader.txt').read_bytes()
contrast = (P/'raw/run1/calibration/contrast.txt').read_bytes()
cal = read(P/'raw/run1/receivers/8/calibration-capture.json')
early = read(P/'raw/run2/receivers/109/loaded-39975799153696.json')
loaded = read(P/'raw/run2/receivers/109/loaded-39976048602615.json')
captured = read(P/'raw/run2/receivers/109/actual-receiver-capture.json')
assert payload(cal) == contrast and payload(cal) != source
assert payload(early) == b'' and payload(early) != source
assert payload(loaded) == source == payload(captured)
assert payload(loaded) != contrast
assert len(source) == 41 and sha(P/'raw/run2/corpus/reader.txt') == r2['sources']['/tmp/work/corpus/reader.txt']['sha256']
assert source == (P/'raw/run1/corpus/reader.txt').read_bytes()
facts['receiver_phase'] = {'calibration_full_normalized_sha256': hashlib.sha256(payload(cal)).hexdigest(),
                          'initial_loaded_payload_bytes': len(payload(early)),
                          'loaded_full_normalized_sha256': hashlib.sha256(payload(loaded)).hexdigest(),
                          'readback_full_normalized_sha256': hashlib.sha256(payload(captured)).hexdigest(),
                          'early_elapsed_ns': early['elapsed_ns'], 'loaded_elapsed_ns': loaded['elapsed_ns'],
                          'capture_elapsed_ns': captured['elapsed_ns']}
proc = loaded['actual_process']
assert proc['pid'] == 109 and proc['executable'] == '/usr/bin/konsole' and proc['startticks'] == '3997401'
assert proc['argv'].split() == ['/usr/bin/konsole','--separate','--nofork','--hold','-e','/usr/bin/cat','/tmp/work/corpus/reader.txt']
assert r2['receiver_directories_before_open'] == []
assert source.rstrip(b'\n').decode() not in proc['argv']
assert all(source.rstrip(b'\n').decode() not in w['title'] for w in loaded['windows'])
assert any('1622077' in m and 'libkonsoleprivate.so.26.08.2' in m for m in proc['loaded_konsole_maps'])
assert any('1622076' in m and 'libkonsoleapp.so.26.08.2' in m for m in proc['loaded_konsole_maps'])
facts['receiver_identity'] = proc

# Exercise the actual repaired preflight with original raw observations, no invented fields.
tree = ast.parse((P/'candidate/tools/desktop_open_v3_run2.py').read_text())
fn = next(n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == 'source_ready')
for run, result in [('run1', r1), ('run2', r2)]:
    snapshot = read(P/'raw'/run/'app-observer'/(result['app_source_phase_receipt']+'.json'))
    scope = {'app_command': lambda *args: snapshot, 'source': Path('/tmp/work/corpus/reader.txt'),
             'report': result, 'time': __import__('time')}
    exec(compile(ast.fix_missing_locations(ast.Module(body=[fn], type_ignores=[])), '<frozen-source-ready>', 'exec'), scope)
    answer = scope['source_ready']()
    assert bool(answer) == (run == 'run2')
    title_nodes = [n for n in snapshot['snapshot']['nodes'] if n.get('text') == 'reader.txt']
    facts[run+'_source_phase'] = [{'class': n['class'], 'node': n['node'], 'viewport': n.get('window_clipped_bounds'),
                                  'backend_rows': n.get('actual_backend_state', {}).get('results')} for n in title_nodes]
    facts[run+'_source_phase_repaired_predicate'] = bool(answer)
    if run == 'run2':
        button = answer[1][0]
        signal = read(P/'raw/run2/app-observer/actual-open-signal.json')
        assert signal['command']['node'] == button['node'] and signal['signal_invoked']
        replies = [e for e in r2['events'] if e['direction'] == 'reply' and e['raw'].get('id') == 'ui-2']
        assert len(replies) == 1 and replies[0]['raw']['ok'] and replies[0]['raw']['protocol'] == 1
        assert len(replies[0]['raw']['results']) == 1
        row = replies[0]['raw']['results'][0]
        assert row['path'] == '/tmp/work/corpus/reader.txt' and row['title'] == 'reader.txt' and row['page'] == 0
        assert row['text'].encode() == source
        assert replies[0]['monotonic_ns'] < int(result['app_source_phase_receipt'].rsplit('-',1)[1]) < result['open_action_monotonic_ns']
        facts['raw_completed_reply'] = replies[0]

trace1 = (P/'raw/run1/portal-monitor.log').read_text()
trace2 = (P/'raw/run2/portal-monitor.log').read_text()
assert 'member=OpenFile' not in trace1
assert trace2.count('member=OpenFile') == 1 and 'inode: 8232834' in trace2
assert 'member=ChooseApplication' not in trace2
assert 'path=/org/freedesktop/portal/desktop/request/1_1/t; interface=org.freedesktop.portal.Request; member=Response\n   uint32 0' in trace2
events = [json.loads(line) for line in (P/'raw/run2/app-observer/open-events.jsonl').read_text().splitlines()]
assert len(events) == 1 and events[0]['pid'] == 30 and events[0]['actual_url'] == 'file:///tmp/work/corpus/reader.txt'
assert events[0]['local_file'] == '/tmp/work/corpus/reader.txt'
session = (P/'raw/run2/session.log').read_text()
for text in ['Recommended handlers for file, text/plain: desktop-source-consumer',
             'Allowing to skip app chooser: no choice', 'Skipping app chooser',
             'Launching desktop-source-consumer file:///tmp/work/corpus/reader.txt']:
    assert text in session
facts['portal'] = {'OpenFile_counts': [0,1], 'run2_ChooseApplication_count': 0, 'source_inode': 8232834,
                   'response_request': '/org/freedesktop/portal/desktop/request/1_1/t', 'response': 0, 'api': events[0]}
facts['closure'] = {run: result['closure'] for run, result in [('run1',r1),('run2',r2)]}
for run in ('run1','run2'):
    c = facts['closure'][run]
    assert c['relay_threads_closed'] and c['accept_thread_closed'] and c['owned_socket_files_absent']
    assert all(r['exit'] is not None for r in c['owned_exits'])
    facts[run+'_outer'] = read(P/'raw'/run/'outer.json')
assert facts['run1_outer']['namespace_exit'] == 1 and facts['run2_outer']['namespace_exit'] == 0
facts['review_runtime'] = {'native_launches': 0, 'review_children': 0, 'live_handles': [],
                           'token_usage': 'unknown', 'billing': 'unknown', 'total_elapsed_time': 'unknown'}
(OUT/'raw-audit.json').write_text(json.dumps(facts,indent=2)+'\n')
print(json.dumps({'verified_candidate_pins':len(pins),'repair2_pins':len(repair_targets),
                  'raw_body_sha256':facts['receiver_phase']['loaded_full_normalized_sha256'],
                  'preflight_negative':not facts['run1_source_phase_repaired_predicate'],
                  'preflight_positive':facts['run2_source_phase_repaired_predicate'],'native_launches':0}))
