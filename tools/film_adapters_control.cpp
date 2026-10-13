#include "../src/film-v1/adapters.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <iostream>
#include <stdexcept>

using namespace FilmAdapters;

static void require(bool ok, const char *msg) { if(!ok) throw std::runtime_error(msg); }
static void reject(const QByteArray &b, const QString &f, const QString &piece)
{
    try { parse(b,f); } catch(const std::invalid_argument &e) { require(QString::fromUtf8(e.what()).contains(piece), "wrong diagnostic"); return; }
    throw std::runtime_error(("input unexpectedly accepted, expected diagnostic: "+piece).toStdString());
}
static QJsonObject at(const QJsonArray &a,int i) { return a.at(i).toObject(); }

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try {
        const QByteArray srt("\xEF\xBB\xBF" "1\r\n00:00:01,000 --> 00:00:03,500\r\nINT. CAFÉ - NIGHT\r\nALICE: Hi 😀\r\n\r\n2\r\n00:00:02,000 --> 00:00:04,000\r\nBye.\r\n");
        const auto sa=parse(srt,"srt"); require(sa.size()==2,"SRT cue count");
        const auto s0=at(sa,0); require(s0.value("unit_id").toString()=="cue-1","stable SRT ID");
        require(s0.value("source_format").toString()=="srt" && !s0.contains("source_kind"),"format metadata is independent of manifest source_kind");
        require(s0.value("original_id").toString()=="1","original SRT ID");
        require(s0.value("cue_start_ms").toInt()==1000 && s0.value("cue_end_ms").toInt()==3500,"SRT timing");
        require(s0.value("text").toString()==QString::fromUtf8("INT. CAFÉ - NIGHT\nALICE: Hi 😀"),"SRT visible text");
        require(at(sa,1).value("cue_start_ms").toInt()==2000,"overlapping cues retained");
        require(s0.value("canonical_codepoint_raw_spans").toArray().size()==s0.value("text").toString().toUcs4().size(),"SRT mapping count");
        const auto rawMap=s0.value("canonical_codepoint_raw_spans").toArray();
        require(rawMap.last().toArray().at(0).toDouble()<rawMap.last().toArray().at(1).toDouble(),"emoji maps to raw UTF-8 bytes");

        const QByteArray vtt="WEBVTT\n\nNOTE private metadata\nignored\n\ncue-A\n00:00.100 --> 00:01.250\n<b>Hi &amp; 😀</b>\n";
        const auto va=parse(vtt,"vtt"); require(va.size()==1,"VTT NOTE skipped");
        const auto v0=at(va,0); require(v0.value("unit_id").toString()=="cue-1" && v0.value("original_id").toString()=="cue-A","VTT IDs");
        require(v0.value("source_format").toString()=="vtt" && !v0.contains("source_kind"),"VTT format metadata");
        require(v0.value("text").toString()==QString::fromUtf8("Hi & 😀"),"VTT rendered text");
        const auto vm=v0.value("canonical_codepoint_raw_spans").toArray();
        require(vm.size()==v0.value("text").toString().toUcs4().size(),"VTT mapping count");
        const int ampIndex=3; const auto amp=vm.at(ampIndex).toArray();
        require(vtt.mid(amp[0].toInt(),amp[1].toInt()-amp[0].toInt())=="&amp;","entity maps to full source entity");
        require(parse("WEBVTT - Generated header metadata\n\n00:00.000 --> 00:00.001\nx\n","vtt").size()==1,"space metadata header accepted");
        require(parse("WEBVTT\tGenerated header metadata\n\n00:00.000 --> 00:00.001\nx\n","vtt").size()==1,"tab metadata header accepted");

        const auto text=parse("INT. KITCHEN - DAY\r\nALICE: Hello\r\n(Waves)\r\nplain\n","text");
        require(text.size()==4,"text line count including physical numbers");
        require(at(text,0).value("source_format").toString()=="text" && !at(text,0).contains("source_kind"),"screenplay format metadata");
        require(at(text,0).value("kind").toString()=="text" && at(text,0).value("source_label").toString()=="scene_heading","scene heading classification");
        require(at(text,1).value("kind").toString()=="dialogue","speaker/dialogue classification");
        require(at(text,2).value("kind").toString()=="stage_direction","parenthetical classification");
        require(at(text,3).value("unit_id").toString()=="line-4","physical line ID");

        const auto markdown=parse("**Markdown**\n","markdown");
        require(markdown.size()==1 && at(markdown,0).value("source_format").toString()=="markdown" && !at(markdown,0).contains("source_kind"),"Markdown format metadata");

        const auto pdf=parse("Page one line\n\fPage two line\n","pdf_text");
        require(pdf.size()==2,"PDF extracted lines");
        require(at(pdf,0).value("unit_id").toString()=="page-1-line-1" && at(pdf,0).value("page").toInt()==1,"PDF page one");
        require(at(pdf,1).value("unit_id").toString()=="page-2-line-1" && at(pdf,1).value("page").toInt()==2,"PDF page two");
        require(at(pdf,1).value("source_format").toString()=="pdf_text" && !at(pdf,1).contains("source_kind"),"PDF format metadata is independent of manifest source_kind");
        require(at(pdf,1).value("raw_byte_source").toString()=="pdftotext_extracted_utf8","PDF extraction provenance");

        const QString unicode=QString::fromUtf8("a😀e\u0301אב"); qsizetype u0=-1,u1=-1;
        require(codepointSpanToUtf16(unicode,1,2,&u0,&u1) && u0==1 && u1==3,"emoji codepoint to surrogate pair span");
        require(codepointSpanToUtf16(unicode,2,4,&u0,&u1) && u0==3 && u1==5,"combining and RTL offsets");
        require(!codepointSpanToUtf16(unicode,1,1,nullptr,&u1),"null offset rejected");
        require(!codepointSpanToUtf16(unicode,1,2,&u0,&u1) || (u0!=2 && u1!=2),"span does not split surrogate pair");
        require(parse("", "srt").isEmpty(),"empty SRT remains empty input");
        reject("1\n00:00:03,000 --> 00:00:02,000\nx\n","srt","exceeds");
        reject("WEBVTT\n\n00:01.000 --> 00:00.500\nx\n","vtt","exceeds");
        reject("WEBVTT\n\n00:01.000 --> 00:02.000 align:start\nx\n","vtt","settings");
        reject("WEBVTT\n\nSTYLE\n::cue{}\n","vtt","STYLE");
        reject("WEBVTTjunk\n\n00:00.000 --> 00:00.001\nx\n","vtt","WEBVTT header");
        reject("WEBVTT --> metadata\n\n00:00.000 --> 00:00.001\nx\n","vtt","timing delimiter");
        reject(QByteArray("bad\xC0\xAF",5),"text","UTF-8");
        reject("1\n00:99:00,000 --> 00:99:01,000\nx\n","srt","timestamp");
        reject("1\n00:00:00,000 --> 00:00:01,000 JUNK\nx\n","srt","timestamp tail");
        reject("1\n00:00:00,000 --> 00:00:01,000\nx\n\n1\n00:00:00,000 --> 00:00:01,000\ny\n","srt","Duplicate");
        reject("WEBVTT\n\n9223372036854775808:00:00.000 --> 9223372036854775808:00:01.000\nx\n","vtt","hour field");
        reject("WEBVTT\n\n2562047788016:00:00.000 --> 2562047788016:00:01.000\nx\n","vtt","exact JSON integer range");
        const qint64 maxExactMs=9007199254740991LL;
        const QString maxExactTime=QStringLiteral("2501999792:59:00.991");
        const QByteArray atJsonMax=QStringLiteral("WEBVTT\n\n%1 --> %1\nx\n").arg(maxExactTime).toUtf8();
        const auto maxUnits=parse(atJsonMax,"vtt");
        require(at(maxUnits,0).value("cue_start_ms").toDouble()==double(maxExactMs) &&
                at(maxUnits,0).value("cue_end_ms").toDouble()==double(maxExactMs),"maximum exact JSON timestamp retained");
        reject("WEBVTT\n\n2501999792:59:00.992 --> 2501999792:59:00.992\nx\n","vtt","exact JSON integer range");
        reject("WEBVTT\n\n2501999793:00:00.001 --> 2501999793:00:00.003\nx\n","vtt","exact JSON integer range");
        std::cout << "film-adapters-controls=PASS cases=positive_srt,vtt,text,markdown,pdf_text,crlf,overlap,raw_maps,unicode_offsets,empty,invalid_utf8,timestamps,duplicate_id,unsupported_vtt,source_format_enum,webvtt_header_metadata,unsupported_srt_tail,hour_conversion_overflow,millisecond_multiply_bound,json_safe_max,json_safe_max_plus_one,unsafe_json_integer\n";
        return 0;
    } catch(const std::exception &e) { std::cerr << "film-adapters-controls=FAIL " << e.what() << '\n'; return 1; }
}
