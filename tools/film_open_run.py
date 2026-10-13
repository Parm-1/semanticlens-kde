#!/usr/bin/env python3
"""Bounded real film-button -> Qt/portal -> native reader; runtime needs explicit owner handback.
Read-only production sources. Only owned corpus/profile/index/wire controls are mutable.
No positive is inferred from a launch/API result. No model-free option claims target proof.
"""
import argparse, copy, hashlib, json, os, re, select, shutil, signal, socket, struct, subprocess, threading, time
from pathlib import Path
from film_open_desktop import launch_dedicated, validate_child_environment, validate_film_worker, film_worker_flags
from film_open_evaluate import search_pair, selection_context, reveal_pair, navigation_pair, pdf_phase_views, pdf_content_view
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
p=argparse.ArgumentParser()
p.add_argument('--package',type=Path,required=True)
p.add_argument('--stage',type=Path)
p.add_argument('--output',type=Path)
p.add_argument('--models',type=Path)
p.add_argument('--film-worker',type=Path,help='Optional exact worker executable, exposed read-only in the dedicated session')
p.add_argument('--film-worker-sha256',help='Expected SHA256 paired with --film-worker')
p.add_argument('--stage-pins',type=Path)
p.add_argument('--approved-stage-pins-sha256')
p.add_argument('--execute-runtime',action='store_true')
p.add_argument('--session',action='store_true')
a=p.parse_args()
try:a.film_worker=validate_film_worker(a.film_worker,a.film_worker_sha256)
except (OSError,ValueError) as error:p.error(str(error))
if not a.session:
    if not a.execute_runtime: p.error('runtime held: execute only after parent explicit resource handback')
    if not all([a.stage,a.output,a.models,a.stage_pins,a.approved_stage_pins_sha256]): p.error('stage/output/models/stage-pins and parent approved-stage-pins-sha256 required')
    pkg=a.package.resolve(); stage=a.stage.resolve(); out=a.output.resolve(); models=a.models.resolve()
    if a.film_worker and sha(a.film_worker)!=a.film_worker_sha256: p.error('film worker SHA256 mismatch before preparation')
    assert re.fullmatch('[0-9a-f]{64}',a.approved_stage_pins_sha256),'parent approval pin required'
    assert sha(a.stage_pins)==a.approved_stage_pins_sha256,'unapproved stage pin file'
    pins=json.loads(a.stage_pins.read_text())
    for rel,digest in pins.items():
        assert not Path(rel).is_absolute() and '..' not in Path(rel).parts
        assert sha(stage/rel)==digest,(rel,'stage identity mismatch')
    for rel in ['bin/kde-index','bin/kde-index-service','bin/embedding-probe','bin/semanticlens-film-tokenizer']: assert rel in pins
    assert pins['bin/embedding-probe']=='fa7d14674f82715386101195d80a959cd5456a16e3cc1621f6ce488ed4be2dd0','unaccepted producer'
    assert sha('/usr/bin/okular')=='7d699b067b6f12d2689cc9a70f8c63f7f2de50b98e02098bbd4ef5c50df01226','unaccepted Okular binary'
    assert not out.exists();out.mkdir(parents=True)
    for n in ['home','config','cache','data','state','runtime','work']:(out/n).mkdir(mode=0o700)
    work=out/'work'
    for n in ['app-observer','receivers']:(work/n).mkdir()
    (out/'config/xdg-desktop-portal').mkdir()
    (out/'config/xdg-desktop-portal/portals.conf').write_text('[preferred]\ndefault=kde\n')
    (out/'runtime').chmod(0o700)
    shutil.copy2(__file__,work/'run.py');shutil.copy2(Path(__file__).with_name('film_open_desktop.py'),work/'film_open_desktop.py');shutil.copy2(Path(__file__).with_name('film_open_evaluate.py'),work/'film_open_evaluate.py');shutil.copy2(a.stage_pins,work/'stage-pins.json')
    (work/'stage-approval.json').write_text(json.dumps({'approved_stage_pins_sha256':a.approved_stage_pins_sha256,'actual_stage_pins_sha256':sha(a.stage_pins),'stage_pins':pins},indent=2)+'\n')
    for source,target in [('bin/libfilm-open-observer.so','app-observer.so'),('bin/libfilm-open-pdf-observer.so','pdf-observer.so'),('bin/libfilm-open-konsole-observer.so','receiver-observer.so'),('inherited/bin/gio-inspect','gio-inspect')]:shutil.copy2(pkg/source,work/target)
    shutil.copytree(pkg/'fixtures/corpus',work/'corpus')
    # Preserve bytes; modes of owned copies become read-only. Never write originals.
    for f in (work/'corpus').rglob('*'):
        if f.is_file():f.chmod(0o444)
    result=launch_dedicated(out,stage,models,bound=180,worker=a.film_worker,worker_sha256=a.film_worker_sha256)
    print(json.dumps(result,indent=2));raise SystemExit(0 if result['passed'] else 1)
try:
    session_environment=validate_child_environment()
except Exception as error:
    Path('/tmp/test/session-environment.json').write_text(json.dumps({'error':repr(error)},indent=2)+'\n')
    raise
Path('/tmp/test/session-environment.json').write_text(json.dumps(session_environment,indent=2)+'\n')
if a.film_worker and (str(a.film_worker)!='/tmp/test/film-worker' or sha(a.film_worker)!=a.film_worker_sha256):
    raise SystemExit('inside-session film worker path or SHA256 mismatch')
O=Path('/tmp/test/work');source=O/'corpus/harbor/harbor.txt';pdf=O/'corpus/parcel-pdf/parcel.pdf'
report={'session_environment':session_environment,'approved_stage':json.loads((O/'stage-approval.json').read_text()),'film_worker':({'path':str(a.film_worker),'sha256':a.film_worker_sha256} if a.film_worker else None),'scope':'owned authored text and two-page PDF full-copy native reader, page intent and copy lifecycle','passed':None,'events':[],'control_records':[],'checks':[],'package_usage':'unknown','timing_scope':'diagnostic execution only; no latency/engineering-cost benchmark'}
owned=[];logs=[];threads=[];peers=[];stop=threading.Event();listener=None;client=None;seq=0;buffer=b'';mutation=None;live_connections={};selected_context=None;issued_tokens={}
originals={str(f):{'sha256':sha(f),'size':f.stat().st_size,'mode':oct(f.stat().st_mode&0o777),'inode':f.stat().st_ino} for f in [source,pdf]};report['sources']=originals
report['binaries']={str(f):sha(f) for f in [Path('/tmp/test/stage/bin/kde-index'),Path('/tmp/test/stage/bin/kde-index-service'),Path('/tmp/test/stage/bin/embedding-probe'),Path('/tmp/test/stage/bin/semanticlens-film-tokenizer'),Path('/usr/bin/konsole'),Path('/usr/bin/cat'),O/'receiver-observer.so',O/'app-observer.so',O/'pdf-observer.so',Path('/usr/bin/okular'),Path('/usr/lib/libOkular6Core.so.4.0.0'),Path('/usr/lib/qt6/plugins/kf6/parts/okularpart.so'),Path('/usr/bin/tesseract'),Path('/usr/bin/pdftotext')]}
for relative,digest in report['approved_stage']['stage_pins'].items():
    assert sha(Path('/tmp/test/stage')/relative)==digest,('stage changed inside namespace',relative)
apps=Path('/tmp/test/data/applications');apps.mkdir();(apps/'film-source-consumer.desktop').write_text('[Desktop Entry]\nType=Application\nName=Owned film source reader\nExec=/usr/bin/konsole --separate --nofork --hold -e /usr/bin/cat %f\nMimeType=text/plain;\nNoDisplay=false\nTerminal=false\n');(Path('/tmp/test/config/mimeapps.list')).write_text('[Default Applications]\ntext/plain=film-source-consumer.desktop;\n')
def wait(fn,bound=15):
    deadline=time.monotonic()+bound
    while time.monotonic()<deadline:
        r=fn()
        if r:return r
        time.sleep(.04)
    raise TimeoutError('bounded condition '+str(bound)+'s')
def check(name,value):
    report['checks'].append({'name':name,'passed':bool(value)})
    assert value,name

def spawn(argv,name,env_changes=None):
    log=(O/(name+'.log')).open('wb');logs.append(log);r=subprocess.Popen(argv,env={**os.environ,**(env_changes or {})},stdout=log,stderr=log);owned.append(r);return r

def command(directory,name,op='snapshot',**fields):
    cmd={'id':name,'op':op,**fields};tmp=directory/'command.tmp';tmp.write_text(json.dumps(cmd));tmp.rename(directory/'command.json');receipt=directory/(name+'.json');wait(receipt.exists,5);return json.loads(receipt.read_text())
def app_command(name,op='snapshot',**fields):return command(O/'app-observer',name,op,**fields)
def snap():
    receipt=app_command('snapshot-'+str(time.monotonic_ns()))['snapshot'];require_receiver_environment(receipt);return receipt
def nodes(s,name):return [n for n in s['nodes'] if n.get('name')==name]
def state(s):return next(n['actual_film_state'] for n in s['nodes'] if 'actual_film_state' in n)
def clickable(s,name):return next((n for n in nodes(s,name) if n.get('enabled',True) and n.get('geometry_intersects_viewport')),None)
def action(name,op='click',**fields):
    s=snap();n=clickable(s,name)
    if not n:
        panel=clickable(s,'filmEvidencePanel')
        if panel:app_command('scroll-'+str(time.monotonic_ns()),'wheel',node=panel['node'],delta=-480);s=snap();n=clickable(s,name)
    assert n,('no visible enabled input target',name)
    r=app_command('input-'+str(time.monotonic_ns()),op,node=n['node'],**fields);assert r['input_dispatched'];return r

def req(op,**kw):
    global seq,buffer
    seq+=1;request={'id':'control-'+str(seq),'op':op,**kw};client.sendall(json.dumps(request).encode()+b'\n')
    while b'\n' not in buffer:
        b=client.recv(1024*1024)
        if not b:raise RuntimeError('control EOF')
        buffer+=b
    line,buffer=buffer.split(b'\n',1);r=json.loads(line);report['control_records'].append({'request':request,'reply':r});assert r['id']==request['id'] and r['ok'],r;return r

def copy_readback(path):
    f=Path(path);return {'path':path,'exists':f.exists(),'sha256':sha(f) if f.is_file() else None,'size':f.stat().st_size if f.exists() else None,'mode':oct(f.stat().st_mode&0o777) if f.exists() else None,'directory_mode':oct(f.parent.stat().st_mode&0o777) if f.parent.exists() else None,'inode':f.stat().st_ino if f.exists() else None,'device':f.stat().st_dev if f.exists() else None,'symlink':f.is_symlink()}

def relay(peer,index):
    dest=socket.socket(socket.AF_UNIX);dest.connect('/tmp/test/work/service.sock');peers.extend([peer,dest]);live_connections[index]=(peer,dest);pid,uid,gid=struct.unpack('3i',peer.getsockopt(socket.SOL_SOCKET,socket.SO_PEERCRED,12));report.setdefault('ipc_peers',[]).append({'connection':index,'pid':pid,'uid':uid,'gid':gid,'executable':os.readlink(f'/proc/{pid}/exe')});buffers={peer:b'',dest:b''}
    try:
        while not stop.is_set():
            for s in select.select([peer,dest],[],[],.1)[0]:
                b=s.recv(65536)
                if not b:return
                buffers[s]+=b
                while b'\n' in buffers[s]:
                    line,buffers[s]=buffers[s].split(b'\n',1);raw=json.loads(line);delivered=copy.deepcopy(raw)
                    event={'connection':index,'direction':'request' if s is peer else 'reply','monotonic_ns':time.monotonic_ns(),'raw':raw}
                    if s is dest and raw.get('navigation'):
                        event['copied_readback_before_gui']=copy_readback(raw['navigation']['snapshot_path'])
                        event['mutation']=mutation
                        if mutation=='original_path':delivered['navigation']['snapshot_path']=str(source)
                        if mutation=='stale_epoch':delivered['navigation']['film_epoch']='-1'
                        if mutation=='source_revision':delivered['navigation']['source_revision']='0'*64
                        if mutation=='action':delivered['navigation']['action']='reveal'
                        if mutation=='page':delivered['navigation']['page']=2
                        if mutation=='nullable_zero':delivered['navigation']['page']=0
                    event['delivered']=delivered;report['events'].append(event);(dest if s is peer else peer).sendall(json.dumps(delivered).encode()+b'\n')
    except OSError:pass
    finally:peer.close();dest.close();live_connections.pop(index,None)
def accept():
    i=0
    while not stop.is_set():
        try:peer,_=listener.accept()
        except socket.timeout:continue
        except OSError:return
        t=threading.Thread(target=relay,args=(peer,i));threads.append(t);t.start();i+=1

def normalized_screen(raw):
    # This ASCII fixture has no trailing spaces and exactly one final LF. Keep interior blank lines.
    return ('\n'.join(line.rstrip(' ') for line in raw.replace('\r\n','\n').replace('\r','\n').split('\n')).strip('\n')+'\n').encode('ascii')
known_receivers={}
def require_receiver_environment(receipt):
    env=receipt.get('environment',{})
    assert env.get('WAYLAND_DISPLAY')=='semanticlens-test' and env.get('DISPLAY') is None and env.get('XDG_RUNTIME_DIR')=='/tmp/test/runtime' and env.get('HOME')=='/tmp/test/home' and env.get('XDG_CONFIG_HOME')=='/tmp/test/config' and env.get('XDG_CACHE_HOME')=='/tmp/test/cache' and env.get('XDG_DATA_HOME')=='/tmp/test/data' and str(env.get('DBUS_SESSION_BUS_ADDRESS','')).startswith('unix:path=/tmp/dbus-') and receipt.get('platform_name')=='wayland',('receiver on wrong display',env)

def native_receipt(pid,name,capture=False):
    directory=O/'receivers'/str(pid);r=command(directory,name,'capture' if capture else 'snapshot');require_receiver_environment(r);r['actual_process']={'pid':pid,'exe':os.readlink(f'/proc/{pid}/exe'),'argv':Path(f'/proc/{pid}/cmdline').read_bytes().replace(b'\0',b' ').decode(),'startticks':Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()[19],'loaded_konsole_maps':[line for line in Path(f'/proc/{pid}/maps').read_text().splitlines() if any(v in line for v in ['libkonsole','receiver-observer.so','libOkular','okularpart.so','pdf-observer.so'])]};known_receivers[pid]=r['actual_process'];(directory/(name+'.json')).write_text(json.dumps(r,indent=2)+'\n');return r

def event_mark():return {'event_index':len(report['events']),'monotonic_ns':time.monotonic_ns()}
def latest_navigation(marker):
    pair=navigation_pair(report['events'],marker,selected_context)
    if not pair:return None
    event=pair['reply'];event['current_action_request']=pair['request'];event['action_interval_start']=marker
    issued_tokens[event['raw']['navigation']['token']]=event;return event

def release_receipt(token):
    issue=issued_tokens.get(token)
    if not issue:return False
    ids={(e['connection'],e['raw']['id']) for e in report['events'] if e['direction']=='request' and e['connection']==issue['connection'] and e['monotonic_ns']>=issue['monotonic_ns'] and e['raw'].get('op')=='film_release_navigation' and e['raw'].get('token')==token}
    return any(e['direction']=='reply' and (e['connection'],e['raw']['id']) in ids and e['raw'].get('ok') is True and e['raw'].get('released') is True for e in report['events'])

def evaluator_countercase(label,original,derived,accepted):
    record={'scope':'derived serialized evaluator contradiction; not observed altered native state','label':label,'original':original,'derived':derived,'accepted':bool(accepted)}
    report.setdefault('evaluator_countercases',[]).append(record);assert not accepted,label

def search_select(query,film_id):
    global selected_context
    marker=event_mark();input_receipt=action('filmSearchField','input',text=query)
    def ready():
        snapshot=snap();pair=search_pair(report['events'],marker,query,film_id,state(snapshot))
        return (snapshot,pair) if pair else None
    snapshot,pair=wait(ready,30);row=state(snapshot)['results'][0];index=0
    target=next(n for n in snapshot['nodes'] if n.get('modelData')==row and n.get('index')==index and n.get('class','').startswith('ItemDelegate') and n.get('geometry_intersects_viewport'))
    clicked=app_command('select-'+str(time.monotonic_ns()),'click',node=target['node']);assert clicked['input_dispatched']
    def selected():
        post=snap();context=selection_context(post,row,index);return (post,context) if context else None
    post,context=wait(selected,5);context['connection']=pair['request']['connection'];assert any(peer['connection']==context['connection'] and peer['pid']==app.pid for peer in report['ipc_peers']);selected_context=context
    report.setdefault('search_selection_phases',[]).append({'interval':marker,'input':input_receipt,'fresh_pair':pair,'row_click':clicked,'selected_snapshot':post,'selected_context':context})
    historical_marker={'event_index':len(report['events']),'monotonic_ns':time.monotonic_ns()}
    evaluator_countercase('historical-only-search',{'pair':pair,'native':state(post)},{'after_history_interval':historical_marker},search_pair(report['events'],historical_marker,query,film_id,state(post)) is not None)
    return post

def reveal():
    context=copy.deepcopy(selected_context);before=snap();assert selection_context(before,context['row'],context['index']) is not None
    button=next(n for n in before['nodes'] if n.get('name')=='filmEvidenceReveal' and n.get('modelData',{}).get('reference')==context['reference'] and n.get('geometry_intersects_viewport') and n.get('enabled',True))
    marker=event_mark();clicked=app_command('reveal-'+str(time.monotonic_ns()),'click',node=button['node']);assert clicked['input_dispatched']
    def revealed():
        snapshot=snap();pair=reveal_pair(report['events'],marker,context,state(snapshot))
        return (snapshot,pair) if pair and selection_context(snapshot,context['row'],context['index']) and clickable(snapshot,'filmOpenSource') else None
    snapshot,pair=wait(revealed,15)
    selected_context['current_reveal_pair']=pair
    report.setdefault('reveal_phases',[]).append({'interval':marker,'input':clicked,'selected_context':context,'fresh_pair':pair,'native_snapshot':snapshot})
    after={'event_index':len(report['events']),'monotonic_ns':time.monotonic_ns()}
    evaluator_countercase('historical-only-reveal',pair,after,reveal_pair(report['events'],after,context,state(snapshot)) is not None)
    other=next((entry['reference'] for row in state(snapshot)['results'] for entry in row.get('evidence',[]) if entry.get('reference') and entry['reference']!=context['reference']),None)
    assert other,'actual alternate emitted reference required for selected-reference countercase'
    wrong_pair=copy.deepcopy(pair);wrong_pair['request']['raw']['reference']=other;wrong_pair['request']['delivered']['reference']=other
    wrong_pair['reply']['raw']['evidence']['reference']=other;wrong_pair['reply']['delivered']['evidence']['reference']=other
    wrong_events=[wrong_pair['request'],wrong_pair['reply']];wrong_marker={'event_index':0,'monotonic_ns':wrong_pair['request']['monotonic_ns']}
    evaluator_countercase('wrong-selected-reference-reveal',pair,{'events':wrong_events,'alternate_actual_emitted_reference':other},reveal_pair(wrong_events,wrong_marker,context,state(snapshot)) is not None)
    return snapshot

def open_source():
    snapshot=snap();context=selected_context
    assert selection_context(snapshot,context['row'],context['index']) is not None
    pair=context['current_reveal_pair'];assert pair['reply']['raw']['evidence']==state(snapshot)['evidence']
    assert pair['reply']['raw']['film_revision']==context['film_revision'] and pair['reply']['raw']['film_epoch']==context['film_epoch']
    marker=event_mark();clicked=action('filmOpenSource');report.setdefault('open_action_phases',[]).append({'interval':marker,'input':clicked,'selected_context':copy.deepcopy(context),'native_before':snapshot});return marker

def open_api_count():
    path=O/'app-observer/open-events.jsonl';return len(path.read_text().splitlines()) if path.exists() else 0

def pdf_source_text(page):
    r=subprocess.run(['/usr/bin/pdftotext','-f',str(page),'-l',str(page),'-layout',str(pdf),'-'],capture_output=True,timeout=5)
    assert r.returncode==0
    raw=r.stdout.decode();(O/('source-page-'+str(page)+'.txt')).write_text(raw);return raw

def pdf_native(pid,expected_index,expected_path,name,expected_text,negative_text):
    # Observe natural/exposed owning phase WITHOUT capture before any grab is requested.
    pre=native_receipt(pid,name+'-natural-phase',False)
    if not pdf_phase_views(pre,expected_index,expected_path):return None
    receipt=native_receipt(pid,name,True)
    eligible={v['object'] for v in pdf_phase_views(receipt,expected_index,expected_path)}
    for capture in receipt['captures']:
        if capture['view'] not in eligible or not capture.get('capture_saved'):continue
        path=O/'receivers'/str(pid)/capture['capture_file'];ocr=subprocess.run(['/usr/bin/tesseract',str(path),'stdout','--psm','6'],capture_output=True,timeout=10)
        text=ocr.stdout.decode();path.with_suffix('.ocr.txt').write_text(text);path.with_suffix('.ocr.stderr').write_bytes(ocr.stderr)
        capture['ocr_exit']=ocr.returncode;capture['ocr_text']=text
    matched=pdf_content_view(receipt,expected_index,expected_path,expected_text,negative_text)
    receipt['source_derived_expected_text']=expected_text;receipt['source_derived_contradictory_text']=negative_text;receipt['natural_phase_receipt']=name+'-natural-phase';receipt['matched_view']=matched['view'] if matched else None
    (O/'receivers'/str(pid)/(name+'.json')).write_text(json.dumps(receipt,indent=2)+'\n')
    if not matched:return None
    contradictory=copy.deepcopy(receipt)
    for window in contradictory['pre_capture_phase']['windows']:window['window_exposed']=False
    for view in contradictory['pre_capture_phase']['pageviews']:
        view['window_exposed']=False;view['natural_paint_events']=0
        view['last_natural_paint_phase']['window_exposed']=False
        if view.get('first_capture_pre_phase'):
            view['first_capture_pre_phase']['natural_paint_events']=0;view['first_capture_pre_phase']['window_exposed']=False
    for capture in contradictory['captures']:capture['capture_paint_events_delta']=max(1,capture.get('capture_paint_events_delta',0))
    evaluator_countercase('unexposed-capture-only-PDF',{'receipt':name,'pid':pid,'matched_view':matched['view']},contradictory,pdf_content_view(contradictory,expected_index,expected_path,expected_text,negative_text) is not None)
    return receipt

def pdf_open(page1_text,page2_text):
    search_select('PARC-20942','film:parcel-pdf');s=reveal();check('actual PDF reveal page2',state(s)['evidence'].get('page')==2)
    before=set(d.name for d in (O/'receivers').iterdir());start=open_source();nav=wait(lambda:latest_navigation(start),10);n=nav['raw']['navigation'];path=n['snapshot_path'];token=n['token']
    check('PDF copied bytes and page intent',type(n['page']) is int and n['page']==2 and nav['copied_readback_before_gui']['sha256']==originals[str(pdf)]['sha256'] and nav['copied_readback_before_gui']['mode']=='0o400' and nav['copied_readback_before_gui']['directory_mode']=='0o700')
    def rendered():
        for d in (O/'receivers').iterdir():
            if d.name in before or not (d/'identity.json').exists():continue
            r=pdf_native(int(d.name),1,path,'page2-'+str(time.monotonic_ns()),page2_text,page1_text)
            if r:return r
        return None
    r=wait(rendered,25);check('real Okular binary and --page2 copy argv',r['actual_process']['exe']=='/usr/bin/okular' and '--page 2' in r['actual_process']['argv'] and path in r['actual_process']['argv'] and str(pdf) not in r['actual_process']['argv'])
    check('PDF token held through native page2 readback',Path(path).exists() and sha(path)==originals[str(pdf)]['sha256'] and not release_receipt(token));report['pdf']={'page2_verified':True,'navigation':nav,'native_receipt':r,'copy_at_receipt':copy_readback(path)}
    action('filmSearchField','input',text='They erased my name from the rescue report.');wait(lambda:release_receipt(token) and not Path(path).exists(),10);report['pdf']['copy_after_selection']=copy_readback(path)

try:
    r=subprocess.run(['/usr/bin/update-desktop-database',str(apps)],capture_output=True,timeout=10);check('owned mime cache',r.returncode==0)
    g=subprocess.run(['/tmp/test/work/gio-inspect'],capture_output=True,timeout=10);report['gio']=json.loads(g.stdout);check('real owned handler recommendation',g.returncode==0 and report['gio']['default']=='film-source-consumer.desktop' and [x['id'] for x in report['gio']['choices']]==['film-source-consumer.desktop'])
    portal=subprocess.run(['/usr/bin/gdbus','call','--session','--dest','org.freedesktop.portal.Desktop','--object-path','/org/freedesktop/portal/desktop','--method','org.freedesktop.DBus.Properties.Get','org.freedesktop.portal.OpenURI','version'],capture_output=True,timeout=15)
    report['dedicated_portal_preflight']={'exit':portal.returncode,'stdout':portal.stdout.decode(),'stderr':portal.stderr.decode()}
    check('private KDE OpenURI5 available',portal.returncode==0 and 'uint32 5' in portal.stdout.decode())
    # Fresh native B loaded calibration. Expected body is never given to receiver observer.
    contrast=O/'contrast.txt';contrast.write_bytes(b'Violet rails hold calm beacons 78162044.\n');contrast.chmod(0o444)
    cal=spawn(['/usr/bin/konsole','--separate','--nofork','--hold','-e','/usr/bin/cat',str(contrast)],'calibration',{})
    wait(lambda:(O/'receivers'/str(cal.pid)/'identity.json').exists(),10)
    def calibrated():
        r=native_receipt(cal.pid,'calibration-'+str(time.monotonic_ns()))
        return r if any(normalized_screen(t['actual_native_text'])==contrast.read_bytes() for t in r['terminals'] if t.get('text_interface_present') and t.get('accessible_valid')) else None
    c=wait(calibrated,15);report['calibration']=c;check('contrast actual native body rejects Harbor',all(source.read_bytes()!=normalized_screen(t['actual_native_text']) for t in c['terminals'] if t.get('text_interface_present')));native_receipt(cal.pid,'calibration-capture',True);cal.terminate();cal.wait(timeout=5)
    page1_text=pdf_source_text(1);page2_text=pdf_source_text(2)
    pdf_cal=spawn(['/usr/bin/okular','--page','1',str(pdf)],'pdf-calibration',{'LD_PRELOAD':'/tmp/test/work/pdf-observer.so'})
    wait(lambda:(O/'receivers'/str(pdf_cal.pid)/'identity.json').exists(),10)
    report['pdf_calibration']=wait(lambda:pdf_native(pdf_cal.pid,0,str(pdf),'page1-calibration-'+str(time.monotonic_ns()),page1_text,page2_text),20)
    pdf_cal.terminate();pdf_cal.wait(timeout=5)
    service_argv=['/tmp/test/stage/bin/kde-index-service','--socket','/tmp/test/work/service.sock','--data-dir','/tmp/test/work/index','--probe','/tmp/test/stage/bin/embedding-probe','--probe-sha256',sha('/tmp/test/stage/bin/embedding-probe'),'--text-model','/tmp/test/models/embeddinggemma-2-Q8_0.gguf','--image-model','/tmp/test/models/mmproj-embeddinggemma-2-Q8_0.gguf','--threads','2','--film-tokenizer','/tmp/test/stage/bin/semanticlens-film-tokenizer','--film-tokenizer-sha256',sha('/tmp/test/stage/bin/semanticlens-film-tokenizer')]
    service_argv+=film_worker_flags(a.film_worker,a.film_worker_sha256)
    report['service_launch']={'argv':service_argv,'film_worker_path':str(a.film_worker) if a.film_worker else None,'film_worker_sha256':a.film_worker_sha256}
    service=spawn(service_argv,'service')
    wait(lambda:(O/'service.sock').exists(),25);client=socket.socket(socket.AF_UNIX);client.settimeout(40);client.connect('/tmp/test/work/service.sock');wait(lambda:req('status')['status']['ready'],35)
    cid=req('film_add_collection',path=str(O/'corpus'))['collection_id']
    for rel in ['harbor/manifest.json','parcel-pdf/manifest.json']:
        jid=req('film_import_manifest',collection_id=cid,relative_path=rel,expected_sha256=sha(O/'corpus'/rel))['job_id'];job=wait(lambda:(j if (j:=req('film_status',job_id=jid)['status']['jobs'][0])['state'] in ['complete','failed','interrupted','canceled'] else None),25);check('source import '+rel,job['state']=='complete')
    listener=socket.socket(socket.AF_UNIX);listener.bind('/tmp/test/work/relay.sock');listener.listen();listener.settimeout(.1);at=threading.Thread(target=accept);at.start()
    monitor=spawn(['/usr/bin/dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'","interface='org.freedesktop.impl.portal.AppChooser'","interface='org.freedesktop.portal.Request'","type='method_return'","type='error'"],'portal-monitor')
    wait(lambda:'NameAcquired' in (O/'portal-monitor.log').read_text(errors='replace'),5)
    app=spawn(['/tmp/test/stage/bin/kde-index','--socket','/tmp/test/work/relay.sock','--models','/tmp/test/models','--data-dir','/tmp/test/work/index','--film-view'],'app',{'LD_PRELOAD':'/tmp/test/work/app-observer.so:/tmp/test/work/pdf-observer.so','DESKTOP_LAYOUT_OUT':'/tmp/test/work/app-observer'})
    wait(lambda:state(snap())['available'],15);search_select('They erased my name from the rescue report.','film:harbor');before=snap();report['hidden_before_reveal']=before;check('no source open before actual reveal',not state(before)['evidence'] and not clickable(before,'filmOpenSource'));report['revealed_phase']=reveal();app_command('revealed-capture','capture')
    for branch in ['original_path','stale_epoch','source_revision','action','page','nullable_zero']:
        mutation=branch;before_dirs=set(x.name for x in (O/'receivers').iterdir());before_open_calls=open_api_count();start=open_source();nav=wait(lambda:latest_navigation(start),10);token=nav['raw']['navigation']['token'];path=nav['raw']['navigation']['snapshot_path'];check('same producer source page-null '+branch,'page' in nav['raw']['navigation'] and nav['raw']['navigation']['page'] is None);wait(lambda:release_receipt(token) and not Path(path).exists(),10);receipt=snap();check('contradictory raw '+branch+' rejects before receiver',not (set(x.name for x in (O/'receivers').iterdir())-before_dirs) and open_api_count()==before_open_calls);report.setdefault('negative_branches',[]).append({'branch':branch,'navigation':nav,'gui_after':receipt,'copied_path_removed':not Path(path).exists()})
    mutation=None
    for cleanup in ['selection_change','disconnect']:
        before_dirs=set(x.name for x in (O/'receivers').iterdir());start=open_source();nav=wait(lambda:latest_navigation(start),10);n=nav['raw']['navigation'];path=n['snapshot_path'];token=n['token'];check('normal source page is JSON null '+cleanup,'page' in n and n['page'] is None);check('source copy before open '+cleanup,nav['copied_readback_before_gui']['sha256']==originals[str(source)]['sha256'] and nav['copied_readback_before_gui']['mode']=='0o400' and nav['copied_readback_before_gui']['directory_mode']=='0o700')
        def received():
            for d in (O/'receivers').iterdir():
                if d.name in before_dirs or not (d/'identity.json').exists():continue
                r=native_receipt(int(d.name),'loaded-'+str(time.monotonic_ns()))
                if any(normalized_screen(t['actual_native_text'])==source.read_bytes() for t in r['terminals'] if t.get('accessible_valid') and t.get('text_interface_present')):return r
            return None
        receipt=wait(received,20);check('actual receiving executable/copy argv '+cleanup,receipt['actual_process']['exe']=='/usr/bin/konsole' and path in receipt['actual_process']['argv'] and str(source) not in receipt['actual_process']['argv']);check('token copy held through native receipt '+cleanup,Path(path).exists() and sha(path)==originals[str(source)]['sha256'] and not release_receipt(token));capture=native_receipt(receipt['pid'],'source-capture',True);app_command('open-capture-'+cleanup,'capture');report.setdefault('positive_branches',[]).append({'cleanup':cleanup,'navigation':nav,'receiver':receipt,'capture':capture,'copy_at_receipt':copy_readback(path)})
        if cleanup=='selection_change':
            action('filmSearchField','input',text='MARA: The people on the water still need someone.');wait(lambda:release_receipt(token) and not Path(path).exists(),10);pdf_open(page1_text,page2_text);search_select('They erased my name from the rescue report.','film:harbor');reveal()
        else:
            for index,(peer,dest) in list(live_connections.items()):
                # Only FilmBackend's connection issued this navigation; leave Files connection intact.
                if index==nav['connection']:
                    peer.shutdown(socket.SHUT_RDWR);dest.shutdown(socket.SHUT_RDWR)
            wait(lambda:not Path(path).exists(),10)
        report['positive_branches'][-1]['copy_after_cleanup']=copy_readback(path)
    report['open_api_events']=[json.loads(x) for x in (O/'app-observer/open-events.jsonl').read_text().splitlines()];report['portal_trace']=(O/'portal-monitor.log').read_text(errors='replace');check('real desktop portal OpenFile branch',"member=OpenFile" in report['portal_trace']);check('every actual Qt open uses copy not original',len(report['open_api_events'])==2 and all(e['local_file']!=str(source) for e in report['open_api_events']))
    blocks=re.findall(r'method call[^\n]*member=OpenFile\n(?P<body>.*?)(?=\n(?:method |signal |error )|\Z)',report['portal_trace'],re.S)
    portal_inodes=[int(m.group(1)) for block in blocks if (m:=re.search(r'inode: (\d+)',block))]
    expected_inodes=[v['navigation']['copied_readback_before_gui']['inode'] for v in report['positive_branches']]
    report['portal_fd_inodes']=portal_inodes;check('real portal descriptors match both copied inodes',portal_inodes==expected_inodes)
    check('fixture bytes unchanged',all(sha(f)==originals[str(f)]['sha256'] for f in [source,pdf]));report['text_open_qualified']=True;report['pdf_open_qualified']=True;report['passed']=True
    req('shutdown');service.wait(timeout=10)
except Exception as e:report['error']=repr(e);report['passed']=False
finally:
    stop.set()
    if listener:listener.close()
    for s in peers:
        try:s.shutdown(socket.SHUT_RDWR)
        except OSError:pass
    for t in threads:t.join(2)
    if 'at' in globals():at.join(2)
    if client:client.close()
    for r in reversed(owned):
        if r.poll() is None:r.terminate()
        try:r.wait(timeout=5)
        except subprocess.TimeoutExpired:r.kill();r.wait()
    receiver_closure=[]
    for pid,identity in known_receivers.items():
        path=Path('/proc')/str(pid)
        try:
            now={'exe':os.readlink(path/'exe'),'startticks':(path/'stat').read_text().rsplit(')',1)[1].split()[19]}
            if now['exe']!=identity['exe'] or now['startticks']!=identity['startticks']:
                receiver_closure.append({'pid':pid,'status':'identity_changed_no_signal'});continue
            os.kill(pid,signal.SIGTERM);deadline=time.monotonic()+5
            while path.exists() and time.monotonic()<deadline:time.sleep(.05)
            if path.exists():os.kill(pid,signal.SIGKILL)
            receiver_closure.append({'pid':pid,'status':'owned_receiver_signaled','identity':now})
        except OSError:receiver_closure.append({'pid':pid,'status':'already_closed'})
    # Leave private KWin/private bus to their owning outer --exit-with-session lifecycle.
    extras=[]
    for path in Path('/proc').iterdir():
        if path.name.isdigit():
            try:extras.append({'pid':int(path.name),'argv':(path/'cmdline').read_bytes().replace(b'\0',b' ').decode()})
            except OSError:pass
    for f in logs:f.close()
    for name in ['service.sock','relay.sock']:
        if (O/name).exists():(O/name).unlink()
    report['closure']={'owned_exits':[{'pid':r.pid,'exit':r.returncode} for r in owned],'relay_threads_closed':all(not t.is_alive() for t in threads),'accept_thread_closed':'at' not in globals() or not at.is_alive(),'receiver_closure':receiver_closure,'namespace_processes_pending_outer_exit':extras,'socket_paths_absent':not any((O/n).exists() for n in ['service.sock','relay.sock']),'copy_paths_remaining':[str(x) for x in (O/'index').glob('film-extract-*/input.*')]}
    if report.get('passed') and not (report['closure']['relay_threads_closed'] and report['closure']['accept_thread_closed'] and report['closure']['socket_paths_absent'] and not report['closure']['copy_paths_remaining']):report['passed']=False;report['error']='resource closure predicate failed'
    (O/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report.get(k) for k in ['passed','text_open_qualified','pdf','error','closure']}));raise SystemExit(0 if report.get('passed') else 1)
