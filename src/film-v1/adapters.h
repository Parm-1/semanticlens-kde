#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QString>

namespace FilmAdapters {

// Formats: srt, vtt, text, markdown, pdf_text. pdf_text is already extracted
// UTF-8; all byte offsets refer to that extracted buffer, not to the PDF file.
// Invalid/unsupported input throws std::invalid_argument with a user-displayable
// diagnostic. No decoding replacement or Unicode normalization is performed.
QJsonArray parse(const QByteArray &bytes, const QString &format);

// Convert an end-exclusive Unicode-code-point span in QString to a UTF-16
// code-unit span. Returns false for invalid bounds or a boundary inside a
// surrogate pair. QString offsets are never accepted as code-point offsets.
bool codepointSpanToUtf16(const QString &text, qsizetype cpStart, qsizetype cpEnd,
                          qsizetype *utf16Start, qsizetype *utf16End);

} // namespace FilmAdapters
