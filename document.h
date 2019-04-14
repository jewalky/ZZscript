#ifndef DOCUMENT_H
#define DOCUMENT_H

#include <QString>
#include <QWidget>
#include <QPlainTextEdit>
#include <QContextMenuEvent>
#include "tokenizer.h"
#include "parser.h"

class Document
{
public:
    Document();

    void parse();

    bool isnew;
    QString location;
    QString contents;
    QList<Tokenizer::Token> tokens;
    QList<ParserToken> parsedTokens;
};

class DocumentTab : public QWidget
{
    Q_OBJECT

public:
    explicit DocumentTab(QWidget* parent = nullptr) : QWidget(parent)
    {
        doc = nullptr;
    }

    void setDocument(Document* doc)
    {
        if (!this->doc) this->doc = doc;
    }

    Document* document()
    {
        return doc;
    }

private:
    Document* doc;
};

class DocumentEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit DocumentEditor(QWidget* parent = nullptr);

    void contextMenuEvent(QContextMenuEvent *event) override;

public slots:
    void onTextChanged();

private:
    bool processing;
};

#endif // DOCUMENT_H
