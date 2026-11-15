#include "utils/SourcePreviewEdit.hpp"

#include <algorithm>
#include <cmath>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QWheelEvent>

namespace {
QTextCharFormat MakeFormat(const QColor& color, bool bold = false,
                           bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(color);
    if (bold) format.setFontWeight(QFont::DemiBold);
    format.setFontItalic(italic);
    return format;
}
}

SourcePreviewHighlighter::SourcePreviewHighlighter(QTextDocument* document,
                                                   Language language)
    : QSyntaxHighlighter(document), m_language(language)
{
    const QTextCharFormat keyword = MakeFormat(QColor("#c586c0"), true);
    const QTextCharFormat type = MakeFormat(QColor("#4ec9b0"));
    const QTextCharFormat function = MakeFormat(QColor("#dcdcaa"));
    const QTextCharFormat string = MakeFormat(QColor("#ce9178"));
    const QTextCharFormat number = MakeFormat(QColor("#b5cea8"));
    const QTextCharFormat literal = MakeFormat(QColor("#569cd6"), true);
    const QTextCharFormat comment = MakeFormat(QColor("#6a9955"), false, true);
    const QTextCharFormat property = MakeFormat(QColor("#9cdcfe"));
    const QTextCharFormat annotation = MakeFormat(QColor("#d7ba7d"));
    m_multilineStringFormat = string;

    if (m_language == Language::Python) {
        AddRule(QStringLiteral(
            R"(\b(?:and|as|assert|async|await|break|class|continue|def|del|elif|else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|pass|raise|return|try|while|with|yield)\b)"),
            keyword);
        AddRule(QStringLiteral(R"(\b(?:True|False|None)\b)"), literal);
        AddRule(QStringLiteral(
            R"(\b(?:0[xX][0-9A-Fa-f]+|0[bB][01]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?)\b)"),
            number);
        AddRule(QStringLiteral(R"(@[A-Za-z_]\w*)"), annotation);
        AddRule(QStringLiteral(R"(\bclass\s+([A-Za-z_]\w*))"), type, 1);
        AddRule(QStringLiteral(R"(\bdef\s+([A-Za-z_]\w*))"), function, 1);
        AddRule(QStringLiteral(
            R"re((?:[rRuUbBfF]{0,2})(?:'(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*"))re"),
            string);
        // Comments are last so keyword-like text inside one stays comment-colored.
        AddRule(QStringLiteral(R"(#[^\n]*)"), comment);
    } else if (m_language == Language::Yaml) {
        AddRule(QStringLiteral(R"(^\s*(?:-\s*)?([A-Za-z_][\w.-]*)(?=\s*:))"),
                property, 1);
        AddRule(QStringLiteral(R"((?<!\w)[&*][A-Za-z_][\w.-]*)"), annotation);
        AddRule(QStringLiteral(R"((?<!\w)![A-Za-z_][\w.-]*)"), type);
        AddRule(QStringLiteral(R"(\b(?:true|false|null|yes|no|on|off)\b)"), literal);
        AddRule(QStringLiteral(
            R"((?<![\w.-])[+-]?(?:0x[0-9A-Fa-f]+|\d+(?:\.\d+)?)(?![\w.-]))"),
            number);
        AddRule(QStringLiteral(
            R"re('(?:''|[^'])*'|"(?:\\.|[^"\\])*")re"), string);
        AddRule(QStringLiteral(R"(#[^\n]*)"), comment);
    }
}

SourcePreviewHighlighter::Language
SourcePreviewHighlighter::LanguageForPath(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix == "py") return Language::Python;

    static const QStringList yamlSuffixes = {
        "yaml", "yml", "scene", "mat", "material", "texture", "shader",
        "font", "audio", "tilemap", "tileset", "config"
    };
    if (yamlSuffixes.contains(suffix)) return Language::Yaml;
    return Language::PlainText;
}

QString SourcePreviewHighlighter::LanguageName(Language language)
{
    switch (language) {
        case Language::Python: return "Python";
        case Language::Yaml: return "YAML";
        default: return "Plain text";
    }
}

void SourcePreviewHighlighter::AddRule(const QString& pattern,
                                       const QTextCharFormat& format,
                                       int capture)
{
    m_rules.push_back({QRegularExpression(pattern), format, capture});
}

void SourcePreviewHighlighter::highlightBlock(const QString& text)
{
    for (const Rule& rule : m_rules) {
        auto matches = rule.pattern.globalMatch(text);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            const int start = match.capturedStart(rule.capture);
            const int length = match.capturedLength(rule.capture);
            if (start >= 0 && length > 0) setFormat(start, length, rule.format);
        }
    }

    setCurrentBlockState(0);
    if (m_language == Language::Python)
        HighlightPythonMultilineStrings(text);
}

void SourcePreviewHighlighter::HighlightPythonMultilineStrings(const QString& text)
{
    auto findNextOpening = [&text](int from, int& state) {
        const int single = text.indexOf("'''", from);
        const int doubled = text.indexOf("\"\"\"", from);
        if (single < 0 && doubled < 0) {
            state = 0;
            return -1;
        }
        if (single >= 0 && (doubled < 0 || single < doubled)) {
            state = 1;
            return single;
        }
        state = 2;
        return doubled;
    };

    int state = previousBlockState();
    int start = 0;
    bool continuing = state == 1 || state == 2;
    if (!continuing) {
        start = findNextOpening(0, state);
        if (start < 0) return;
    }

    while (start >= 0) {
        const QString delimiter = state == 1 ? "'''" : "\"\"\"";
        const int searchFrom = continuing ? start : start + delimiter.size();
        const int end = text.indexOf(delimiter, searchFrom);
        if (end < 0) {
            setFormat(start, text.size() - start, m_multilineStringFormat);
            setCurrentBlockState(state);
            return;
        }

        setFormat(start, end - start + delimiter.size(), m_multilineStringFormat);
        continuing = false;
        start = findNextOpening(end + delimiter.size(), state);
    }
}

SourcePreviewEdit::SourcePreviewEdit(const QString& filePath, QWidget* parent)
    : QPlainTextEdit(parent),
      m_language(SourcePreviewHighlighter::LanguageForPath(filePath))
{
    setReadOnly(true);
    setUndoRedoEnabled(false);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_fontPointSize = fixedFont.pointSize() > 0 ? fixedFont.pointSize() : 10;
    setFont(fixedFont);
    setTabStopDistance(QFontMetricsF(fixedFont).horizontalAdvance(' ') * 4.0);

    new SourcePreviewHighlighter(document(), m_language);
}

QString SourcePreviewEdit::LanguageName() const
{
    return SourcePreviewHighlighter::LanguageName(m_language);
}

void SourcePreviewEdit::wheelEvent(QWheelEvent* event)
{
    if (!event->modifiers().testFlag(Qt::ControlModifier)) {
        QPlainTextEdit::wheelEvent(event);
        return;
    }

    int delta = event->angleDelta().y();
    if (delta == 0) delta = event->pixelDelta().y();
    if (delta == 0) {
        event->ignore();
        return;
    }

    const int steps = std::max(1, std::abs(delta) / 120);
    SetPreviewFontPointSize(m_fontPointSize + (delta > 0 ? steps : -steps));
    event->accept();
}

void SourcePreviewEdit::SetPreviewFontPointSize(int pointSize)
{
    m_fontPointSize = std::clamp(pointSize, 8, 32);
    QFont previewFont = font();
    previewFont.setPointSize(m_fontPointSize);
    setFont(previewFont);
    setTabStopDistance(QFontMetricsF(previewFont).horizontalAdvance(' ') * 4.0);
}
