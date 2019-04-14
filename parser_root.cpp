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
                skipWhitespace(stream, false);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("invalid include at line %d - expected filename", token.line);
                    return false; // for now abort, but later - just ignore the token
                }

                ZInclude* incl = new ZInclude(root);
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
                }
                ZClass* cls = parseClass(stream, isExtend);
                if (!cls)
                    return false;
                cls->parent = root;
                root->children.append(cls);
            }
            else if (token.value == "struct")
            {
                ZStruct* struc = parseStruct(stream);
                if (!struc)
                    return false;
                struc->parent = root;
                root->children.append(struc);
            }
            else if (token.value == "enum")
            {
                ZEnum* enm = parseEnum(stream);
                if (!enm)
                    return false;
                enm->parent = root;
                root->children.append(enm);
            }
            else
            {
                qDebug("invalid identifier at top level: %s at line %d", token.toCString(), token.line);
            }
        }
    }
}

ZClass* Parser::parseClass(TokenStream& stream, bool extend)
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

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::Colon|Tokenizer::OpenCurly|Tokenizer::Identifier))
    {
        qDebug("parseClass: unexpected %s, expected parent class, replace, flag or class body at line %d", token.toCString(), token.line);
        return nullptr;
    }

    if (token.type == Tokenizer::Colon)
    {
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected parent class name at line %d", token.toCString(), token.line);
            return nullptr;
        }
        c_parentName = token.value;

        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected replace, flag or class body at line %d", token.toCString(), token.line);
            return nullptr;
        }
    }

    if (token.type == Tokenizer::Identifier && token.value == "replaces")
    {
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected replaced class name at line %d", token.toCString(), token.line);
            return nullptr;
        }
        c_replaceName = token.value;

        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
        {
            qDebug("parseClass: unexpected %s, expected flag or class body at line %d", token.toCString(), token.line);
            return nullptr;
        }
    }

    if (token.type == Tokenizer::Identifier)
    {
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

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("parseClass: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                if (token.value == "version")
                    c_version = token.value;
                else c_deprecated = token.value;

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen))
                {
                    qDebug("parseClass: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
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

        ZClass* cls = new ZClass(nullptr);
        cls->flags = c_flags;
        cls->identifier = c_className;
        cls->parentName = c_parentName;
        cls->extendName = c_extendName;
        cls->replaceName = c_replaceName;
        cls->tokens = classTokens;
        cls->version = c_version;
        cls->deprecated = c_deprecated;
        cls->isValid = true;
        return cls;
    }
    else
    {
        qDebug("parseClass: no class body found at line %d", token.line);
        return nullptr;
    }
}

ZStruct* Parser::parseStruct(TokenStream& stream)
{
    QString s_structName;
    QList<QString> s_flags;
    QString s_version;
    QString s_deprecated;

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

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Identifier))
    {
        qDebug("parseStruct: unexpected %s, expected flag or struct body at line %d", token.toCString(), token.line);
        return nullptr;
    }

    if (token.type == Tokenizer::Identifier)
    {
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

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::String))
                {
                    qDebug("parseStruct: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
                if (token.value == "version")
                    s_version = token.value;
                else s_deprecated = token.value;

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen))
                {
                    qDebug("parseStruct: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                    return nullptr;
                }
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

        ZStruct* struc = new ZStruct(nullptr);
        struc->flags = s_flags;
        struc->identifier = s_structName;
        struc->tokens = classTokens;
        struc->version = s_version;
        struc->deprecated = s_deprecated;
        struc->isValid = true;
        return struc;
    }
    else
    {
        qDebug("parseStruct: no class body found at line %d", token.line);
        return nullptr;
    }
}

ZEnum* Parser::parseEnum(TokenStream& stream)
{
    QString e_enumName;
    QList< QPair<QString, ZExpression*> > e_values;

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

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::OpenCurly))
    {
        qDebug("parseEnum: unexpected %s, expected enum body at line %d", token.toCString(), token.line);
        return nullptr;
    }

    while (true)
    {
        skipWhitespace(stream, true);
        // get name
        if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::CloseCurly))
        {
            for (auto& e : e_values)
                delete e.second;
            qDebug("parseEnum: unexpected %s, expected closing brace or item name at line %d", token.toCString(), token.line);
            return nullptr;
        }

        if (token.type == Tokenizer::CloseCurly)
            break;

        QString enum_id = token.value;
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::OpAssign|Tokenizer::CloseCurly))
        {
            for (auto& e : e_values)
                delete e.second;
            qDebug("parseEnum: unexpected %s, expected closing brace, comma or value assignment at line %d", token.toCString(), token.line);
            return nullptr;
        }

        if (token.type == Tokenizer::CloseCurly)
            break; // done, valid enum

        if (token.type == Tokenizer::OpAssign)
        {
            skipWhitespace(stream, true);
            ZExpression* expr = parseExpression(stream, Tokenizer::Comma|Tokenizer::CloseCurly);
            if (!expr)
            {
                for (auto& e : e_values)
                    delete e.second;
                qDebug("parseEnum: failed parsing expression at line %d", token.line);
                return nullptr;
            }

            e_values.append(QPair<QString, ZExpression*>(enum_id, expr));

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::CloseCurly))
            {
                for (auto& e : e_values)
                    delete e.second;
                qDebug("parseEnum: unexpected %s, expected closing brace or comma at line %d", token.toCString(), token.line);
                return nullptr;
            }

            if (token.type == Tokenizer::CloseCurly)
                break;
        }
    }

    ZEnum* enm = new ZEnum(nullptr);
    enm->identifier = e_enumName;
    enm->values = e_values;
    // this is so that destructor works correctly
    for (auto& e : e_values)
    {
        e.second->parent = enm;
        enm->children.append(e.second);
    }
    enm->isValid = true;
    return enm;
}