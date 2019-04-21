#include "parser.h"
#include <cmath>

QList<ZSystemType> Parser::systemTypes = QList<ZSystemType>()
        << ZSystemType("string", ZSystemType::SType_String, 0, "StringStruct")
        << ZSystemType("name", ZSystemType::SType_Integer, 32)
        << ZSystemType("uint", ZSystemType::SType_Integer, 32)
        << ZSystemType("int", ZSystemType::SType_Integer, 32)
        << ZSystemType("uint16", ZSystemType::SType_Integer, 16)
        << ZSystemType("int16", ZSystemType::SType_Integer, 16)
        << ZSystemType("uint8", ZSystemType::SType_Integer, 8)
        << ZSystemType("int8", ZSystemType::SType_Integer, 8)
        << ZSystemType("sbyte", ZSystemType::SType_Integer, 8)
        << ZSystemType("byte", ZSystemType::SType_Integer, 8)
        << ZSystemType("short", ZSystemType::SType_Integer, 16)
        << ZSystemType("ushort", ZSystemType::SType_Integer, 16)
        << ZSystemType("double", ZSystemType::SType_Float, 64)
        << ZSystemType("float", ZSystemType::SType_Float, 32)
        << ZSystemType("float64", ZSystemType::SType_Float, 64)
        << ZSystemType("float32", ZSystemType::SType_Float, 32)
        << ZSystemType("color", ZSystemType::SType_Integer, 32)
        << ZSystemType("vector2", ZSystemType::SType_Vector, 2)
        << ZSystemType("vector3", ZSystemType::SType_Vector, 3)
        << ZSystemType("array", ZSystemType::SType_Array, 0)
        << ZSystemType("class", ZSystemType::SType_Class, 0)
        << ZSystemType("readonly", ZSystemType::SType_Readonly, 0)
        << ZSystemType("bool", ZSystemType::SType_Integer, 8)
        << ZSystemType("sound", ZSystemType::SType_String, 0)
        << ZSystemType("spriteid", ZSystemType::SType_Integer, 32)
        << ZSystemType("state", ZSystemType::SType_Object, 0)
        << ZSystemType("statelabel", ZSystemType::SType_String, 0)
        << ZSystemType("textureid", ZSystemType::SType_Integer, 32)
        << ZSystemType("void", ZSystemType::SType_Void, 0)
        << ZSystemType("voidptr", ZSystemType::SType_Object, 0);

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
    root->parser = this;
    root->isValid = true;

    TokenStream stream(tokens);

    // this needs to be done in two iterations.
    // first parse includes
    // then parse defines, classes, structs and consts
    // then once all classes are known, parse contents of the classes.

    if (!parseRoot(stream))
        return false;

    return true;
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

void Parser::setTypeInformation(QList<ZTreeNode*> _types)
{
    types = _types;
    for (ZTreeNode* struc : types)
    {
        // if struct is a class, we need to resolve references to other types (replaces, extends...)
        if (struc->type() != ZTreeNode::Class)
            continue;
        ZClass* cls = reinterpret_cast<ZClass*>(struc);
        if (!cls->extendName.isEmpty() || !cls->parentName.isEmpty() || !cls->replaceName.isEmpty())
        {
            for (ZTreeNode* struc2 : types)
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

    // go through parsed tokens and find types. and resolve if needed
    for (ParserToken& token : parsedTokens)
    {
        if (token.type == ParserToken::TypeName)
        {
            ZTreeNode* resolved = resolveType(token.referencePath);
            if (resolved) token.reference = resolved;
            else qDebug("setTypeInformation: warning: unresolved type %s", token.referencePath.toUtf8().data());
        }
    }
}

ZTreeNode* Parser::resolveType(QString name, ZStruct* context, bool onlycontext)
{
    if (!onlycontext && name.toLower() == "string")
        name = "stringstruct"; // this is because of ZScript hack

    // find in context if applicable
    QList<QString> nameParts = name.split(".");
    if (context)
    {
        if (!onlycontext && !context->identifier.compare(name, Qt::CaseInsensitive))
            return context;
        // search for local type name
        for (ZTreeNode* node : context->children)
        {
            if ((node->type() == ZTreeNode::Struct || node->type() == ZTreeNode::Class || node->type() == ZTreeNode::Enum)
                    && !node->identifier.compare(nameParts[0], Qt::CaseInsensitive))
            {
                //qDebug("return item %s from context %s", node->identifier.toUtf8().data(), context->identifier.toUtf8().data());
                return node;
            }
        }
    }

    if (onlycontext) return nullptr;

    // search global type scope
    for (ZTreeNode* node : types)
    {
        if ((node->type() == ZTreeNode::Struct || node->type() == ZTreeNode::Class || node->type() == ZTreeNode::Enum)
                && !node->identifier.compare(nameParts[0], Qt::CaseInsensitive))
        {
            if (nameParts.size() == 1)
                return node; // type found
            else if (node->type() == ZTreeNode::Struct || node->type() == ZTreeNode::Class)
                return resolveType(nameParts.mid(1).join("."), reinterpret_cast<ZStruct*>(node), true);
        }
    }

    return nullptr; // not found
}

ZSystemType* Parser::resolveSystemType(QString name)
{
    for (ZSystemType& t : systemTypes)
    {
        if (!t.identifier.compare(name, Qt::CaseInsensitive))
            return &t;
    }

    return nullptr;
}

ZTreeNode* Parser::resolveSymbol(QString name, ZTreeNode* parent, ZStruct* context)
{
    if (name == "self")
    {
        if (context) return context->self;
        return nullptr; // invalid
    }

    // first, look in all parent scopes
    ZTreeNode* p = parent;
    while (p)
    {
        if (p->type() == ZTreeNode::ForCycle) // for cycle also has initializer.
        {
            // check if it's a variable in the initializer
            ZForCycle* forCycle = reinterpret_cast<ZForCycle*>(p);
            for (ZTreeNode* node : forCycle->initializers)
            {
                if (node->type() == ZTreeNode::LocalVariable && !node->identifier.compare(name, Qt::CaseInsensitive))
                    return node; // found local variable from For initializer
            }
        }
        else if (p->type() == ZTreeNode::Method) // method also has arguments.
        {
            ZMethod* method = reinterpret_cast<ZMethod*>(p);
            for (ZLocalVariable* var : method->arguments)
            {
                if (!var->identifier.compare(name, Qt::CaseInsensitive))
                    return var;
            }
        }

        // look for local variable definition in the block
        for (ZTreeNode* node : p->children)
        {
            if (node->type() == ZTreeNode::LocalVariable && !node->identifier.compare(name, Qt::CaseInsensitive))
                return node; // found local variable in block
        }

        p = p->parent;
    }

    // check context fields
    if (context)
    {
        for (ZTreeNode* node : context->children)
        {
            if ((node->type() == ZTreeNode::Field ||
                 node->type() == ZTreeNode::Method ||
                 node->type() == ZTreeNode::Constant) && !node->identifier.compare(name, Qt::CaseInsensitive))
                return node;
        }
    }

    // todo check global constants, but not yet

    return nullptr;
}

QList<ZTreeNode*> Parser::getOwnTypeInformation()
{
    QList<ZTreeNode*> types;
    for (ZTreeNode* node : root->children)
    {
        if (node->type() == ZTreeNode::Class || node->type() == ZTreeNode::Struct || node->type() == ZTreeNode::Enum)
            types.append(reinterpret_cast<ZStruct*>(node));
    }
    return types;
}

QList<ZTreeNode*> Parser::getTypeInformation()
{
    return types;
}

ZStruct::~ZStruct()
{
    if (self) delete self;
    self = nullptr;
}
