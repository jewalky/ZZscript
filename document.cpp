#include "tokenizer.h"
#include "document.h"

#include <QTime>
#include <QToolTip>
#include <QTabWidget>
#include <QFile>
#include <QFileInfo>
#include <QBoxLayout>

Document::Document(DocumentTab* tab)
{
    isnew = false;
    ownparser = true;
    parser = nullptr;
    this->tab = tab;
}

Document::~Document()
{
    if (parser && ownparser)
        delete parser;
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
    if (parser && ownparser) delete parser;
    ownparser = true;
    parser = new Parser(tokens);
    parser->parse();
    parsedTokens = parser->parsedTokens;
}

void Document::reparse()
{
    if (!parser) return;

    // get current types
    QList<QSharedPointer<ZTreeNode>> allTypes = parser->getTypeInformation();
    QList<QSharedPointer<ZTreeNode>> ownTypes = parser->getOwnTypeInformation();
    for (QSharedPointer<ZTreeNode> ownType : ownTypes)
        allTypes.removeAll(ownType);
    if (ownparser)
        delete parser;
    Tokenizer tok(contents);
    tokens = tok.readAllTokens();
    parser = new Parser(tokens);
    ownparser = true;
    parser->parse();
    allTypes.append(parser->getOwnTypeInformation());
    parser->setTypeInformation(allTypes);

    // field pass
    for (QSharedPointer<ZTreeNode> node : parser->root->children)
    {
        if (node->type() == ZTreeNode::Class)
        {
            parser->parseClassFields(node.dynamicCast<ZClass>());
        }
        else if (node->type() == ZTreeNode::Struct)
        {
            parser->parseStructFields(node.dynamicCast<ZStruct>());
        }
    }

    // method pass
    for (QSharedPointer<ZTreeNode> node : parser->root->children)
    {
        if (node->type() == ZTreeNode::Class)
        {
            parser->parseClassMethods(node.dynamicCast<ZClass>());
        }
        else if (node->type() == ZTreeNode::Struct)
        {
            parser->parseStructMethods(node.dynamicCast<ZStruct>());
        }
    }

    parsedTokens = parser->parsedTokens;

    // to-do: if we ctrl+s, it's a good idea to update all other code (that might try to reference the new class)
}

void Document::save()
{
    // take contents, save to disk
    if (isnew) return; // do nothing for now.. later make a popup window asking for save path
    QFile f(fullPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        //
        f.write(contents.toUtf8());
        // and once done, force reparse for all the types
        // note: this is not very good coding. need to retrieve parser in some more direct way
        // also, later we need to check that only current project (and below) is reloaded.
        // we don't need to reparse gzdoom.pk3 just because some user code was modified.
        //
        reparse();
        QList<QSharedPointer<ZTreeNode>> allTypes = parser->getTypeInformation();
        QList<Parser*> parsers;
        for (QSharedPointer<ZTreeNode> node : allTypes)
        {
            QSharedPointer<ZTreeNode> nodeParent = node->parent.toStrongRef();
            QSharedPointer<ZFileRoot> root = nodeParent.dynamicCast<ZFileRoot>();
            if (!root) continue;
            Parser* ownParser = root->parser;
            if (!parsers.contains(ownParser))
                parsers.append(ownParser);
        }
        for (Parser* p : parsers)
        {
            p->parse();
            p->setTypeInformation(allTypes);

            // field pass
            for (QSharedPointer<ZTreeNode> node : parser->root->children)
            {
                if (node->type() == ZTreeNode::Class)
                {
                    parser->parseClassFields(node.dynamicCast<ZClass>());
                }
                else if (node->type() == ZTreeNode::Struct)
                {
                    parser->parseStructFields(node.dynamicCast<ZStruct>());
                }
            }

            // method pass
            for (QSharedPointer<ZTreeNode> node : parser->root->children)
            {
                if (node->type() == ZTreeNode::Class)
                {
                    parser->parseClassMethods(node.dynamicCast<ZClass>());
                }
                else if (node->type() == ZTreeNode::Struct)
                {
                    parser->parseStructMethods(node.dynamicCast<ZStruct>());
                }
            }
        }
    }
    else
    {
        qDebug("could not save into %s", fullPath.toUtf8().data());
    }
}

void Document::setTab(DocumentTab* tab)
{
    if (this->tab) return;
    this->tab = tab;
}

DocumentTab* Document::getTab()
{
    return tab;
}

void Document::syncFromSource(ProjectFile* pf)
{
    if (isnew) return;

    if (pf)
    {
        contents = pf->contents;
        parser = pf->parser;
        ownparser = false;
        parsedTokens = parser->parsedTokens;
        if (tab)
        {
            DocumentEditor* editor = tab->getEditor();
            editor->setPlainText(contents);
            editor->textChanged();
        }
        return;
    }

    if (ownparser && parser)
        delete parser;
    ownparser = true;
    parser = nullptr;

    qDebug("path = %s", fullPath.toUtf8().data());
    QFileInfo fi(fullPath);
    if (!fi.isFile())
        return;

    //
    QFile f(fullPath);
    if (f.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        contents = f.readAll();

        if (tab)
        {
            DocumentEditor* editor = tab->getEditor();
            editor->setPlainText(contents);
            editor->textChanged();
        }

        f.close();
        return;
    }

    // else
    qDebug("failed to sync with source: %s", fullPath.toUtf8().data());
}

DocumentEditor* DocumentTab::getEditor()
{
    return editor;
}

DocumentEditor::DocumentEditor(QWidget* parent) : QPlainTextEdit(parent)
{
    // link various events
    // for now, any text change should cause reparse

    connect(this, SIGNAL(textChanged()), this, SLOT(onTextChanged()));
    setUndoRedoEnabled(false);
    setMouseTracking(true);

    //
    const int tabStop = 4;  // 4 characters

    QFont f(font());
    f.setFamily("Courier");
    QFontMetricsF metrics(f);
    setTabStopDistance(metrics.width(' ')*tabStop);

    processing = false;
}

DocumentTab::DocumentTab(QWidget* parent, Document* doc) : QWidget(parent)
{
    this->doc = doc;

    // create text document
    editor = new DocumentEditor(this);
    QLayout* tabLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    setLayout(tabLayout);
    tabLayout->addWidget(editor);
    tabLayout->setMargin(2);
    editor->textChanged();
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
    /*
    QTime elTimer;
    elTimer.start();
    doc->parse();
    qDebug("parsing done in %d ms", elTimer.elapsed());*/
    // reparse this document
    doc->reparse();

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

                case ParserToken::Local:
                {
                    format.setFontItalic(true);
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
    else if (tok->type == ParserToken::Local)
    {
        if (tok->reference)
        {
            QString typelocal = "<b>Local</b>";
            QSharedPointer<ZLocalVariable> local = tok->reference.dynamicCast<ZLocalVariable>();
            QString addauto = local->hasType ? "" : "auto ";
            QString typeclass = " (unresolved "+addauto+"type) ";
            QSharedPointer<ZTreeNode> localParent = local->parent.toStrongRef();
            if (localParent && localParent->type() == ZTreeNode::Method)
                typelocal = "<b>Argument</b>";
            // get type of variable if present
            ZCompoundType vtype = local->varType;
            if (!local->hasType && local->children.size() && local->children[0]->type() == ZTreeNode::Expression)
            {
                ZExpression* localExpr = reinterpret_cast<ZExpression*>(local->children[0].get());
                vtype = localExpr->resultType;
            }
            if (vtype.reference)
            {
                switch (vtype.reference->type())
                {
                case ZTreeNode::Class:
                    typeclass = " "+addauto+"class ";
                    break;
                case ZTreeNode::Struct:
                    typeclass = " "+addauto+"struct ";
                    break;
                case ZTreeNode::Enum:
                    typeclass = " "+addauto+"enum ";
                    break;
                case ZTreeNode::SystemType:
                    typeclass = " "+addauto+"";
                    break;
                default:
                    break;
                }
            }
            return typelocal + typeclass + vtype.type + " <i>" + tok->referencePath + "</i>";
        }
        else
        {
            return "<b>Unresolved local</b> <i>" + tok->referencePath + "</i>";
        }
    }

    return "";
}

void DocumentEditor::showEvent(QShowEvent* event)
{
    onTextChanged();
}
