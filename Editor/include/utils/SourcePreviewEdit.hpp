#pragma once

#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QString>
#include <vector>

class QWheelEvent;

// Lightweight syntax coloring for project files shown by the Inspector's
// read-only source fallback. Qt supplies QSyntaxHighlighter, not language
// definitions, so RockEngine owns the small Python/YAML rule sets here.
class SourcePreviewHighlighter final : public QSyntaxHighlighter {
public:
    enum class Language {
        PlainText,
        Python,
        Yaml
    };

    SourcePreviewHighlighter(QTextDocument* document, Language language);

    static Language LanguageForPath(const QString& filePath);
    static QString LanguageName(Language language);

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
        int capture = 0;
    };

    void AddRule(const QString& pattern, const QTextCharFormat& format,
                 int capture = 0);
    void HighlightPythonMultilineStrings(const QString& text);

    Language m_language = Language::PlainText;
    std::vector<Rule> m_rules;
    QTextCharFormat m_multilineStringFormat;
};

class SourcePreviewEdit final : public QPlainTextEdit {
public:
    explicit SourcePreviewEdit(const QString& filePath, QWidget* parent = nullptr);

    QString LanguageName() const;
    int PreviewFontPointSize() const { return m_fontPointSize; }

protected:
    void wheelEvent(QWheelEvent* event) override;

private:
    void SetPreviewFontPointSize(int pointSize);

    SourcePreviewHighlighter::Language m_language;
    int m_fontPointSize = 10;
};
