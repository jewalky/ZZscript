#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QString>
#include <QWidget>
#include <QPlainTextEdit>
#include <QContextMenuEvent>
#include "tokenizer.h"
#include "parser.h"
#include "project.h"

class DocumentTab;
class Document
{
public:
    Document(DocumentTab* tab = nullptr);

    void parse();
    void reparse();
    void setTab(DocumentTab* tab);
    void save();
    DocumentTab* getTab();

    void syncFromSource(ProjectFile* pf = nullptr);

    bool isnew;
    QString fullPath;
    QString location;
    QString contents;
    QList<Tokenizer::Token> tokens;
    QList<ParserToken> parsedTokens;

private:
    // parsed tokens and such are valid until reparse
    Parser* parser;
    DocumentTab* tab;
};

class DocumentEditor;
class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(QWidget* parent = nullptr, Document* doc = nullptr);

    void setDocument(Document* doc)
    {
        if (!this->doc) this->doc = doc;
    }

    Document* document()
    {
        return doc;
    }

    DocumentEditor* getEditor();

private:
    Document* doc;
    // for quick access
    DocumentEditor* editor;
};

class DocumentEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit DocumentEditor(QWidget* parent = nullptr);

    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

public slots:
    void onTextChanged();

private:
    bool processing;

    QString makeTokenTooltip(ParserToken* tok);
};

#endif // DOCUMENT_H
