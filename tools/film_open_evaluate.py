"""Read-only predicates for fresh RPC/selection and owned PDF pre-capture phases.
No native state or pixels are produced here. Runtime evidence comes from the actual observers.
"""
def fresh_pairs(events, marker, op, arguments):
    requests={}
    for event in events[marker['event_index']:]:
        if event.get('monotonic_ns',0)<marker['monotonic_ns']:continue
        raw=event.get('raw',{})
        key=(event.get('connection'),raw.get('id'))
        if event.get('direction')=='request' and raw.get('op')==op and all(raw.get(k)==v for k,v in arguments.items()):
            requests[key]=event
        elif event.get('direction')=='reply' and key in requests:
            request=requests[key]
            if raw.get('protocol')==1 and raw.get('ok') is True and event['monotonic_ns']>=request['monotonic_ns']:
                yield {'request':request,'reply':event}

def search_pair(events,marker,query,film_id,native):
    rows=native.get('results',[])
    if native.get('searching') or native.get('query')!=query or native.get('mode')!='hybrid' or native.get('includeUnverifiedPrior') is not False:return None
    if not rows or rows[0].get('film_id')!=film_id:return None
    for pair in fresh_pairs(events,marker,'film_search',{'text':query,'mode':'hybrid','include_unverified_prior':False}):
        event=pair['reply'];raw=event['raw']
        if event.get('delivered')!=raw or raw.get('results')!=rows or raw.get('include_unverified_prior') is not False:continue
        if not raw.get('film_revision') or not raw.get('film_epoch'):continue
        if all(row.get('film_revision')==raw['film_revision'] and row.get('film_epoch')==raw['film_epoch'] for row in rows):return pair
    return None

def selection_context(snapshot,expected_row,index):
    nodes=snapshot.get('nodes',[])
    page=next((n for n in nodes if n.get('name')=='filmPage'),{})
    view=next((n for n in nodes if n.get('name')=='filmResults'),{})
    if page.get('selectedIndex')!=index or view.get('currentIndex')!=index or page.get('selectedResult')!=expected_row:return None
    selected=next((n for n in nodes if n.get('class','').startswith('ItemDelegate') and n.get('index')==index and n.get('modelData')==expected_row and n.get('highlighted') is True),None)
    evidence=expected_row.get('evidence',[])
    if not selected or not evidence or not isinstance(evidence[0].get('reference'),dict):return None
    reference=evidence[0]['reference']
    reveal=next((n for n in nodes if n.get('name')=='filmEvidenceReveal' and n.get('modelData',{}).get('reference')==reference and n.get('geometry_intersects_viewport') and n.get('enabled',True)),None)
    if not reveal:return None
    return {'reference':reference,'row':expected_row,'index':index,'film_revision':expected_row['film_revision'],'film_epoch':expected_row['film_epoch'],'reveal_node':reveal['node'],'selected_node':selected['node']}

def reveal_pair(events,marker,context,native):
    evidence=native.get('evidence',{})
    if evidence.get('reference')!=context['reference'] or evidence.get('reference_kind')!='source' or not evidence.get('source_excerpt'):return None
    for pair in fresh_pairs(events,marker,'film_evidence',{'reference':context['reference'],'expected_film_revision':context['film_revision'],'spoiler_policy':'allow'}):
        if pair['request']['connection']!=context.get('connection'):continue
        raw=pair['reply']['raw']
        if pair['reply'].get('delivered')==raw and raw.get('film_revision')==context['film_revision'] and raw.get('film_epoch')==context['film_epoch'] and raw.get('evidence')==evidence:return pair
    return None

def navigation_pair(events,marker,context):
    for pair in fresh_pairs(events,marker,'film_navigation',{'reference':context['reference'],'expected_film_revision':context['film_revision'],'spoiler_policy':'allow','action':'open'}):
        if pair['request']['connection']!=context.get('connection'):continue
        nav=pair['reply']['raw'].get('navigation',{})
        if nav.get('film_revision')==context['film_revision'] and nav.get('film_epoch')==context['film_epoch'] and nav.get('source_revision')==context['reference'].get('source_revision') and nav.get('action')=='open':return pair
    return None

def pdf_phase_views(receipt,expected_index,expected_path):
    phase=receipt.get('pre_capture_phase',{})
    documents={d.get('object'):d for d in phase.get('documents',[])}
    windows={w.get('object'):w for w in phase.get('windows',[])}
    eligible=[]
    for view in phase.get('pageviews',[]):
        doc=documents.get(view.get('document'),{});window=windows.get(view.get('owner_window'),{})
        if not(view.get('owner_widget')==doc.get('owner_widget') and view.get('owner_window')==doc.get('owner_window')==window.get('object')):continue
        if not(window.get('visible') is True and window.get('window_exposed') is True and view.get('visible') is True and view.get('canvas_visible') is True and view.get('natural_paint_events',0)>0):continue
        if not(doc.get('is_opened') and doc.get('page_count')==2 and doc.get('current_page_index_zero_based')==expected_index and doc.get('local_file')==expected_path and any(v.get('page_index_zero_based')==expected_index for v in doc.get('visible_page_rects',[]))):continue
        natural=view.get('last_natural_paint_phase',{})
        if not(natural.get('document')==doc.get('object') and natural.get('owner_window')==window.get('object') and natural.get('local_file')==expected_path and natural.get('current_page_index_zero_based')==expected_index and natural.get('page_count')==2 and natural.get('window_exposed') is True and natural.get('canvas_visible') is True):continue
        first=view.get('first_capture_pre_phase')
        if first is not None and not(first.get('natural_paint_events',0)>0 and first.get('window_exposed') is True and first.get('canvas_visible') is True and first.get('document')==view.get('document') and first.get('owner_window')==view.get('owner_window')):continue
        eligible.append(view)
    return eligible

def pdf_content_view(receipt,expected_index,expected_path,expected_text,opposite_text):
    eligible={view['object']:view for view in pdf_phase_views(receipt,expected_index,expected_path)}
    expected=[' '.join(line.split()) for line in expected_text.replace('\f','').splitlines() if line.strip()]
    opposite=[' '.join(line.split()) for line in opposite_text.replace('\f','').splitlines() if line.strip()]
    if not expected or not opposite:return None
    for capture in receipt.get('captures',[]):
        view=eligible.get(capture.get('view'))
        if not view or capture.get('document')!=view.get('document') or capture.get('owner_window')!=view.get('owner_window'):continue
        if capture.get('capture_saved') is not True or (type(capture.get('ocr_exit')) is not int or capture.get('ocr_exit')!=0):continue
        text=' '.join(capture.get('ocr_text','').split())
        if all(line in text for line in expected) and all(line not in text for line in opposite):return capture
    return None
