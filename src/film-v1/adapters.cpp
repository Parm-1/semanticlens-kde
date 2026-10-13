#include "adapters.h"

#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <stdexcept>

namespace FilmAdapters {
namespace {
constexpr qsizetype MaxBytes = 2 * 1024 * 1024;
constexpr qsizetype MaxUnits = 10000;
constexpr qsizetype MaxUnitBytes = 64 * 1024;
constexpr auto Norm = "semanticlens-film-units/1";

[[noreturn]] void fail(const QString &why) { throw std::invalid_argument(why.toUtf8().constData()); }

struct SrcChar { uint cp; qsizetype begin; qsizetype end; };

QVector<SrcChar> decode(const QByteArray &b, qsizetype from, qsizetype to)
{
    QVector<SrcChar> out;
    for (qsizetype i = from; i < to;) {
        const auto c = uchar(b.at(i));
        uint cp = 0; qsizetype n = 0;
        if (c < 0x80) { cp = c; n = 1; }
        else if (c >= 0xc2 && c <= 0xdf) { cp = c & 0x1f; n = 2; }
        else if (c >= 0xe0 && c <= 0xef) { cp = c & 0x0f; n = 3; }
        else if (c >= 0xf0 && c <= 0xf4) { cp = c & 0x07; n = 4; }
        else fail(QStringLiteral("Invalid UTF-8 at byte %1").arg(i));
        if (i + n > to) fail(QStringLiteral("Truncated UTF-8 at byte %1").arg(i));
        for (qsizetype j = 1; j < n; ++j) {
            const auto d = uchar(b.at(i + j));
            if ((d & 0xc0) != 0x80) fail(QStringLiteral("Invalid UTF-8 continuation at byte %1").arg(i + j));
            cp = (cp << 6) | (d & 0x3f);
        }
        if ((n == 3 && ((c == 0xe0 && uchar(b.at(i+1)) < 0xa0) || (c == 0xed && uchar(b.at(i+1)) >= 0xa0))) ||
            (n == 4 && ((c == 0xf0 && uchar(b.at(i+1)) < 0x90) || (c == 0xf4 && uchar(b.at(i+1)) >= 0x90))))
            fail(QStringLiteral("Non-scalar or overlong UTF-8 at byte %1").arg(i));
        out.push_back({cp, i, i+n}); i += n;
    }
    return out;
}

QString makeText(const QVector<SrcChar> &chars)
{
    QString s;
    for (const auto &c : chars) { const char32_t cp=char32_t(c.cp); s.append(QString::fromUcs4(&cp, 1)); }
    return s;
}

QJsonArray spans(const QVector<SrcChar> &chars)
{
    QJsonArray a;
    for (const auto &c : chars) a.append(QJsonArray{double(c.begin), double(c.end)});
    return a;
}

QJsonObject unit(const QString &id, const QString &text, const QString &kind,
                 const QString &sourceFormat, const QByteArray &bytes,
                 qsizetype rawStart, qsizetype rawEnd, const QVector<SrcChar> &map)
{
    if (rawEnd - rawStart > MaxUnitBytes) fail(QStringLiteral("Unit exceeds 64 KiB limit"));
    QJsonObject o{{"unit_id", id}, {"text", text}, {"kind", kind},
                  {"source_format", sourceFormat},
                  {"raw_text", QString::fromUtf8(bytes.constData()+rawStart, rawEnd-rawStart)},
                  {"raw_start_byte", double(rawStart)}, {"raw_end_byte", double(rawEnd)},
                  {"canonical_codepoint_raw_spans", spans(map)},
                  {"raw_byte_source", "input_utf8"},
                  {"normalization_version", Norm}};
    return o;
}

QString classify(const QString &s)
{
    static const QRegularExpression heading(QStringLiteral("^(INT\\.|EXT\\.|INT/EXT\\.|I/E\\.)\\s*.+$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression speaker(QStringLiteral("^[A-Z][A-Z0-9 ._'’-]{0,49}:\\s*.+$"));
    if (speaker.match(s).hasMatch()) return QStringLiteral("dialogue");
    if ((s.startsWith('(') && s.endsWith(')')) || (s.startsWith('[') && s.endsWith(']')))
        return QStringLiteral("stage_direction");
    return QStringLiteral("text");
}

bool isSceneHeading(const QString &s)
{
    static const QRegularExpression heading(QStringLiteral("^(INT\\.|EXT\\.|INT/EXT\\.|I/E\\.)\\s*.+$"), QRegularExpression::CaseInsensitiveOption);
    return heading.match(s).hasMatch();
}

struct Line { qsizetype start, end, next; int number; };
QVector<Line> lines(const QByteArray &b, qsizetype start, qsizetype limit=-1, bool formFeed=false)
{
    if(limit<0) limit=b.size();
    QVector<Line> result; qsizetype p=start; int n=1;
    while (p < limit) {
        qsizetype e=p; while (e < limit && b.at(e)!='\n' && b.at(e)!='\r' && (!formFeed || b.at(e)!='\f')) ++e;
        qsizetype next=e;
        const bool pageBreak=formFeed && next<limit && b.at(next)=='\f';
        if (next < b.size() && b.at(next)=='\r') ++next;
        if (next < b.size() && b.at(next)=='\n') ++next;
        if (pageBreak) ++next;
        result.push_back({p,e,next,n++}); p=next;
    }
    if (p==limit && (limit==start || (limit>start && (b.at(limit-1)=='\n' || b.at(limit-1)=='\r'))))
        result.push_back({p,p,p,n});
    return result;
}

qint64 timestamp(const QByteArray &s, bool vtt)
{
    constexpr qint64 MaxExactJsonInteger = 9007199254740991LL;
    const QString pattern = vtt
      ? QStringLiteral("^(\\d{2,}:)?([0-5]?\\d):([0-5]?\\d)\\.(\\d{3})$")
      : QStringLiteral("^(\\d{2}):([0-5]\\d):([0-5]\\d),(\\d{3})$");
    const auto m=QRegularExpression(pattern).match(QString::fromLatin1(s));
    if (!m.hasMatch()) fail(QStringLiteral("Malformed %1 timestamp: %2").arg(vtt?"VTT":"SRT", QString::fromLatin1(s)));
    qint64 h=0, min=0, sec=0, ms=0;
    if (vtt) {
        QString first=m.captured(1);
        if (first.endsWith(':')) first.chop(1);
        bool ok=true;
        h=first.isEmpty()?0:first.toLongLong(&ok);
        if(!ok) fail(QStringLiteral("VTT hour field is out of range"));
        min=m.captured(2).toLongLong(); sec=m.captured(3).toLongLong(); ms=m.captured(4).toLongLong();
    } else { h=m.captured(1).toLongLong(); min=m.captured(2).toLongLong(); sec=m.captured(3).toLongLong(); ms=m.captured(4).toLongLong(); }
    const qint64 remainderMs=((min*60+sec)*1000+ms);
    if(h>(MaxExactJsonInteger-remainderMs)/3600000)
        fail(QStringLiteral("Timestamp exceeds exact JSON integer range (2^53-1 ms)"));
    return h*3600000+remainderMs;
}

QVector<SrcChar> visibleVtt(const QByteArray &b, qsizetype a, qsizetype z)
{
    const auto src=decode(b,a,z); QVector<SrcChar> out;
    for (qsizetype i=0; i<src.size();) {
        if (src[i].cp=='<') {
            qsizetype j=i+1; while (j<src.size() && src[j].cp!='>') ++j;
            if (j==src.size()) fail(QStringLiteral("Unclosed VTT markup tag"));
            QString tag; for (qsizetype k=i+1;k<j;++k) tag.append(QChar(src[k].cp));
            static const QRegularExpression supported(QStringLiteral("^/?(b|i|u|c(?:\\.[A-Za-z0-9_-]+)*|v(?:\\s+[^<>]+)?|lang(?:\\s+[A-Za-z0-9-]+)?)$"));
            if (!supported.match(tag).hasMatch()) fail(QStringLiteral("Unsupported VTT markup: <%1>").arg(tag));
            i=j+1; continue;
        }
        if (src[i].cp=='&') {
            qsizetype j=i+1; while (j<src.size() && j-i<=12 && src[j].cp!=';' && src[j].cp!='\n') ++j;
            if (j==src.size() || src[j].cp!=';') fail(QStringLiteral("Malformed VTT entity"));
            QString name; for (qsizetype k=i+1;k<j;++k) name.append(QChar(src[k].cp));
            uint cp=0;
            if(name=="amp")cp='&'; else if(name=="lt")cp='<'; else if(name=="gt")cp='>'; else if(name=="quot")cp='"'; else if(name=="apos")cp='\'';
            else if(name.startsWith('#')) { bool ok=false; cp=name.startsWith("#x",Qt::CaseInsensitive)?name.mid(2).toUInt(&ok,16):name.mid(1).toUInt(&ok,10); if(!ok||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)) fail(QStringLiteral("Invalid VTT numeric entity")); }
            else fail(QStringLiteral("Unsupported VTT entity &%1;").arg(name));
            out.push_back({cp,src[i].begin,src[j].end}); i=j+1; continue;
        }
        out.push_back(src[i++]);
    }
    return out;
}

void checkUnitCount(const QJsonArray &a) { if (a.size() >= MaxUnits) fail(QStringLiteral("Input exceeds 10000 source units")); }

QJsonArray parseText(const QByteArray &b, const QString &format, qsizetype start, qsizetype end)
{
    const QString sourceFormat = format;
    QJsonArray out;
    if(format=="pdf_text") {
        int page=1; qsizetype p=start;
        while(p<=end) {
            qsizetype pageEnd=p; while(pageEnd<end && b.at(pageEnd)!='\f') ++pageEnd;
            const auto pls=lines(b,p,pageEnd,true);
            for(const auto &l:pls) {
                if(l.start==l.end) continue;
                const auto chars=decode(b,l.start,l.end); const QString s=makeText(chars);
                if(s.trimmed().isEmpty()) continue;
                auto o=unit(QStringLiteral("page-%1-line-%2").arg(page).arg(l.number),s,classify(s),sourceFormat,b,l.start,l.end,chars);
                o.insert("line",l.number); o.insert("page",page); o.insert("raw_byte_source","pdftotext_extracted_utf8");
                if(isSceneHeading(s)) o.insert("source_label","scene_heading");
                checkUnitCount(out); out.append(o);
            }
            if(pageEnd>=end) break;
            p=pageEnd+1; ++page;
        }
        return out;
    }
    const auto ls=lines(b,start,end);
    for (const auto &l:ls) {
        if(l.start==l.end) continue;
        const auto chars=decode(b,l.start,l.end); const QString s=makeText(chars);
        auto o=unit(QStringLiteral("line-%1").arg(l.number),
                    s, classify(s), sourceFormat,b,l.start,l.end,chars);
        o.insert("line", l.number);
        if(isSceneHeading(s)) o.insert("source_label","scene_heading");
        checkUnitCount(out); out.append(o);
    }
    Q_UNUSED(end);
    return out;
}

QJsonArray parseSubtitles(const QByteArray &b, bool vtt, qsizetype start)
{
    const auto ls=lines(b,start); QJsonArray out; QSet<QString> suppliedIds;
    qsizetype i=0; int ordinal=0;
    if(vtt) {
        if(ls.isEmpty()) fail(QStringLiteral("WebVTT requires WEBVTT header"));
        const QByteArray header=b.mid(ls[0].start,ls[0].end-ls[0].start);
        const bool signature=header=="WEBVTT" || header.startsWith("WEBVTT ") || header.startsWith("WEBVTT\t");
        if(!signature || header.contains("-->")) fail(QStringLiteral("WebVTT requires WEBVTT header with optional space/tab metadata and no timing delimiter"));
        i=1;
    }
    while(i<ls.size()) {
        while(i<ls.size() && ls[i].start==ls[i].end) ++i;
        if(i>=ls.size()) break;
        const auto first=QString::fromUtf8(b.constData()+ls[i].start,ls[i].end-ls[i].start);
        static const QRegularExpression noteBlock(QStringLiteral("^NOTE(?:[ \\t].*)?$"));
        if(vtt && noteBlock.match(first).hasMatch()) { while(i<ls.size() && ls[i].start!=ls[i].end) ++i; continue; }
        if(vtt && (first=="STYLE" || first.startsWith("REGION"))) fail(QStringLiteral("VTT STYLE/REGION blocks are unsupported"));
        QString suppliedId; int timeLine=i;
        const QByteArray firstBytes=b.mid(ls[i].start,ls[i].end-ls[i].start);
        if(!firstBytes.contains(" --> ")) {
            suppliedId=first;
            if(suppliedId.toUtf8().size()>1024) fail(QStringLiteral("Subtitle cue ID exceeds 1 KiB limit"));
            ++timeLine; if(timeLine>=ls.size()) fail(QStringLiteral("Cue ID without timestamp"));
        }
        const QByteArray tb=b.mid(ls[timeLine].start,ls[timeLine].end-ls[timeLine].start);
        const auto pieces=tb.split(' ');
        if(pieces.size()<3 || pieces[1]!="-->") fail(QStringLiteral("Malformed subtitle cue timing"));
        if(vtt && pieces.size()>3) fail(QStringLiteral("VTT cue settings are unsupported"));
        if(!vtt && pieces.size()!=3) fail(QStringLiteral("SRT timestamp tail is unsupported"));
        const qint64 sm=timestamp(pieces[0],vtt), em=timestamp(pieces[2],vtt);
        if(sm>em) fail(QStringLiteral("Cue start time exceeds end time"));
        if(!suppliedId.isEmpty()) { if(suppliedIds.contains(suppliedId)) fail(QStringLiteral("Duplicate subtitle cue ID: %1").arg(suppliedId)); suppliedIds.insert(suppliedId); }
        int body=timeLine+1; if(body>=ls.size()||ls[body].start==ls[body].end) fail(QStringLiteral("Subtitle cue has no text"));
        const qsizetype rawA=ls[body].start; int last=body;
        while(last+1<ls.size() && ls[last+1].start!=ls[last+1].end) ++last;
        const qsizetype rawZ=ls[last].end;
        QVector<SrcChar> map;
        for(int ln=body;ln<=last;++ln) {
            if(ln>body) map.push_back({'\n',ls[ln-1].end,ls[ln].start});
            const auto part=vtt?visibleVtt(b,ls[ln].start,ls[ln].end):decode(b,ls[ln].start,ls[ln].end);
            map+=part;
        }
        const QString text=makeText(map);
        auto o=unit(QStringLiteral("cue-%1").arg(++ordinal),text,"dialogue",vtt?"vtt":"srt",b,rawA,rawZ,map);
        o.insert("raw_byte_source","input_utf8");
        if(!suppliedId.isEmpty()) o.insert("original_id",suppliedId);
        o.insert("cue_start_ms",double(sm)); o.insert("cue_end_ms",double(em));
        checkUnitCount(out); out.append(o); i=last+1;
    }
    return out;
}
}

QJsonArray parse(const QByteArray &input, const QString &formatInput)
{
    if(input.size()>MaxBytes) fail(QStringLiteral("Input exceeds 2 MiB limit"));
    QString format=formatInput.toLower();
    if(format!="srt" && format!="vtt" && format!="text" && format!="markdown" && format!="pdf_text")
        fail(QStringLiteral("Unsupported source format: %1").arg(formatInput));
    qsizetype start=0;
    if(input.startsWith("\xEF\xBB\xBF")) start=3;
    decode(input,start,input.size()); // Validate the whole source, including skipped blocks.
    if(format=="srt"||format=="vtt") return parseSubtitles(input,format=="vtt",start);
    return parseText(input,format,start,input.size());
}

bool codepointSpanToUtf16(const QString &text, qsizetype cpStart, qsizetype cpEnd,
                          qsizetype *utf16Start, qsizetype *utf16End)
{
    if(!utf16Start||!utf16End||cpStart<0||cpEnd<cpStart) return false;
    auto boundary=[&](qsizetype target, qsizetype *result) {
        qsizetype cp=0, u=0;
        while(u<text.size() && cp<target) { const QChar c=text.at(u); if(c.isHighSurrogate()) { if(u+1>=text.size()||!text.at(u+1).isLowSurrogate()) return false; u+=2; } else { if(c.isLowSurrogate()) return false; ++u; } ++cp; }
        if(cp!=target) return false; *result=u; return true;
    };
    qsizetype a=0,b=0;
    if(!boundary(cpStart,&a)||!boundary(cpEnd,&b)) return false;
    *utf16Start=a; *utf16End=b; return true;
}
}
