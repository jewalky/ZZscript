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

bool Parser::parse()
{
    // first off, remove all comments
    for (int i = 0; i < tokens.size(); i++)
    {
        Tokenizer::Token& tok = tokens[i];
        if (tok.type != Tokenizer::LineComment && tok.type != Tokenizer::BlockComment)
            continue;
        ParserToken ptok = tok;
        ptok.type = ParserToken::Comment;
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
