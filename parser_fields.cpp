#include "parser.h"
#include <cmath>

bool Parser::parseObjectFields(ZClass* cls, ZStruct* struc)
{
    if (cls)
    {
        qDebug("parseObjectFields: not implemented yet");
        return false;
    }

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
                                                       <<"virtualscope"<<"vararg"<<"final"<<"clearscope"<<"action";
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
                    if (token.value == "version" || token.value == "deprecated")
                    {
                        QString tt = token.value;
                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::OpenParen))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }

                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::String))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }
                        if (token.value == "version")
                            f_version = token.value;
                        else f_deprecated = token.value;

                        skipWhitespace(stream, true);
                        if (!stream.expectToken(token, Tokenizer::CloseParen))
                        {
                            qDebug("parseObjectFields: unexpected %s at line %d, expected %s(\"string\")", token.toCString(), token.line, tt.toUtf8().data());
                            return false;
                        }
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
            if (!parseCompoundType(stream, f_type))
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

        // now we either have open parenthesis (method) or semicolon (field)
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::OpenParen|Tokenizer::Semicolon))
        {
            qDebug("parseObjectFields: unexpected %s, expected method signature or semicolon at line %d", token.toCString(), token.line);
            return false;
        }

        //
        if (token.type == Tokenizer::Semicolon)
        {
            if (fieldTypes.size() > 1)
            {
                qDebug("parseObjectFields: multiple types in a field definition are not allowed at line %d", token.line);
                return false;
            }

            // field is done!
            ZField* field = new ZField(struc);
            field->identifier = f_name;
            field->flags = f_flags;
            field->fieldType = fieldTypes[0];
            field->version = f_version;
            field->deprecated = f_deprecated;
            field->isValid = true;
            struc->children.append(field);
        }
        else
        {
            //
            QList<ZMethod::Argument> args;
            QList<Tokenizer::Token> body;
            bool hadellipsis = false;
            while (true)
            {
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::Ellipsis|Tokenizer::Identifier))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
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

                ZCompoundType arg_type;
                if (!parseCompoundType(stream, arg_type))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: expected valid argument type at line %d", token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Identifier))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: unexpected %s, expected argument name at line %d", token.toCString(), token.line);
                    return false;
                }

                QString arg_name = token.value;

                skipWhitespace(stream, true);
                // check token, it can be either closing parenthesis or assignment
                if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::OpAssign))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: unexpected %s, expected closing parenthesis or default value at line %d", token.toCString(), token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                // parse default expression
                ZExpression* dexpr = parseExpression(stream, Tokenizer::CloseParen|Tokenizer::Comma);
                if (!dexpr)
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: expected valid default value expression for '%s' at line %d", arg_name.toUtf8().data(), token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                // expect comma or closing parenthesis now
                if (!stream.expectToken(token, Tokenizer::CloseParen|Tokenizer::Comma))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: unexpected %s, expected closing parenthesis or comma at line %d", token.toCString(), token.line);
                    return false;
                }

                ZMethod::Argument arg;
                arg.name = arg_name;
                arg.defaultValue = dexpr;
                arg.type = arg_type;
                args.append(arg);
            }

            // check for "const" after signature
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Identifier|Tokenizer::OpenCurly|Tokenizer::Semicolon))
            {
                for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                qDebug("parseObjectFields: unexpected %s, expected 'const', semicolon or method body at line %d", token.toCString(), token.line);
                return false;
            }

            if (token.type == Tokenizer::Identifier)
            {
                if (token.value == "const")
                {
                    f_flags.append("const");
                }
                else
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: invalid method flag %s, expected 'const' at line %d", token.toCString(), token.line);
                    return false;
                }

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpenCurly|Tokenizer::Semicolon))
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
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
                if (!consumeTokens(stream, body, Tokenizer::CloseCurly) || !stream.peekToken(token) || token.type != Tokenizer::CloseCurly)
                {
                    for (auto& arg : args) if (arg.defaultValue) delete arg.defaultValue;
                    qDebug("parseObjectFields: unexpected end of input for method body (method %s)", f_name.toUtf8().data());
                    return false;
                }

                stream.setPosition(stream.position()+1);
            }

            // we have all data about method
            ZMethod* method = new ZMethod(struc);
            method->identifier = f_name;
            method->flags = f_flags;
            method->returnTypes = fieldTypes;
            method->version = f_version;
            method->deprecated = f_deprecated;
            method->arguments = args;
            method->hasEllipsis = hadellipsis;
            method->tokens = body;
            method->isValid = true;
            // for destructor to work
            for (auto& arg : args)
            {
                if (arg.defaultValue)
                {
                    arg.defaultValue->parent = method;
                    method->children.append(arg.defaultValue);
                }
            }
            struc->children.append(method);
        }
    }
}

bool Parser::parseCompoundType(TokenStream& stream, ZCompoundType& type)
{
    // get initial identifier
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::Identifier))
    {
        qDebug("parseCompoundType: expected identifier at line %d", token.line);
        return false;
    }

    type.type = token.value;
    type.reference = nullptr;

    int cpos = stream.position();
    skipWhitespace(stream, true);
    if (stream.expectToken(token, Tokenizer::OpLessThan))
    {
        while (true)
        {
            skipWhitespace(stream, true);
            // more types!
            ZCompoundType subType;
            if (!parseCompoundType(stream, subType))
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
            //
            type.arguments.append(subType);
            //
            if (token.type == Tokenizer::OpGreaterThan)
                break; // done
        }
    }
    else stream.setPosition(cpos);
    return true;
}
