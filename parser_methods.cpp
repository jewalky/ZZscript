#include "parser.h"
#include <cmath>

bool Parser::parseObjectMethods(ZClass* cls, ZStruct* struc)
{
    // go through methods
    for (ZTreeNode* node : struc->children)
    {
        if (node->type() != ZTreeNode::Method)
            continue;

        // parse method
        ZMethod* method = reinterpret_cast<ZMethod*>(node);
        TokenStream stream(method->tokens);
        ZCodeBlock* rootBlock = parseCodeBlock(stream, nullptr, nullptr, struc);
        if (!rootBlock)
        {
            qDebug("parseObjectMethods: failed to parse '%s'", method->identifier.toUtf8().data());
            continue;
        }
        rootBlock->parent = method;
        method->children.append(rootBlock);
    }
    return true;
}

ZCodeBlock* Parser::parseCodeBlock(TokenStream& stream, ZCodeBlock* parent, ZTreeNode* aux, ZStruct* context)
{
    ZCodeBlock* block = new ZCodeBlock(nullptr);
    while (true)
    {
        quint64 flags = Stmt_Initializer|Stmt_Expression|Stmt_Cycle|Stmt_Return;
        if (aux) flags |= Stmt_CycleControl;
        QList<ZTreeNode*> rootStatements = parseStatement(stream, block, nullptr, context, flags, Tokenizer::Semicolon);

        if (!rootStatements.size())
            break; // done
        for (ZTreeNode* stmt : rootStatements)
        {
            stmt->parent = block;
            block->children.append(stmt);
        }
    }

    return block;
}

ZForCycle::~ZForCycle()
{
    for (ZTreeNode* var : initializers)
        delete var;
    if (condition) delete condition;
    for (ZExpression* expr : step)
        delete expr;
    initializers.clear();
    step.clear();
    condition = nullptr;
}

ZForCycle* Parser::parseForCycle(TokenStream& stream, ZCodeBlock* parent, ZTreeNode* aux, ZStruct* context)
{
    // "for" is already parsed here. skip it
    ZForCycle* cycle = new ZForCycle(nullptr);
    cycle->condition = nullptr;
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    if (!stream.expectToken(token, Tokenizer::OpenParen))
    {
        qDebug("parseForCycle: unexpected %s, expected open parenthesis at line %d", token.line);
        delete cycle;
        return nullptr;
    }
    parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
    // either expression or list of initializers. same rules as local variables. todo: move out to some function
    QList<ZTreeNode*> initializers = parseStatement(stream, parent, aux, context, Stmt_Initializer|Stmt_Expression, Tokenizer::Semicolon);
    cycle->initializers = initializers;

    QList<ZTreeNode*> condition = parseStatement(stream, parent, aux, context, Stmt_Expression, Tokenizer::Semicolon);
    cycle->condition = condition.size() ? reinterpret_cast<ZExpression*>(condition[0]) : nullptr;
    if (cycle->condition->type() != ZTreeNode::Expression)
    {
        qDebug("parseForCycle: expected valid expression for loop condition at line %d", token.line);
        delete cycle;
        return nullptr;
    }

    // condition done. parse steps
    skipWhitespace(stream, true);
    while (true)
    {
        skipWhitespace(stream, true);
        ZExpression* expr = parseExpression(stream, Tokenizer::CloseParen);
        if (!expr)
        {
            if (!stream.expectToken(token, Tokenizer::CloseParen))
            {
                qDebug("parseForCycle: unexpected %s, expected closing parenthesis at line %d", token.toCString(), token.line);
                delete cycle;
                return nullptr;
            }
        }
        cycle->step.append(expr);
        skipWhitespace(stream, true);
        if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::CloseParen))
        {
            qDebug("parseForCycle: unexpected %s, expected comma or closing parenthesis at line %d", token.toCString(), token.line);
            delete cycle;
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        if (token.type == Tokenizer::CloseParen)
            break;
    }

    // now either block of code or single statement
    skipWhitespace(stream, true);
    if (stream.peekToken(token) && token.type == Tokenizer::OpenCurly) // this is a code block
    {
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
        stream.setPosition(stream.position()+1);
        QList<Tokenizer::Token> tokens;
        if (!consumeTokens(stream, tokens, Tokenizer::CloseCurly))
        {
            qDebug("parseForCycle: unexpected end of stream, expected cycle code block at line %d", token.line);
            delete cycle;
            return nullptr;
        }

        if (!stream.readToken(token) || token.type != Tokenizer::CloseCurly)
        {
            qDebug("parseForCycle: unexpected end of stream, expected closing curly brace at line %d", token.line);
            delete cycle;
            return nullptr;
        }
        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));

        TokenStream childTs(tokens);
        ZCodeBlock* block = parseCodeBlock(childTs, parent, cycle, context);
        if (!block)
        {
            qDebug("parseForCycle: expected valid code block at line %d", token.line);
            delete cycle;
            return nullptr;
        }
        cycle->children.append(block);
    }
    else
    {
        QList<ZTreeNode*> statements = this->parseStatement(stream, parent, cycle, context, Stmt_Initializer|Stmt_Expression|Stmt_Cycle|Stmt_CycleControl|Stmt_Return, Tokenizer::Semicolon);
        if (!statements.size())
        {
            qDebug("parseForCycle: expected one-line statement at line %d", token.line);
            delete cycle;
            return nullptr;
        }

        ZCodeBlock* block = new ZCodeBlock(cycle);
        cycle->children.append(block);
        for (ZTreeNode* statement : statements)
        {
            statement->parent = block;
            block->children.append(statement);
        }
    }

    return cycle;
}

QList<ZTreeNode*> Parser::parseStatement(TokenStream& stream, ZCodeBlock* parent, ZTreeNode* aux, ZStruct* context, quint64 flags, quint64 stopAtAnyOf)
{
    ZTreeNode* node;
    skipWhitespace(stream, true);
    Tokenizer::Token token;
    QList<ZTreeNode*> nodes;
    static QList<ZTreeNode*> empty;
    if (!stream.readToken(token))
        return empty;

    bool allowInitializer = flags & Stmt_Initializer;
    bool allowCycle = flags & Stmt_Cycle;
    bool allowCycleControl = flags & Stmt_CycleControl;
    bool allowReturn = flags & Stmt_Return;
    bool allowExpression = flags & Stmt_Expression;

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
                    for (ZTreeNode* node : nodes) delete node;
                    return empty;
                }
                Tokenizer::Token identifierToken = token;
                // check assignment
                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::OpAssign))
                {
                    qDebug("parseStatement: unexpected %s, expected assignment at line %d", token.toCString(), token.line);
                    for (ZTreeNode* node : nodes) delete node;
                    return empty;
                }
                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                skipWhitespace(stream, true);
                ZExpression* expr = parseExpression(stream, Tokenizer::Comma|stopAtAnyOf);
                if (!expr)
                {
                    qDebug("parseStatement: expected valid assignment expression at line %d", token.toCString(), token.line);
                    for (ZTreeNode* node : nodes) delete node;
                    return empty;
                }
                ZLocalVariable* var = new ZLocalVariable(nullptr);
                var->hasType = false;
                var->lineNumber = identifierToken.line;
                expr->parent = var;
                var->children.append(expr);
                nodes.append(var);
                parsedTokens.append(ParserToken(identifierToken, ParserToken::Local, var));

                skipWhitespace(stream, true);
                if (!stream.expectToken(token, Tokenizer::Comma|stopAtAnyOf))
                {
                    qDebug("parseStatement: unexpected %s, expected next variable or finalizing token at line %d", token.toCString(), token.line);
                    for (ZTreeNode* node : nodes) delete node;
                    return empty;
                }

                parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                if (token.type == Tokenizer::Semicolon)
                    break;
            }
            // done
            return nodes;
        }
        else if (token.value == "return" && allowReturn)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            // for now, return single value
            skipWhitespace(stream, true);
            ZExpression* expr = parseExpression(stream, Tokenizer::Semicolon);
            if (!expr)
            {
                qDebug("parseStatement: expected valid return expression at line %d", token.line);
                for (ZTreeNode* node : nodes) delete node;
                return empty;
            }
            ZExecutionControl* ctl = new ZExecutionControl(nullptr);
            expr->parent = ctl;
            ctl->children.append(expr);
            ctl->ctlType = ZExecutionControl::CtlReturn;
            nodes.append(ctl);

            skipWhitespace(stream, true);
            if (!stream.expectToken(token, Tokenizer::Semicolon))
            {
                qDebug("parseStatement: unexpected %s, expected semicolon at line %d", token.toCString(), token.line);
                for (ZTreeNode* node : nodes) delete node;
                return empty;
            }
            parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
            return nodes;
        }
        else if (token.value == "for" && allowCycle)
        {
            parsedTokens.append(ParserToken(token, ParserToken::Keyword));
            ZForCycle* cycle = parseForCycle(stream, parent, aux, context);
            if (!cycle)
            {
                qDebug("parseStatement: expected valid for cycle at line %d", token.line);
                for (ZTreeNode* node : nodes) delete node;
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
            ZExpression* expr = allowExpression ? parseExpression(stream, stopAtAnyOf) : nullptr;
            if (expr)
            {
                nodes.append(expr);

                if (!stream.expectToken(token, stopAtAnyOf))
                {
                    qDebug("parseStatement: unexpected %s, expected finalizing token at line %d", token.toCString(), token.line);
                    for (ZTreeNode* node : nodes) delete node;
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
                ZCompoundType type;
                if (!parseCompoundType(stream, type, context))
                {
                    qDebug("parseStatement: expected valid local type at line %d", token.line);
                    for (ZTreeNode* node : nodes) delete node;
                    return empty;
                }
                while (true)
                {
                    // make a new local variable
                    // todo: check if already present
                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::Identifier))
                    {
                        qDebug("parseStatement: unexpected %s, expected variable name at line %d", token.toCString(), token.line);
                        for (ZTreeNode* node : nodes) delete node;
                        return empty;
                    }
                    Tokenizer::Token identifierToken = token;
                    // check assignment
                    ZExpression* expr = nullptr;
                    skipWhitespace(stream, true);
                    if (stream.peekToken(token) && token.type == Tokenizer::OpAssign)
                    {
                        parsedTokens.append(ParserToken(token, ParserToken::SpecialToken));
                        stream.setPosition(stream.position()+1);
                        skipWhitespace(stream, true);
                        expr = parseExpression(stream, Tokenizer::Comma|Tokenizer::Semicolon);
                        if (!expr)
                        {
                            qDebug("parseStatement: expected valid assignment expression at line %d", token.toCString(), token.line);
                            for (ZTreeNode* node : nodes) delete node;
                            return empty;
                        }
                    }
                    ZLocalVariable* var = new ZLocalVariable(nullptr);
                    if (isConst)
                        var->flags.append("const");
                    var->hasType = false;
                    var->lineNumber = identifierToken.line;
                    if (expr)
                    {
                        expr->parent = var;
                        var->children.append(expr);
                    }
                    nodes.append(var);
                    parsedTokens.append(ParserToken(identifierToken, ParserToken::Local, var));

                    skipWhitespace(stream, true);
                    if (!stream.expectToken(token, Tokenizer::Comma|Tokenizer::Semicolon))
                    {
                        qDebug("parseStatement: unexpected %s, expected next variable or semicolon at line %d", token.toCString(), token.line);
                        for (ZTreeNode* node : nodes) delete node;
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
