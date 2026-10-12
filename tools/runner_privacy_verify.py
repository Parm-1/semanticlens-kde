#!/usr/bin/env python3
"""Focused display contract checks against raw native RunnerManager receipts."""
import json
import sys
from pathlib import Path
rows = [json.loads(line) for line in Path(sys.argv[1]).read_text().splitlines()]
assert len(rows) == 11 and len({r['case'] for r in rows}) == 11
by = {r['case']: r for r in rows}
expected = {
    'normal-id': ('notes-IDENTIFIER-77331.txt', 'IDENTIFIER-77331 ordinary passage'),
    'title-tag': ('＜b＞TITLE-77331.txt', 'normal passage'),
    'passage-image': ('passage.txt', 'DESKTOP-77331 ＜img src="http://127.0.0.1:43573/passage" /＞ tail'),
    'doctype': ('doctype.txt', '＜!DOCTYPE html＞＜img src=x＞'),
    'xml': ('xml.txt', "＜?xml version='1.0'?＞＜img src=x＞"),
    'literal-entities': ('＆lt;entity＆gt;.txt', '＆lt;img src=x＆gt; ＆amp; ＆#60;b＆#62;'),
    'ordinary-ampersands': ('R＆D A ＆ B.txt', 'R＆D A ＆ B ＆＆ x＆'),
    'pdf': ('PDF ＜b＞＆.pdf', 'Page 3 · ＜img src=x＞ page passage ＆lt;'),
    'path-fallback': ('image.png', '/tmp/＜b＞＆lt;image＆gt;.png'),
    'error': ('KDE Index service could not complete the search', 'cannot decode ＜img src=x＞ ＆lt;source＆gt; R＆D'),
    'clipping': ('title ' + 'x' * 153 + '…', '＜img src=x＞ ' + 'y' * 227 + '…'),
}
for name, row in by.items():
    assert row['qt_version'] == '6.12.0', row['qt_version']
    assert row['request']['op'] == 'search' and row['request']['text'] == 'IDENTIFIER-77331' and row['request']['limit'] == 12
    assert row['peer_observed_close'] is True
    assert len(row['matches']) == 1, name
    match = row['matches'][0]
    assert (match['title']['text'], match['subtext']['text']) == expected[name], (name, match)
    for field in ('title', 'subtext'):
        c = match[field]
        assert c['mightBeRichText'] is False and c['stock_tooltip_mightBeRichText'] is False, (name, c)
        assert c['stock_tooltip_text'] == c['text'], (name, c)
        assert not any(x in c['text'] for x in '<>&'), (name, c)
    if name == 'error':
        assert match['data'] == {'setup': False} and match['urls'] == []
        assert match['relevance'] == 0.2
    else:
        raw = row['response']['results'][0]
        assert match['data'] == {k: raw[k] for k in ('path', 'kind', 'page')}
        assert len(match['urls']) == 1 and match['urls'][0]['local_file'] == raw['path']
        assert match['id'].endswith(raw['path'] + '#' + str(raw['page']))
        assert match['relevance'] == 0.625
for name, field in [('title-tag','raw_title'), ('passage-image','raw_passage'), ('doctype','raw_passage'),
                    ('xml','raw_passage'), ('literal-entities','raw_title'), ('literal-entities','raw_passage'),
                    ('pdf','raw_title'), ('pdf','raw_passage'), ('path-fallback','raw_path'), ('error','raw_passage'),
                    ('clipping','raw_passage')]:
    assert by[name][field]['mightBeRichText'] is True, (name, field)
assert by['ordinary-ampersands']['raw_title']['mightBeRichText'] is False
assert by['ordinary-ampersands']['raw_title']['stock_tooltip_text'] == 'RD A & B.txt'
assert by['literal-entities']['raw_title']['stock_tooltip_text'] == 'lt;entitygt;.txt'
print('PASS: 11 native fixture cases; 22 display fields; 10 original source URLs/action maps; 11 raw-rich controls; ordinary ampersand and entity readability controls; all peers closed')
