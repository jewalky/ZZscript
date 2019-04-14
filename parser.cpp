#include "parser.h"
#include <cmath>

Parser::Parser(QList<Tokenizer::Token> tokens) : tokens(tokens)
{
    root = nullptr;
}

Parser::~Parser()
{
    if (root)
        delete root;
    root = nullptr;
}

ZTreeNode::~ZTreeNode()
{
    for (QList<ZTreeNode*>::iterator it = children.begin(); it != children.end(); ++it)
        delete (*it);
    children.clear();
}

static void dumpType(ZStruct*, int);
bool Parser::parse()
{
    // first off, remove all comments
    for (int i = 0; i < tokens.size(); i++)
    {
        Tokenizer::Token& tok = tokens[i];
        if (tok.type != Tokenizer::LineComment && tok.type != Tokenizer::BlockComment)
            continue;
        ParserToken ptok(tok, ParserToken::Comment);
        parsedTokens.append(ptok);
        tokens.removeAt(i);
        i--;
    }

    root = new ZFileRoot(nullptr);
    root->isValid = true;

    TokenStream stream(tokens);

    // this needs to be done in two iterations.
    // first parse includes
    // then parse defines, classes, structs and consts
    // then once all classes are known, parse contents of the classes.

    if (!parseRoot(stream))
        return false;

    //
    QList<ZStruct*> types;
    for (ZTreeNode* node : root->children)
    {
        if (node->type() == ZTreeNode::Class || node->type() == ZTreeNode::Struct)
            types.append(reinterpret_cast<ZStruct*>(node));
    }
    setTypeInformation(types);

    // for debug:
    // later this needs to be done outside of the parser after includes are processed!
    for (ZTreeNode* node : root->children)
    {
        if (node->type() == ZTreeNode::Class)
        {
            if (!parseClassFields(reinterpret_cast<ZClass*>(node)))
                return false;
        }
        else if (node->type() == ZTreeNode::Struct)
        {
            if (!parseStructFields(reinterpret_cast<ZStruct*>(node)))
                return false;
        }
    }

    // later this also needs to be done outside of the parser after all fields are processed
    for (ZTreeNode* node : root->children)
    {
        if (node->type() == ZTreeNode::Class)
        {
            if (!parseClassMethods(reinterpret_cast<ZClass*>(node)))
                return false;
        }
        else if (node->type() == ZTreeNode::Struct)
        {
            if (!parseStructMethods(reinterpret_cast<ZStruct*>(node)))
                return false;
        }
    }
}

bool Parser::skipWhitespace(TokenStream& stream, bool newline)
{
    int cpos = stream.position();
    Tokenizer::Token token;

    int lastWs = cpos;
    while (stream.readToken(token) && (token.type == Tokenizer::Whitespace || (newline && token.type == Tokenizer::Newline)))
        lastWs = stream.position();

    if (stream.position() != stream.length() && !stream.isPositionValid())
    {
        stream.setPosition(cpos);
        return false;
    }

    stream.setPosition(lastWs);
    return true;
}

bool Parser::consumeTokens(TokenStream& stream, QList<Tokenizer::Token>& out, quint64 stopAtAnyOf)
{
    out.clear();
    QList<Tokenizer::Token> stack;
    Tokenizer::Token token;
    while (stream.readToken(token))
    {
        if (!stack.size() && (token.type & stopAtAnyOf))
        {
            // don't include this.
            stream.setPosition(stream.position()-1);
            return true;
        }
        else if (token.type == Tokenizer::OpenCurly ||
                 token.type == Tokenizer::OpenSquare ||
                 token.type == Tokenizer::OpenParen)
        {
            stack.append(token);
            out.append(token);
        }
        else if (token.type == Tokenizer::CloseCurly ||
                 token.type == Tokenizer::CloseSquare ||
                 token.type == Tokenizer::CloseParen)
        {
            if (!stack.size()) // abort, return what we have
            {
                stream.setPosition(stream.position()-1);
                return true;
            }

            int expectedType = Tokenizer::Invalid;
            switch (stack.last().type)
            {
            case Tokenizer::OpenCurly:
                expectedType = Tokenizer::CloseCurly;
                break;
            case Tokenizer::OpenParen:
                expectedType = Tokenizer::CloseParen;
                break;
            case Tokenizer::OpenSquare:
                expectedType = Tokenizer::CloseSquare;
                break;
            default:
                break;
            }

            if (token.type != expectedType)
            {
                out.append(token);
                continue;
            }
            else
            {
                out.append(token);
                stack.removeLast();
            }
        }
        else
        {
            out.append(token);
        }
    }

    return true; // stream ended
}

void Parser::reportError(QString err)
{
    qDebug("Parser ERRO: %s", err.toUtf8().data());
}

void Parser::reportWarning(QString warn)
{
    qDebug("Parser WARN: %s", warn.toUtf8().data());
}

void Parser::setTypeInformation(QList<ZStruct*> _types)
{
    types = _types;
    for (ZStruct* struc : types)
    {
        // if struct is a class, we need to resolve references to other types (replaces, extends...)
        if (struc->type() != ZTreeNode::Class)
            continue;
        ZClass* cls = reinterpret_cast<ZClass*>(struc);
        if (!cls->extendName.isEmpty() || !cls->parentName.isEmpty() || !cls->replaceName.isEmpty())
        {
            for (ZStruct* struc2 : types)
            {
                if (struc2->type() != ZTreeNode::Class)
                    continue;
                ZClass* cls2 = reinterpret_cast<ZClass*>(struc2);
                if (cls2->identifier == cls->extendName)
                {
                    cls->extendReference = cls2;
                    cls2->extensions.append(cls);
                }
                if (cls2->identifier == cls->replaceName)
                {
                    cls->replaceReference = cls2;
                    cls2->replacedByReferences.append(cls);
                }
                if (cls2->identifier == cls->parentName)
                {
                    cls->parentReference = cls2;
                    cls2->childrenReferences.append(cls);
                }
            }
            if (!cls->extendName.isEmpty() && !cls->extendReference)
                qDebug("setTypeInformation: warning: extend type %s not found for class %s", cls->extendName.toUtf8().data(), cls->identifier.toUtf8().data());
            if (!cls->replaceName.isEmpty() && !cls->replaceReference)
                qDebug("setTypeInformation: warning: replaced type %s not found for class %s", cls->replaceName.toUtf8().data(), cls->identifier.toUtf8().data());
            if (!cls->parentName.isEmpty() && !cls->parentReference)
                qDebug("setTypeInformation: warning: parent type %s not found for class %s", cls->parentName.toUtf8().data(), cls->identifier.toUtf8().data());
        }
    }
}
