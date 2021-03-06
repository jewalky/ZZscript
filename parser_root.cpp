#include "parser.h"
#include <cmath>

bool Parser::parseRoot(TokenStream& stream)
{
    bool firstToken = true;
    //
    while (true)
    {
        skipWhitespace(stream, true);
        Tokenizer::Token token;
        if (!stream.readToken(token))
        {
            // end of input, suddenly
            return true;
        }

        token.makeLower();

        if (firstToken && token.type == Tokenizer::Identifier && token.value == "version")
        {
            skipWhitespace(stream, false);
            if (!stream.expectToken(token, Tokenizer::String))
            {
                qDebug("invalid version statement, expected string at line %d", token.line);
                return false;
            }

            root->version = token.value;
            firstToken = false;
            continue;
        }

        firstToken = false;

        if (token.type == Tokenizer::Preprocessor) // #
        {
            parsedTokens.append(ParserToken(token, ParserToken::Preprocessor));
            skipWhitespace(stream, false);
            if (!stream.expectToken(token, Tokenizer::Identifier))
            {
                qDebug("invalid preprocessor token at line %d", token.line);
                return false; // for now abort, but later - just ignore the token
            }

            // is #define supported?
            // answer: NO. use const
            if (token.value == "include")
            {
                parsedTokens.append(ParserToken(token, ParserToken::Preprocessor));
                skipWhitespace(stream, false);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("invalid include at line %d - expected filename", token.line);
                    return false; // for now abort, but later - just ignore the token
                }
                parsedTokens.append(ParserToken(token, ParserToken::Preprocessor));

                QSharedPointer<ZInclude> incl = QSharedPointer<ZInclude>(new ZInclude(root));
                incl->location = token.value;
                incl->isValid = true;
                root->children.append(incl);
                continue;
            }
            else
            {
                qDebug("invalid preprocessor directive '%s' at line %d", token.toCString(), token.line);
                return false; // for now abort, but later - just ignore the token
            }
        }
        else if (token.type == Tokenizer::Identifier) // class/struct for now, const later
        {
            if (token.value == "class" || token.value == "extend")
            {
                parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                bool isExtend = token.value == "extend";
                if (isExtend)
                {
                    // get "class" keyword
                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::Identifier))
                    {
                        qDebug("invalid extend class at line %d", token.line);
                        return false;
                    }

                    if (token.value != "class")
                    {
                        qDebug("unexpected '%s' at line %d, expected 'extend class'", token.toCString(), token.line);
                        return false;
                    }
                    parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                }
                QSharedPointer<ZClass> cls = parseClass(stream, isExtend);
                if (!cls)
                    return false;
                cls->parent = root;
                cls->lineNumber = token.line;
                root->children.append(cls);
            }
            else if (token.value == "struct")
            {
                parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                QSharedPointer<ZStruct> struc = parseStruct(stream, nullptr);
                if (!struc)
                    return false;
                struc->parent = root;
                struc->lineNumber = token.line;
                root->children.append(struc);
            }
            else if (token.value == "enum")
            {
                parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                QSharedPointer<ZEnum> enm = parseEnum(stream, nullptr);
                if (!enm)
                    return false;
                enm->parent = root;
                enm->lineNumber = token.line;
                root->children.append(enm);
            }
            else if (token.value == "const")
            {
                parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                QSharedPointer<ZConstant> konst = parseConstant(stream, nullptr);
                if (!konst)
                    return false;
                konst->parent = root;
                konst->lineNumber = token.line;
                root->children.append(konst);
            }
            else
            {
                qDebug("invalid identifier at top level: %s at line %d", token.toCString(), token.line);
            }
        }
    }
}

QSharedPointer<ZClass> Parser::parseClass(TokenStream& stream, bool extend)
{
    QString c_className;
    QString c_parentName;
    QString c_extendName;
    QString c_replaceName;
    QList<QString> c_flags;
    QString c_version;
    QString c_deprecated;

    // this is very simple actually, just parse name and everything between {}
    // same for structs.
    // class <name> : <name> [replaces <name>] [<flag1> [<flag2> [<flag3> ...]]] (or version("..."))
    // or:
    // extend class <name>
    //
    // the "class" is already parsed, we don't need to process it
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseClass: unexpected %s, expected class name at line %d", token.toCString(), token.line);
        return nullptr;
    }
    c_className = token.value;
    if (extend) c_extendName = c_className;
    parsedTokens.append(ParserToken(token, ParserToken::TypeName, nullptr, c_className));

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::Colon|Tokenizer::OpenCurly|Tokenizer::Identifier))
    {
        qDebug("parseClass: unexpected %s, expected parent class, replace, flag or class body at line %d", token.toCString(), token.line);
        return nullptr;
    }

    if (token.type == Tokenizer::Colon)
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected parent class name at line %d", token.toCString(), token.line);
            return nullptr;
        }
        c_parentName = token.value;
        parsedTokens.append(ParserToken(token, ParserToken::TypeName, nullptr, c_parentName));

        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected replace, flag or class body at line %d", token.toCString(), token.line);
            return nullptr;
        }
    }

    if (token.type == Tokenizer::Identifier && token.value == "replaces")
    {
        parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected replaced class name at line %d", token.toCString(), token.line);
            return nullptr;
        }
        c_replaceName = token.value;
        parsedTokens.append(ParserToken(token, ParserToken::TypeName, nullptr, c_replaceName));

        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected flag or class body at line %d", token.toCString(), token.line);
            return nullptr;
        }
    }

    if (token.type == Tokenizer::Identifier)
    {
        parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        // start of flags
        while (true)
        {
            if (token.value == "version" || token.value == "deprecated")
            {
                QString tt = token.value;
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpenParen))
                {
                    qDebug("parseClass: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("parseClass: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                if (token.value == "version")
                    c_version = token.value;
                else c_deprecated = token.value;
                parsedTokens.append(ParserToken(token, ParserToken::String));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen))
                {
                    qDebug("parseClass: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }
            else
            {
                c_flags.append(token.value);
            }

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::OpenCurly))
            {
                qDebug("parseClass: unexpected %s, expected flag or class body at line %d", token.toCString(), token.line);
                return nullptr;
            }

            if (token.type == Tokenizer::OpenCurly)
                break;
        }
    }

    if (token.type == Tokenizer::OpenCurly)
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        // rewind one token back
        QList<Tokenizer::Token> classTokens;
        if (!consumeTokens(stream, classTokens, Tokenizer::CloseCurly) || !stream.expectToken(token, Tokenizer::CloseCurly))
        {
            qDebug("parseClass: unexpected end of input");
            return nullptr;
        }
        // check if we actually finished at closing curly brace...
        if (token.type != Tokenizer::CloseCurly)
        {
            qDebug("parseClass: unexpected end of input while parsing class body; check curly braces");
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

        QSharedPointer<ZClass> cls = QSharedPointer<ZClass>(new ZClass(nullptr));
        cls->flags = c_flags;
        cls->identifier = c_className;
        cls->parentName = c_parentName;
        cls->extendName = c_extendName;
        cls->replaceName = c_replaceName;
        cls->tokens = classTokens;
        cls->version = c_version;
        cls->deprecated = c_deprecated;
        cls->isValid = true;

        // initialize "self" variable
        QSharedPointer<ZLocalVariable> self = QSharedPointer<ZLocalVariable>(new ZLocalVariable(cls));
        self->varType.type = cls->identifier;
        self->varType.reference = cls;
        self->identifier = "self";
        cls->self = self;

        return cls;
    }
    else
    {
        qDebug("parseClass: no class body found at line %d", token.line);
        return nullptr;
    }
}

QSharedPointer<ZStruct> Parser::parseStruct(TokenStream& stream, QSharedPointer<ZStruct> parent)
{
    QString s_structName;
    QList<QString> s_flags;
    QString s_version;
    QString s_deprecated;

    QString parentsPrefix = "";
    QSharedPointer<ZTreeNode> p = parent;
    while (p)
    {
        if (p->type() == ZTreeNode::Class || p->type() == ZTreeNode::Struct)
            parentsPrefix = p->identifier + "." + parentsPrefix;
        p = p->parent;
    }

    // similar to class, but even simplier.
    // struct <name> [<flag1> [<flag2> [<flag3> ...]]] (or version("..."))
    //
    // the "struct" is already parsed, we don't need to process it
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseStruct: unexpected %s, expected struct name at line %d", token.toCString(), token.line);
        return nullptr;
    }
    s_structName = token.value;
    Tokenizer::Token structName = token;

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
    {
        qDebug("parseStruct: unexpected %s, expected flag or struct body at line %d", token.toCString(), token.line);
        return nullptr;
    }

    if (token.type == Tokenizer::Identifier)
    {
        parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        // start of flags
        while (true)
        {
            if (token.value == "version" || token.value == "deprecated")
            {
                QString tt = token.value;
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpenParen))
                {
                    qDebug("parseStruct: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("parseStruct: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                if (token.value == "version")
                    s_version = token.value;
                else s_deprecated = token.value;
                parsedTokens.append(ParserToken(token, ParserToken::String));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen))
                {
                    qDebug("parseStruct: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }
            else
            {
                s_flags.append(token.value);
            }

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::OpenCurly))
            {
                qDebug("parseStruct: unexpected %s, expected flag or struct body at line %d", token.toCString(), token.line);
                return nullptr;
            }

            if (token.type == Tokenizer::OpenCurly)
                break;
        }
    }

    if (token.type == Tokenizer::OpenCurly)
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        // rewind one token back
        QList<Tokenizer::Token> classTokens;
        if (!consumeTokens(stream, classTokens, Tokenizer::CloseCurly) || !stream.expectToken(token, Tokenizer::CloseCurly))
        {
            qDebug("parseStruct: unexpected end of input");
            return nullptr;
        }
        // check if we actually finished at closing curly brace...
        if (token.type != Tokenizer::CloseCurly)
        {
            qDebug("parseStruct: unexpected end of input while parsing class body; check curly braces");
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

        QSharedPointer<ZStruct> struc = QSharedPointer<ZStruct>(new ZStruct(nullptr));
        parsedTokens.append(ParserToken(structName, ParserToken::TypeName, struc, parentsPrefix+s_structName));
        struc->flags = s_flags;
        struc->identifier = s_structName;
        struc->tokens = classTokens;
        struc->version = s_version;
        struc->deprecated = s_deprecated;
        struc->isValid = true;

        // initialize "self" variable
        QSharedPointer<ZLocalVariable> self = QSharedPointer<ZLocalVariable>(new ZLocalVariable(struc));
        self->varType.type = struc->identifier;
        self->varType.reference = struc;
        self->identifier = "self";
        struc->self = self;

        return struc;
    }
    else
    {
        qDebug("parseStruct: no class body found at line %d", token.line);
        return nullptr;
    }
}

QSharedPointer<ZEnum> Parser::parseEnum(TokenStream& stream, QSharedPointer<ZStruct> parent)
{
    QString e_enumName;
    QList<QSharedPointer<ZConstant>> e_values;

    QString parentsPrefix = "";
    QSharedPointer<ZTreeNode> p = parent;
    while (p)
    {
        if (p->type() == ZTreeNode::Class || p->type() == ZTreeNode::Struct)
            parentsPrefix = p->identifier + "." + parentsPrefix;
        p = p->parent;
    }

    // enum <name>
    // {
    //   v = ...
    //   v2,
    //   ...
    // }
    //
    // the "enum" is already parsed
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseEnum: unexpected %s, expected enum name at line %d", token.toCString(), token.line);
        return nullptr;
    }
    e_enumName = token.value;
    Tokenizer::Token enumName = token;

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::OpenCurly))
    {
        qDebug("parseEnum: unexpected %s, expected enum body at line %d", token.toCString(), token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

    while (true)
    {
        skipWhitespace(stream, true);
        // get name
        if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::CloseCurly))
        {
            qDebug("parseEnum: unexpected %s, expected closing brace or item name at line %d", token.toCString(), token.line);
            return nullptr;
        }

        if (token.type == Tokenizer::CloseCurly)
            break;

        int lineNo = token.line;
        QString enum_id = token.value;
        parsedTokens.append(ParserToken(token, ParserToken::ConstantName));
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::OpAssign|Tokenizer::CloseCurly))
        {
            qDebug("parseEnum: unexpected %s, expected closing brace, comma or value assignment at line %d", token.toCString(), token.line);
            return nullptr;
        }

        bool lastEnum = false;
        if (token.type == Tokenizer::CloseCurly)
            lastEnum = true;

        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        if (token.type == Tokenizer::OpAssign)
        {
            skipWhitespace(stream, true);
            QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::Comma|Tokenizer::CloseCurly);
            if (!expr)
            {
                qDebug("parseEnum: failed parsing expression at line %d", token.line);
                return nullptr;
            }

            QSharedPointer<ZConstant> konst = QSharedPointer<ZConstant>(new ZConstant(nullptr));
            konst->identifier = enum_id;
            // put expression into const, if any
            if (expr)
            {
                expr->parent = konst;
                konst->children.append(expr);
            }
            konst->lineNumber = lineNo;
            e_values.append(konst);

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::CloseCurly))
            {
                qDebug("parseEnum: unexpected %s, expected closing brace or comma at line %d", token.toCString(), token.line);
                return nullptr;
            }

            if (token.type == Tokenizer::CloseCurly)
                break;

            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        }
        else
        {
            QSharedPointer<ZConstant> konst = QSharedPointer<ZConstant>(new ZConstant(nullptr));
            konst->identifier = enum_id;
            konst->lineNumber = lineNo;
            e_values.append(konst);
        }

        if (lastEnum)
            break;
    }

    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
    // enum can also optionally end with a semicolon - if ported from C++
    int cpos = stream.position();
    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::Semicolon))
        stream.setPosition(cpos);
    else parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

    QSharedPointer<ZEnum> enm = QSharedPointer<ZEnum>(new ZEnum(nullptr));
    parsedTokens.append(ParserToken(enumName, ParserToken::TypeName, enm, parentsPrefix+e_enumName));
    enm->identifier = e_enumName;
    for (QSharedPointer<ZConstant> konst : e_values)
    {
        konst->parent = enm;
        enm->children.append(konst);
    }
    enm->isValid = true;
    return enm;
}

QSharedPointer<ZConstant> Parser::parseConstant(TokenStream& stream, QSharedPointer<ZStruct> struc)
{
    Tokenizer::Token token;
    stream.setPosition(stream.position()+1);
    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseConstant: unexpected %s, expected const identifier at line %d", token.toCString(), token.line);
        return nullptr;
    }
    QString c_identifier = token.value;
    parsedTokens.append(ParserToken(token, ParserToken::ConstantName));
    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::OpAssign))
    {
        qDebug("parseConstant: unexpected %s, expected assignment operator at line %d", token.toCString(), token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::Operator));
    skipWhitespace(stream, true);
    QSharedPointer<ZExpression> c_expression = parseExpression(stream, Tokenizer::Semicolon);
    if (!c_expression)
    {
        qDebug("parseConstant: expected valid const expression at line %d", token.line);
        return nullptr;
    }
    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::Semicolon))
    {
        qDebug("parseConstant: unexpected %s, expected semicolon at line %d", token.toCString(), token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
    QSharedPointer<ZConstant> konst = QSharedPointer<ZConstant>(new ZConstant(struc));
    konst->identifier = c_identifier;
    c_expression->parent = konst;
    konst->children.append(c_expression);
    konst->isValid = true;
    return konst;
}
