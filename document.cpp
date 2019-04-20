#include "tokenizer.h"
#include "document.h"

#include <QTime>
#include <QToolTip>

Document::Document()
{
    isnew = false;
    parser = nullptr;
}

void Document::parse()
{
    Tokenizer tok(contents);
    tokens = tok.readAllTokens();

    //
    // produce structure:
    // - file
    //   - classes[]
    //     - fields[]
    //     - methods[]
    //       - code
    //         - locals
    //         - code[]
    //           ...
    //     - constants[]
    if (parser) delete parser;
    parser = new Parser(tokens);
    parser->parse();
    parsedTokens = parser->parsedTokens;
}

DocumentEditor::DocumentEditor(QWidget* parent) : QPlainTextEdit(parent)
{
    // link various events
    // for now, any text change should cause reparse

    connect(this, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    setUndoRedoEnabled(false);
    setMouseTracking(true);

    processing = false;
}

void DocumentEditor::onTextChanged()
{
    if (processing)
        return;

    //setFontFamily("Courier");

    QString newText = toPlainText();
    DocumentTab* tab = qobject_cast<DocumentTab*>(parentWidget());
    assert(tab != nullptr);
    Document* doc = tab->document();
    assert(doc != nullptr);
    doc->contents = newText;
    QTime elTimer;
    elTimer.start();
    doc->parse();
    qDebug("parsing done in %d ms", elTimer.elapsed());

    this->setUpdatesEnabled(false);
    processing = true;
    QTextCursor tcur = this->textCursor();
    tcur.beginEditBlock();
    tcur.setPosition(0);
    tcur.setPosition(newText.length(), QTextCursor::KeepAnchor);
    QTextCharFormat format_base;
    format_base.setBackground(QColor(0, 0, 0, 0));
    format_base.setFontFamily("Courier");
    tcur.setCharFormat(format_base);
    // do syntax highlighting
    bool primitiveHL = false;
    if (primitiveHL)
    {
        for (QList<Tokenizer::Token>::iterator it = doc->tokens.begin(); it != doc->tokens.end(); ++it)
        {
            Tokenizer::Token& tok = (*it);

            tcur.setPosition(tok.startsAt);
            tcur.setPosition(tok.endsAt, QTextCursor::KeepAnchor);

            QTextCharFormat format;
            format.setFontFamily("Courier");

            if (tok.type == Tokenizer::Identifier)
            {
                if (tok.value == "if" ||
                    tok.value == "for" ||
                    tok.value == "while" ||
                    tok.value == "else" ||
                    tok.value == "break" ||
                    tok.value == "continue")
                {
                    format.setFontWeight(QFont::Bold);
                    QBrush blue(QColor(0, 0, 255));
                    format.setForeground(blue);
                }
                if (tok.value == "true" ||
                    tok.value == "false" ||
                    tok.value == "null")
                {
                    QBrush brown(QColor(127, 0, 0));
                    format.setForeground(brown);
                }
            }
            else if (tok.type == Tokenizer::LineComment || tok.type == Tokenizer::BlockComment)
            {
                QBrush gray(QColor(127, 127, 127));
                format.setForeground(gray);
                format.setFontItalic(true);
            }
            else if (tok.type == Tokenizer::String || tok.type == Tokenizer::Name)
            {
                QBrush green(QColor(0, 127, 0));
                format.setForeground(green);
                format.setFontWeight(QFont::Bold);
            }
            else if (tok.type == Tokenizer::Integer || tok.type == Tokenizer::Double)
            {
                QBrush pink(QColor(164, 0, 164));
                format.setForeground(pink);
            }
            else if (tok.type == Tokenizer::Invalid)
            {
                QBrush red(QColor(255, 0, 0));
                format.setBackground(red);
            }
            else
            {
                QBrush red(QColor(255, 0, 0));
                format.setForeground(red);
            }

            tcur.setCharFormat(format);
        }
    }
    else
    {
        for (QList<ParserToken>::iterator it = doc->parsedTokens.begin(); it != doc->parsedTokens.end(); ++it)
        {
            ParserToken& ptok = (*it);

            if (ptok.type == ParserToken::Invalid)
            {
                for (int i = ptok.startsAt; i < ptok.endsAt; i++)
                {
                    tcur.setPosition(i);
                    tcur.setPosition(i+1, QTextCursor::KeepAnchor);
                    QTextCharFormat lfmt = tcur.charFormat();
                    lfmt.setUnderlineColor(QColor(255, 0, 0));
                    lfmt.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
                    tcur.setCharFormat(lfmt);
                }

                continue;
            }

            tcur.setPosition(ptok.startsAt);
            tcur.setPosition(ptok.endsAt, QTextCursor::KeepAnchor);

            QTextCharFormat format = format_base;

            switch (ptok.type)
            {
                case ParserToken::Comment:
                {
                    QBrush gray(QColor(127, 127, 127));
                    format.setForeground(gray);
                    format.setFontItalic(true);
                    break;
                }

                case ParserToken::Preprocessor:
                {
                    QBrush blue(QColor(0, 0, 255));
                    format.setForeground(blue);
                    format.setFontWeight(QFont::Bold);
                    break;
                }

                case ParserToken::Keyword:
                {
                    QBrush blue(QColor(0, 0, 255));
                    format.setForeground(blue);
                    break;
                }

                case ParserToken::TypeName:
                {
                    QBrush green(QColor(0, 128, 255));
                    format.setForeground(green);
                    break;
                }

                case ParserToken::String:
                {
                    QBrush green(QColor(0, 128, 0));
                    format.setForeground(green);
                    format.setFontWeight(QFont::Bold);
                    break;
                }

                case ParserToken::ConstantName:
                {
                    QBrush brown(QColor(128, 0, 0));
                    format.setForeground(brown);
                    break;
                }

                case ParserToken::Number:
                {
                    QBrush pink(QColor(164, 0, 164));
                    format.setForeground(pink);
                    break;
                }

                case ParserToken::SpecialToken:
                case ParserToken::Operator:
                {
                    QBrush red(QColor(255, 0, 0));
                    format.setForeground(red);
                    break;
                }

                default:
                    break;
            }

            tcur.setCharFormat(format);
        }
    }
    tcur.endEditBlock();
    this->setUpdatesEnabled(true);
    processing = false;
}

void DocumentEditor::contextMenuEvent(QContextMenuEvent* event)
{

}

bool DocumentEditor::event(QEvent* event)
{
    DocumentTab* tab = qobject_cast<DocumentTab*>(parentWidget());
    assert(tab != nullptr);
    Document* doc = tab->document();
    assert(doc != nullptr);

    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
        QTextCursor cursor = cursorForPosition(helpEvent->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        if (!cursor.selectedText().isEmpty())
        {
            int anchor = cursor.anchor();
            ParserToken* tok = nullptr;
            for (ParserToken& ptok : doc->parsedTokens)
            {
                if (ptok.startsAt <= anchor && ptok.endsAt > anchor)
                {
                    tok = &ptok;
                    break;
                }
            }

            if (tok)
            {
                QToolTip::showText(helpEvent->globalPos(), makeTokenTooltip(tok));
            }
            else
            {
                QToolTip::hideText();
            }
        }
        else
        {
            QToolTip::hideText();
        }
        return true;
    }
    return QPlainTextEdit::event(event);
}

QString DocumentEditor::makeTokenTooltip(ParserToken* tok)
{
    if (tok->type == ParserToken::TypeName)
    {
        if (tok->reference)
        {
            QString typeclass = "class";
            switch (tok->reference->type())
            {
            case ZTreeNode::Struct:
                typeclass = "struct";
                break;
            case ZTreeNode::Enum:
                typeclass = "enum";
                break;
            case ZTreeNode::SystemType:
                typeclass = "system";
                break;
            default:
                break;
            }
            return "<b>Type</b> " + typeclass + " <i>" + tok->referencePath + "</i>";
        }
        else
        {
            return "<b>Unresolved type</b> <i>" + tok->referencePath + "</i>";
        }
    }
}
