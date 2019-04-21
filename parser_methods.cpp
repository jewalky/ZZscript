#include "parser.h"
#include <cmath>

bool Parser::parseObjectMethods(QSharedPointer<ZClass> cls, QSharedPointer<ZStruct> struc)
{
    // go through enums
    for (QSharedPointer<ZTreeNode> node : struc->children)
    {
        if (node->type() != ZTreeNode::Enum)
            continue;

        QSharedPointer<ZEnum> enm = node.dynamicCast<ZEnum>();
        parseEnumExpressions(enm, struc);
    }

    // go through methods
    for (QSharedPointer<ZTreeNode> node : struc->children)
    {
        if (node->type() != ZTreeNode::Method)
            continue;

        // parse method
        QSharedPointer<ZMethod> method = node.dynamicCast<ZMethod>();
        TokenStream stream(method->tokens);
        QSharedPointer<ZCodeBlock> rootBlock = parseCodeBlock(stream, nullptr, struc);
        if (!rootBlock)
        {
            qDebug("parseObjectMethods: failed to parse '%s'", method->identifier.toUtf8().data());
            continue;
        }
        rootBlock->parent = method;
        method->children.append(rootBlock);
    }

    // go through subobjects (embedded structs) and parse their methods too
    bool allok = true;
    for (QSharedPointer<ZTreeNode> node : struc->children)
    {
        if (node->type() == ZTreeNode::Struct)
            allok &= parseStructMethods(node.dynamicCast<ZStruct>());
        else if (node->type() == ZTreeNode::Class) // wtf?
            allok &= parseStructMethods(node.dynamicCast<ZClass>());
    }

    return allok;
}

QSharedPointer<ZCodeBlock> Parser::parseCodeBlock(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context)
{
    QSharedPointer<ZCodeBlock> block = QSharedPointer<ZCodeBlock>(new ZCodeBlock(parent));
    while (true)
    {
        quint64 flags = Stmt_Function;

        // check if there are cycles along the parent chain
        QSharedPointer<ZTreeNode> p = parent;
        while (p && p->type() != ZTreeNode::Class && p->type() != ZTreeNode::Struct)
        {
            if (p->type() == ZTreeNode::ForCycle || p->type() == ZTreeNode::WhileCycle)
            {
                flags |= Stmt_CycleControl;
                break;
            }

            p = p->parent;
        }

        QList<QSharedPointer<ZTreeNode>> rootStatements = parseStatement(stream, block, context, flags, Tokenizer::Semicolon);

        if (!rootStatements.size())
            break; // done
        for (QSharedPointer<ZTreeNode> stmt : rootStatements)
        {
            stmt->parent = block;
            block->children.append(stmt);
        }
    }

    return block;
}

ZForCycle::~ZForCycle()
{
    // not needed anymore?
}

QSharedPointer<ZForCycle> Parser::parseForCycle(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context)
{
    // "for" is already parsed here. skip it
    QSharedPointer<ZForCycle> cycle = QSharedPointer<ZForCycle>(new ZForCycle(nullptr));
    cycle->parent = parent;
    cycle->condition = nullptr;
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::OpenParen))
    {
        qDebug("parseForCycle: unexpected %s, expected open parenthesis at line %d", token.toCString(), token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
    // either expression or list of initializers. same rules as local variables. todo: move out to some function
    QList<QSharedPointer<ZTreeNode>> initializers = parseStatement(stream, parent, context, Stmt_CycleInitializer, Tokenizer::Semicolon);
    cycle->initializers = initializers;

    QList<QSharedPointer<ZTreeNode>> condition = parseStatement(stream, cycle, context, Stmt_Expression, Tokenizer::Semicolon);
    cycle->condition = condition.size() ? condition[0].dynamicCast<ZExpression>() : nullptr;
    if (cycle->condition->type() != ZTreeNode::Expression)
    {
        qDebug("parseForCycle: expected valid expression for loop condition at line %d", token.line);
        return nullptr;
    }

    // condition done. parse steps
    skipWhitespace(stream, true);
    while (true)
    {
        skipWhitespace(stream, true);
        QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::CloseParen);
        if (!expr)
        {
            if (!stream.expectToken(token, Tokenizer::CloseParen))
            {
                qDebug("parseForCycle: unexpected %s, expected closing parenthesis at line %d", token.toCString(), token.line);
                return nullptr;
            }
        }
        highlightExpression(expr, cycle, context);
        cycle->step.append(expr);
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::CloseParen))
        {
            qDebug("parseForCycle: unexpected %s, expected comma or closing parenthesis at line %d", token.toCString(), token.line);
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        if (token.type == Tokenizer::CloseParen)
            break;
    }

    // now either block of code or single statement
    skipWhitespace(stream, true);
    QSharedPointer<ZCodeBlock> forBlock = parseCodeBlockOrLine(stream, parent, context, cycle);
    if (!forBlock)
    {
        qDebug("parseForCycle: expected valid cycle code");
        return nullptr;
    }
    forBlock->parent = cycle;
    cycle->children.append(forBlock);

    return cycle;
}

QList<QSharedPointer<ZTreeNode>> Parser::parseStatement(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context, quint64 flags, quint64 stopAtAnyOf)
{
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    QList<QSharedPointer<ZTreeNode>> nodes;
    static QList<QSharedPointer<ZTreeNode>> empty;
    if (!stream.readToken(token))
        return empty;

    bool allowInitializer = flags & Stmt_Initializer;
    bool allowCycle = flags & Stmt_Cycle;
    bool allowCycleControl = flags & Stmt_CycleControl;
    bool allowReturn = flags & Stmt_Return;
    bool allowExpression = flags & Stmt_Expression;
    bool allowCondition = flags & Stmt_Condition;

    if (token.type & stopAtAnyOf)
        return empty;

    //
    if (token.type == Tokenizer::Identifier)
    {
        // check keywords. for now allow only "const", "let", types, and "return"
        if (token.value == "let" && allowInitializer)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            while (true)
            {
                // make a new local variable
                // todo: check if already present
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Identifier))
                {
                    qDebug("parseStatement: unexpected %s, expected variable name at line %d", token.toCString(), token.line);
                    return empty;
                }
                Tokenizer::Token identifierToken = token;
                // check assignment
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpAssign))
                {
                    qDebug("parseStatement: unexpected %s, expected assignment at line %d", token.toCString(), token.line);
                    return empty;
                }
                parsedTokens.append(ParserToken(token, ParserToken::Operator));
                skipWhitespace(stream, true);
                QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::Comma|stopAtAnyOf);
                if (!expr)
                {
                    qDebug("parseStatement: expected valid assignment expression at line %d", token.line);
                    return empty;
                }
                highlightExpression(expr, parent, context);
                QSharedPointer<ZLocalVariable> var = QSharedPointer<ZLocalVariable>(new ZLocalVariable(nullptr));
                var->hasType = false;
                var->lineNumber = identifierToken.line;
                expr->parent = var;
                var->children.append(expr);
                var->identifier = identifierToken.value;
                nodes.append(var);
                parsedTokens.append(ParserToken(identifierToken, ParserToken::Local, var, identifierToken.value));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Comma|stopAtAnyOf))
                {
                    qDebug("parseStatement: unexpected %s, expected next variable or finalizing token at line %d", token.toCString(), token.line);
                    return empty;
                }

                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                if (token.type == Tokenizer::Semicolon)
                    break;
            }
            // done
            return nodes;
        }
        else if ((token.value == "break" || token.value == "continue") && allowCycleControl)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Semicolon))
            {
                qDebug("parseStatement: unexpected %s, expected semicolon at line %d", token.toCString(), token.line);
                return empty;
            }
            QSharedPointer<ZExecutionControl> ctl = QSharedPointer<ZExecutionControl>(new ZExecutionControl(nullptr));
            ctl->ctlType = (token.value == "break") ? ZExecutionControl::CtlBreak : ZExecutionControl::CtlContinue;
            nodes.append(ctl);
            return nodes;
        }
        else if (token.value == "return" && allowReturn)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            // for now, return single value
            skipWhitespace(stream, true);
            QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::Semicolon);
            if (!expr)
            {
                // check if return without value
                skipWhitespace(stream, true);
                if (!stream.peekToken(token) || token.type != Tokenizer::Semicolon)
                {
                    qDebug("parseStatement: expected valid return expression at line %d", token.line);
                    return empty;
                }

            }
            else
            {
                highlightExpression(expr, parent, context);
            }
            QSharedPointer<ZExecutionControl> ctl = QSharedPointer<ZExecutionControl>(new ZExecutionControl(nullptr));
            if (expr)
            {
                expr->parent = ctl;
                ctl->children.append(expr);
            }
            ctl->ctlType = ZExecutionControl::CtlReturn;
            nodes.append(ctl);

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Semicolon))
            {
                qDebug("parseStatement: unexpected %s, expected semicolon at line %d", token.toCString(), token.line);
                return empty;
            }
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            return nodes;
        }
        else if (token.value == "if" && allowCondition)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            QSharedPointer<ZCondition> cond = parseCondition(stream, parent, context);
            if (!cond)
            {
                qDebug("parseCondition: expected valid condition at line %d", token.line);
                return empty;
            }
            nodes.append(cond);
        }
        else if (token.value == "for" && allowCycle)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            QSharedPointer<ZForCycle> cycle = parseForCycle(stream, parent, context);
            if (!cycle)
            {
                qDebug("parseStatement: expected valid for cycle at line %d", token.line);
                return empty;
            }
            nodes.append(cycle);
            return nodes;
        }
        else if (token.value == "while" && allowCycle)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        }
        else if (token.value == "do" && allowCycle)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        }
        else
        {
            // is it an expression?
            int cpos = stream.position()-1;
            stream.setPosition(cpos);
            QSharedPointer<ZExpression> expr = allowExpression ? parseExpression(stream, stopAtAnyOf) : nullptr;
            if (expr)
            {
                nodes.append(expr);
                highlightExpression(expr, parent, context);

                if (!stream.expectToken(token, stopAtAnyOf))
                {
                    qDebug("parseStatement: unexpected %s, expected finalizing token at line %d", token.toCString(), token.line);
                    return empty;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            }
            else if (allowInitializer)
            {
                // [const] <type> <name> [= <value>] [, <name> [= <value>] ...]
                bool isConst = false;
                if (token.value == "const")
                {
                    isConst = true;
                    stream.setPosition(stream.position()+1);
                }
                QList<ZCompoundType> types;
                ZCompoundType type;
                if (!parseCompoundType(stream, type, context))
                {
                    qDebug("parseStatement: expected valid local type at line %d", token.line);
                    return empty;
                }
                while (true)
                {
                    ZCompoundType ftype = type;
                    // make a new local variable
                    // todo: check if already present
                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::Identifier))
                    {
                        qDebug("parseStatement: unexpected %s, expected variable name at line %d", token.toCString(), token.line);
                        return empty;
                    }
                    Tokenizer::Token identifierToken = token;
                    // check assignment
                    QSharedPointer<ZExpression> expr = nullptr;
                    skipWhitespace(stream, true);
                    if (stream.peekToken(token) && token.type == Tokenizer::OpAssign)
                    {
                        parsedTokens.append(ParserToken(token, ParserToken::Operator));
                        stream.setPosition(stream.position()+1);
                        skipWhitespace(stream, true);
                        expr = parseExpression(stream, Tokenizer::Comma|Tokenizer::Semicolon);
                        if (!expr)
                        {
                            qDebug("parseStatement: expected valid assignment expression at line %d", token.line);
                            return empty;
                        }
                        highlightExpression(expr, parent, context);
                    }
                    else if (stream.peekToken(token) && token.type == Tokenizer::OpenSquare)
                    {
                        while (true)
                        {
                            stream.setPosition(stream.position()+1);
                            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                            // read array dimensions. they are mutually incompatible with assignment... at least for now
                            // read in subscript
                            QList<Tokenizer::Token> subTokens;
                            if (!consumeTokens(stream, subTokens, Tokenizer::CloseSquare))
                            {
                                qDebug("parseStatement: unexpected end of stream while reading array expression at line %d", token.line);
                                return empty;
                            }
                            //
                            // make sure we did find a close square
                            stream.peekToken(token);
                            if (!stream.expectToken(token, Tokenizer::CloseSquare))
                            {
                                qDebug("parseStatement: unexpected %s, expected closing square while reading array expression at line %d", token.toCString(), token.line);
                                return empty;
                            }
                            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                            // parse expression under this subscript
                            TokenStream exprStream(subTokens);
                            QSharedPointer<ZExpression> expr = parseExpression(exprStream, 0);
                            if (!expr)
                            {
                                qDebug("parseStatement: expected valid expression while reading array expression at line %d", token.line);
                                return empty;
                            }
                            highlightExpression(expr, parent, context);
                            ftype.arrayDimensions.append(expr);
                            // check for next subscript
                            skipWhitespace(stream, true);
                            if (stream.peekToken(token) && token.type == Tokenizer::OpenSquare)
                                continue;
                            break;
                        }
                    }
                    QSharedPointer<ZLocalVariable> var = QSharedPointer<ZLocalVariable>(new ZLocalVariable(nullptr));
                    if (isConst)
                        var->flags.append("const");
                    var->hasType = true;
                    var->varType = ftype;
                    var->identifier = identifierToken.value;
                    types.append(ftype);
                    var->lineNumber = identifierToken.line;
                    if (expr)
                    {
                        expr->parent = var;
                        var->children.append(expr);
                    }
                    nodes.append(var);
                    parsedTokens.append(ParserToken(identifierToken, ParserToken::Local, var, identifierToken.value));

                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::Semicolon))
                    {
                        qDebug("parseStatement: unexpected %s, expected next variable or semicolon at line %d", token.toCString(), token.line);
                        return empty;
                    }

                    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                    if (token.type == Tokenizer::Semicolon)
                        break;
                }
            }
        }
    }

    return nodes;
}

ZCondition::~ZCondition()
{
    // not needed anymore?
}

QSharedPointer<ZCondition> Parser::parseCondition(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context)
{
    // "if" is already parsed here. skip it
    QSharedPointer<ZCondition> cond = QSharedPointer<ZCondition>(new ZCondition(nullptr));
    cond->condition = nullptr;
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::OpenParen))
    {
        qDebug("parseCondition: unexpected %s, expected open parenthesis at line %d", token.toCString(), token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

    // parse expression
    QSharedPointer<ZExpression> expr = parseExpression(stream, Tokenizer::CloseParen);
    if (!expr)
    {
        qDebug("parseCondition: expected valid condition expression at line %d", token.line);
        return nullptr;
    }
    highlightExpression(expr, parent, context);
    cond->condition = expr;

    skipWhitespace(stream, true);
    if (!stream.expectToken(token, Tokenizer::CloseParen))
    {
        qDebug("parseCondition: unexpected %s, expected close parenthesis at line %d", token.toCString(),token.line);
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

    QSharedPointer<ZCodeBlock> condBlock = parseCodeBlockOrLine(stream, parent, context, cond);
    if (!condBlock)
    {
        qDebug("parseCondition: expected valid conditional code");
        return nullptr;
    }
    condBlock->parent = cond;
    cond->children.append(condBlock);

    int cpos = stream.position();
    skipWhitespace(stream, true);
    if (stream.expectToken(token, Tokenizer::Identifier) && token.value.toLower() == "else")
    {
        parsedTokens.append(ParserToken(token, ParserToken::Keyword));
        QSharedPointer<ZCodeBlock> elseBlock = parseCodeBlockOrLine(stream, parent, context, cond);
        if (!elseBlock)
        {
            qDebug("parseCondition: expected valid else conditional code");
            return nullptr;
        }
        elseBlock->parent = cond;
        cond->elseBlock = elseBlock;
    }
    else stream.setPosition(cpos);

    return cond;

}



QSharedPointer<ZCodeBlock> Parser::parseCodeBlockOrLine(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context, QSharedPointer<ZTreeNode> recip)
{
    // now either block of code or single statement
    Tokenizer::Token token;
    skipWhitespace(stream, true);
    if (stream.peekToken(token) && token.type == Tokenizer::OpenCurly) // this is a code block
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        stream.setPosition(stream.position()+1);
        QList<Tokenizer::Token> tokens;
        if (!consumeTokens(stream, tokens, Tokenizer::CloseCurly))
        {
            qDebug("parseCodeBlockOrLine: unexpected end of stream, expected cycle code block at line %d", token.line);
            return nullptr;
        }

        if (!stream.readToken(token) || token.type != Tokenizer::CloseCurly)
        {
            qDebug("parseCodeBlockOrLine: unexpected end of stream, expected closing curly brace at line %d", token.line);
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

        TokenStream childTs(tokens);
        QSharedPointer<ZCodeBlock> block = parseCodeBlock(childTs, parent, context);
        if (!block)
        {
            qDebug("parseCodeBlockOrLine: expected valid code block at line %d", token.line);
            return nullptr;
        }
        return block;
    }
    else
    {
        QList<QSharedPointer<ZTreeNode>> statements = parseStatement(stream, parent, context, Stmt_Function|Stmt_CycleControl, Tokenizer::Semicolon);
        if (!statements.size())
        {
            qDebug("parseCodeBlockOrLine: expected one-line statement at line %d", token.line);
            return nullptr;
        }

        QSharedPointer<ZCodeBlock> block = QSharedPointer<ZCodeBlock>(new ZCodeBlock(nullptr));
        for (QSharedPointer<ZTreeNode> statement : statements)
        {
            statement->parent = block;
            block->children.append(statement);
        }
        return block;
    }
}

bool Parser::parseEnumExpressions(QSharedPointer<ZEnum> enm, QSharedPointer<ZStruct> context)
{
    for (QSharedPointer<ZTreeNode> node : enm->children)
    {
        if (node->type() != ZTreeNode::Constant)
            continue;
        QSharedPointer<ZConstant> konst = node.dynamicCast<ZConstant>();
        if (!konst->children.size())
            continue;
        QSharedPointer<ZExpression> expr = konst->children[0].dynamicCast<ZExpression>();
        highlightExpression(expr, enm, context);
    }
    return true;
}
