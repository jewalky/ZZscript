#include "parser.h"
#include <cmath>

bool Parser::parseObjectFields(QSharedPointer<ZClass> cls, QSharedPointer<ZStruct> struc)
{
    // at this point, we have a list of tokens contained inside the struct/class body.
    // there, we have values in one of the forms:
    // 1)
    //    <flags> <type> <name>;
    // 2)
    //    <flags> <type> [, <type> ...] <name> ( <type> <name> [= <expression>] [, ...] ) [ const ] { ... }
    // there are also meta fields but we don't parse these for now.
    TokenStream stream(struc->tokens);
    Tokenizer::Token token;
    while (true)
    {
        skipWhitespace(stream, true);
        QString f_version;
        QString f_deprecated;
        QList<QString> f_flags;
        // there are specific keywords recognized as field and method flags.
        // it's required to specify them here, otherwise we cannot really distingiush between flags and other parts of the descriptor.
        // (because our parser is not backwards)
        QList<QString> allowedKeywords = QList<QString>()<<"deprecated"<<"internal"<<"latent"<<"meta"<<"native"<<"play"<<"private"
                                                        <<"protected"<<"readonly"<<"transient"<<"ui"<<"version"<<"virtual"<<"override"
                                                       <<"virtualscope"<<"vararg"<<"final"<<"clearscope"<<"action"<<"static"<<"const";

        if (!stream.peekToken(token))
            break;
        token.makeLower();
        int lineno = token.line;

        if (token.value == "enum")
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            stream.setPosition(stream.position()+1);
            QSharedPointer<ZEnum> enm = parseEnum(stream, struc);
            if (!enm)
                return false;
            enm->parent = struc;
            enm->lineNumber = lineno;
            struc->children.append(enm);
            continue;
        }
        else if (token.value == "struct")
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            stream.setPosition(stream.position()+1);
            QSharedPointer<ZStruct> subStruc = parseStruct(stream, struc);
            if (!subStruc)
                return false;
            subStruc->parent = struc;
            subStruc->lineNumber = lineno;
            struc->children.append(subStruc);
            continue;
        }
        else if (token.value == "const")
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            // read in const value
            // const <name> = <expression>;
            QSharedPointer<ZConstant> konst = parseConstant(stream, struc);
            if (!konst)
                return false;
            konst->parent = struc;
            konst->lineNumber = lineno;
            struc->children.append(konst);
            continue;
        }
        else if (token.value == "property")
        {
            // read in property expression
            // property <name> : <field1> [, <field2> ...]
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            stream.setPosition(stream.position()+1);
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Identifier))
            {
                qDebug("parseObjectFields: unexpected %s, expected property identifier at line %d", token.toCString(), token.line);
                return false;
            }
            QString prop_identifier = token.value;
            parsedTokens.append(ParserToken(token, ParserToken::Field));
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Colon))
            {
                qDebug("parseObjectFields: unexpected %s, expected : at line %d", token.toCString(), token.line);
                return false;
            }
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            QList<QString> prop_fields;
            while (true)
            {
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::Semicolon))
                {
                    qDebug("parseObjectFields: unexpected %s, expected identifier or semicolon at line %d", token.toCString(), token.line);
                    return false;
                }
                if (token.type == Tokenizer::Semicolon)
                    break;
                parsedTokens.append(ParserToken(token, ParserToken::Field));
                prop_fields.append(token.value);
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::Semicolon))
                {
                    qDebug("parseObjectFields: unexpected %s, expected comma or semicolon at line %d", token.toCString(), token.line);
                    return false;
                }
                if (token.type == Tokenizer::Semicolon)
                    break;
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }
            if (!prop_fields.size())
                qDebug("parseObjectFields: warning: property '%s' without fields at line %d", prop_identifier.toUtf8().data(), token.line);
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken)); // semicolon
            QSharedPointer<ZProperty> prop = QSharedPointer<ZProperty>(new ZProperty(struc));
            prop->identifier = prop_identifier;
            prop->fields = prop_fields;
            prop->lineNumber = lineno;
            prop->isValid = true;
            struc->children.append(prop);
            continue;
        }
        else if (token.value == "default") // no processing yet, just to make Doom classes work
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            stream.setPosition(stream.position()+1);
            // read in a block
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::OpenCurly))
            {
                qDebug("parseObjectFields: unexpected %s, expected opening curly brace at line %d", token.toCString(), token.line);
                return false;
            }
            skipWhitespace(stream, true);
            QList<Tokenizer::Token> _;
            consumeTokens(stream, _, Tokenizer::CloseCurly);
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::CloseCurly))
            {
                qDebug("parseObjectFields: unexpected %s, expected closing curly brace at line %d", token.toCString(), token.line);
                return false;
            }
            continue;
        }
        else if (token.value == "states")
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            stream.setPosition(stream.position()+1);
            // read in a block
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::OpenCurly))
            {
                qDebug("parseObjectFields: unexpected %s, expected opening curly brace at line %d", token.toCString(), token.line);
                return false;
            }
            skipWhitespace(stream, true);
            QList<Tokenizer::Token> _;
            consumeTokens(stream, _, Tokenizer::CloseCurly);
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::CloseCurly))
            {
                qDebug("parseObjectFields: unexpected %s, expected closing curly brace at line %d", token.toCString(), token.line);
                return false;
            }
            continue;
        }

        //  <allowedKeywords...> <type> ...
        bool nothingread = true;
        while (true)
        {
            skipWhitespace(stream, true);
            if (!stream.readToken(token))
            {
                if (nothingread)
                    return true; // done
                qDebug("parseObjectFields: unexpected end of input at line %d", token.line);
                return false;
            }

            nothingread = false;

            if (token.type == Tokenizer::Identifier)
            {
                if (allowedKeywords.contains(token.value))
                {
                    parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                    if (token.value == "version" || token.value == "deprecated")
                    {
                        QString tt = token.value;
                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::OpenParen))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }
                        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::String))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }
                        if (token.value == "version")
                            f_version = token.value;
                        else f_deprecated = token.value;
                        parsedTokens.append(ParserToken(token, ParserToken::String));

                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::CloseParen))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }
                        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                    }
                    else
                    {
                        f_flags.append(token.value);
                    }
                }
                else
                {
                    stream.setPosition(stream.position()-1);
                    break;
                }
            }
            else
            {
                qDebug("parseObjectFields: unexpected %s, expected flags or type at line %d", token.toCString(), token.line);
                return false;
            }
        }

        // flags are done, read type(s)
        QList<ZCompoundType> fieldTypes;
        while (true)
        {
            skipWhitespace(stream, true);
            ZCompoundType f_type;
            if (!parseCompoundType(stream, f_type, struc))
            {
                qDebug("parseObjectFields: expected valid type at line %d", token.line);
                return false;
            }

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::Identifier))
            {
                qDebug("parseObjectFields: unexpected %s, expected comma or identifier at line %d", token.toCString(), token.line);
                return false;
            }

            fieldTypes.append(f_type);
            if (token.type != Tokenizer::Comma)
                break; // done with types
        }

        // types done, read name
        skipWhitespace(stream, true);
        QString f_name = token.value;
        Tokenizer::Token fieldNameToken = token;

        // now we either have open parenthesis (method) or semicolon (field)
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenParen|Tokenizer::Semicolon|Tokenizer::OpenSquare|Tokenizer::OpAssign))
        {
            qDebug("parseObjectFields: unexpected %s, expected method signature, array dimensions or semicolon at line %d", token.toCString(), token.line);
            return false;
        }

        //
        if (token.type == Tokenizer::OpenSquare)
        {
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            // parse array dimensons, can have many
            while (true)
            {
                skipWhitespace(stream, true);
                int cpos = stream.position();
                QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::CloseSquare);
                if (!expr)
                {
                    // check if next token is ], then null expr is pretty valid and means "just guess"
                    stream.setPosition(cpos);
                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::CloseSquare))
                    {
                        qDebug("parseObjectFields: expected valid expression for array dimensions at line %d", token.line);
                        return false;
                    }
                    stream.setPosition(stream.position()-1);
                }
                fieldTypes[0].arrayDimensions.append(expr);
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseSquare))
                {
                    qDebug("parseObjectFields: unexpected end of input, closing square brace at line %d", token.line);
                    return false;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                // next dimension or end of def
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Semicolon|Tokenizer::OpenSquare|Tokenizer::OpAssign))
                {
                    qDebug("parseObjectFields: unexpected %s, expected semicolon or array dimensions at line %d", token.toCString(), token.line);
                    return false;
                }
                if (token.type == Tokenizer::Semicolon || token.type == Tokenizer::OpAssign)
                    break; // field is done
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }
        }

        //
        if (token.type == Tokenizer::Semicolon || token.type == Tokenizer::OpAssign)
        {
            // check assignment
            QSharedPointer<ZExpression> assignmentExpr = nullptr;
            if (token.type == Tokenizer::OpAssign)
            {
                // assignment token
                parsedTokens.append(ParserToken(token, ParserToken::Operator));
                //
                skipWhitespace(stream, true);
                assignmentExpr = parseExpression(stream, Tokenizer::Semicolon);
                if (!assignmentExpr)
                {
                    qDebug("parseObjectFields: expected valid expression at line %d", token.line);
                    return false;
                }
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Semicolon))
                {
                    qDebug("parseObjectFields: unexpected %s, expected semicolon at line %d", token.toCString(), token.line);
                    return false;
                }
            }
            // semicolon token
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            // field name
            parsedTokens.append(ParserToken(fieldNameToken, ParserToken::Field));

            if (fieldTypes.size() > 1)
            {
                qDebug("parseObjectFields: multiple types in a field definition are not allowed at line %d", token.line);
                return false;
            }

            // field is done!
            QSharedPointer<ZField> field = QSharedPointer<ZField>(new ZField(struc));
            field->identifier = f_name;
            field->flags = f_flags;
            field->fieldType = fieldTypes[0];
            field->version = f_version;
            field->deprecated = f_deprecated;
            field->lineNumber = lineno;
            field->isValid = true;
            struc->children.append(field);
        }
        else
        {
            // opening parenthesis
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            // method name
            parsedTokens.append(ParserToken(fieldNameToken, ParserToken::Method));
            //
            QList<QSharedPointer<ZLocalVariable>> args;
            QList<Tokenizer::Token> body;
            bool hadellipsis = false;
            while (true)
            {
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::Ellipsis|Tokenizer::Identifier))
                {
                    qDebug("parseObjectFields: unexpected %s, closing parenthesis, ellipsis or argument at line %d", token.toCString(), token.line);
                    return false;
                }

                if (token.type == Tokenizer::CloseParen)
                    break; // args are done
                if (token.type == Tokenizer::Ellipsis)
                {
                    hadellipsis = true;
                    break;
                }

                // check if "out" or "ref"
                token.makeLower();
                bool arg_isOut = false;
                bool arg_isRef = false;
                if (token.value == "out" || token.value == "ref")
                {
                    if (token.value == "out")
                        arg_isOut = true;
                    if (token.value == "ref")
                        arg_isRef = true;
                    parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                    skipWhitespace(stream, true);
                }
                else stream.setPosition(stream.position()-1);
                // not sure if we can have "out ref", todo: check and change the code if needed

                ZCompoundType arg_type;
                if (!parseCompoundType(stream, arg_type, struc))
                {
                    qDebug("parseObjectFields: expected valid argument type at line %d", token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Identifier))
                {
                    qDebug("parseObjectFields: unexpected %s, expected argument name at line %d", token.toCString(), token.line);
                    return false;
                }

                QString arg_name = token.value;
                parsedTokens.append(ParserToken(token, ParserToken::Argument));

                skipWhitespace(stream, true);
                // check token, it can be either closing parenthesis or assignment
                if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::OpAssign|Tokenizer::Comma))
                {
                    qDebug("parseObjectFields: unexpected %s, expected closing parenthesis or default value at line %d", token.toCString(), token.line);
                    return false;
                }

                QSharedPointer<ZExpression> dexpr = nullptr;
                if (token.type == Tokenizer::OpAssign)
                {
                    parsedTokens.append(ParserToken(token, ParserToken::Operator));
                    skipWhitespace(stream, true);
                    // parse default expression
                    dexpr = parseExpression(stream, Tokenizer::CloseParen|Tokenizer::Comma);
                    if (!dexpr)
                    {
                        qDebug("parseObjectFields: expected valid default value expression for '%s' at line %d", arg_name.toUtf8().data(), token.line);
                        return false;
                    }
                    highlightExpression(dexpr, nullptr, struc);

                    skipWhitespace(stream, true);
                    // expect comma or closing parenthesis now
                    if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::Comma))
                    {
                        qDebug("parseObjectFields: unexpected %s, expected closing parenthesis or comma at line %d", token.toCString(), token.line);
                        return false;
                    }
                }

                QSharedPointer<ZLocalVariable> arg = QSharedPointer<ZLocalVariable>(new ZLocalVariable(nullptr));
                arg->identifier = arg_name;
                if (dexpr)
                {
                    dexpr->parent = arg;
                    arg->children.append(dexpr);
                }
                arg->hasType = true;
                arg->varType = arg_type;
                if (arg_isOut)
                    arg->flags.append("out");
                if (arg_isRef)
                    arg->flags.append("ref");
                args.append(arg);

                if (token.type == Tokenizer::CloseParen)
                    break;
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }

            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken)); // closing parenthesis

            // check for "const" after signature
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::OpenCurly|Tokenizer::Semicolon))
            {
                qDebug("parseObjectFields: unexpected %s, expected 'const', semicolon or method body at line %d", token.toCString(), token.line);
                return false;
            }

            if (token.type == Tokenizer::Identifier)
            {
                if (token.value == "const")
                {
                    parsedTokens.append(ParserToken(token, ParserToken::Keyword));
                    f_flags.append("const");
                }
                else
                {
                    qDebug("parseObjectFields: invalid method flag %s, expected 'const' at line %d", token.toCString(), token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Semicolon))
                {
                    qDebug("parseObjectFields: unexpected %s, expected semicolon or method body at line %d", token.toCString(), token.line);
                    return false;
                }
            }

            if (token.type == Tokenizer::Semicolon)
            {
                //
                if (!f_flags.contains("native"))
                {
                    qDebug("parseObjectFields: warning: non-native function without body: %s at line %d", f_name.toUtf8().data(), token.line);
                }
            }
            else
            {
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                if (!consumeTokens(stream, body, Tokenizer::CloseCurly) || !stream.peekToken(token) || token.type != Tokenizer::CloseCurly)
                {
                    qDebug("parseObjectFields: unexpected end of input for method body (method %s)", f_name.toUtf8().data());
                    return false;
                }
                if (!stream.expectToken(token, Tokenizer::CloseCurly))
                {
                    qDebug("parseObjectFields: unexpected %s, expected closing curly brace at line %d", token.toCString(), token.line);
                    return false;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

                stream.setPosition(stream.position()+1);
            }

            // we have all data about method
            QSharedPointer<ZMethod> method = QSharedPointer<ZMethod>(new ZMethod(struc));
            method->identifier = f_name;
            method->flags = f_flags;
            method->returnTypes = fieldTypes;
            method->version = f_version;
            method->deprecated = f_deprecated;
            method->arguments = args;
            method->hasEllipsis = hadellipsis;
            method->tokens = body;
            method->lineNumber = lineno;
            method->isValid = true;
            // for destructor and expressions to work
            for (QSharedPointer<ZLocalVariable> arg : args)
                arg->parent = method;
            struc->children.append(method);
        }
    }

    // success here means that we don't need tokens anymore. free memory
    tokens.clear();

    bool allok = true;
    // now that all object fields are parsed, we need to also call this operation on subobjects (embeded structs for now)
    for (QSharedPointer<ZTreeNode> node : struc->children)
    {
        if (node->type() == ZTreeNode::Struct)
            allok &= parseStructFields(node.dynamicCast<ZStruct>());
        else if (node->type() == ZTreeNode::Class) // wtf?
            allok &= parseClassFields(node.dynamicCast<ZClass>());
    }

    return allok;
}

bool Parser::parseCompoundType(TokenStream& stream, ZCompoundType& type, QSharedPointer<ZStruct> context)
{
    // get initial identifier
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseCompoundType: expected identifier at line %d", token.line);
        return false;
    }

    QSharedPointer<ZStruct> iterContext = context;
    QString prependContext = "";
    do
    {
        prependContext = iterContext->identifier + "." + prependContext;
        QSharedPointer<ZTreeNode> p = iterContext->parent;
        iterContext = (p && (p->type() == ZTreeNode::Class || p->type() == ZTreeNode::Struct)) ? p.dynamicCast<ZStruct>() : nullptr;
    }
    while (iterContext);

    // resolve system type
    QSharedPointer<ZSystemType> systemType = resolveSystemType(token.value);
    if (systemType)
    {
        type.type = token.value.toLower();
        type.reference = systemType;

        parsedTokens.append(ParserToken(token, ParserToken::TypeName, systemType, token.value));
    }
    else
    {
        QSharedPointer<ZTreeNode> lastType = resolveType(token.value, context);
        if (!lastType)
        {
            qDebug("parseCompoundType: warning: unresolved type %s", token.value.toUtf8().data());
        }

        if (lastType && (!lastType->parent.toStrongRef() || lastType->parent.toStrongRef()->type() == ZTreeNode::FileRoot))
            prependContext = "";

        if (!lastType)
        {
            parsedTokens.append(ParserToken(token, ParserToken::TypeName, lastType, prependContext+token.value));
        }
        else
        {
            parsedTokens.append(ParserToken(token, ParserToken::TypeName, lastType, getFullType(lastType)));
        }

        // check for multi-component type (i.e. A.B.C)
        QString fullType = token.value;
        int cpos;
        while (true)
        {
            cpos = stream.position();
            skipWhitespace(stream, true);
            if (stream.peekToken(token) && token.type == Tokenizer::Dot)
            {
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                stream.setPosition(stream.position()+1);
                if (!stream.expectToken(token, Tokenizer::Identifier))
                {
                    qDebug("parseCompoundType: expected identifer at line %d", token.line);
                    return false;
                }
                fullType += "."+token.value;
                lastType = resolveType(fullType, context);
                if (!lastType)
                {
                    qDebug("parseCompoundType: warning: unresolved type %s", fullType.toUtf8().data());
                    parsedTokens.append(ParserToken(token, ParserToken::TypeName, lastType, prependContext+fullType));
                }
                else
                {
                    parsedTokens.append(ParserToken(token, ParserToken::TypeName, lastType, getFullType(lastType)));
                }
            }
            else
            {
                stream.setPosition(cpos);
                break;
            }
        }

        if (!lastType)
        {
            type.type = fullType;
            type.reference = lastType;
        }
        else
        {
            type.type = getFullType(lastType);
            type.reference = lastType;
        }
    }

    int cpos = stream.position();
    skipWhitespace(stream, true);
    if (stream.expectToken(token, Tokenizer::OpLessThan))
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        while (true)
        {
            skipWhitespace(stream, true);
            // more types!
            ZCompoundType subType;
            if (!parseCompoundType(stream, subType, context))
            {
                qDebug("parseCompoundType: expected valid subtype at line %d", token.line);
                return false;
            }
            //
            if (!stream.expectToken(token, Tokenizer::OpGreaterThan|Tokenizer::Comma))
            {
                qDebug("parseCompoundType: expected close brace or comma at line %d", token.line);
                return false;
            }
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            //
            type.arguments.append(subType);
            //
            if (token.type == Tokenizer::OpGreaterThan)
                break; // done
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
    }
    else stream.setPosition(cpos);
    return true;
}
