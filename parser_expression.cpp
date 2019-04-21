#include "parser.h"
#include <cmath>

ZExpression::~ZExpression()
{
    for (int i = 0; i < leaves.size(); i++)
    {
        if (leaves[i].expr)
        {
            delete leaves[i].expr;
            leaves[i].expr = nullptr;
        }
    }

    leaves.clear();
    resultType.destroy();
}

static QString typeFromLeaf(ZExpressionLeaf& leaf)
{
    switch (leaf.type)
    {
    case ZExpressionLeaf::Expression:
        return "expr"; // should not happen usually
    case ZExpressionLeaf::Integer:
        return "int";
    case ZExpressionLeaf::Double:
        return "double";
    case ZExpressionLeaf::Boolean:
        return "bool";
    case ZExpressionLeaf::Identifier:
        return "expr";
    case ZExpressionLeaf::Invalid:
        return "invalid";
    case ZExpressionLeaf::String:
        return "string";
    case ZExpressionLeaf::Token:
        return "expr";
    default:
        return "unknown";
    }
}

static bool evaluateCast(ZExpressionLeaf& leaf, ZExpressionLeaf& out, QString typeFrom, QString typeTo, bool isexplicit)
{
    typeFrom = typeFrom.toLower();
    typeTo = typeTo.toLower();

    // get system types for both
    ZSystemType* systemFrom = Parser::resolveSystemType(typeFrom);
    ZSystemType* systemTo = Parser::resolveSystemType(typeTo);
    if (!systemFrom || !systemTo)
        return false;

    // this evaluates only system ints and doubles
    if (systemFrom->kind != ZSystemType::SType_Float && systemFrom->kind != ZSystemType::SType_Integer)
        return false;
    if (systemTo->kind != ZSystemType::SType_Float && systemTo->kind != ZSystemType::SType_Integer)
        return false;

    if (systemFrom->kind == ZSystemType::SType_Float) typeFrom = "double";
    if (systemTo->kind == ZSystemType::SType_Float) typeTo = "double";

    if (typeFrom == typeTo)
    {
        out = leaf;
        return true;
    }

    // bool and int convert naturally
    if (systemFrom->kind == systemTo->kind)
    {
        out.type = (typeTo == "bool") ? ZExpressionLeaf::Boolean : ZExpressionLeaf::Integer;
        out.token.valueInt = leaf.token.valueInt;
        if (typeTo == "bool")
            out.token.valueInt = !!out.token.valueInt;
        return true;
    }

    // any int type converts to double
    if (systemFrom->kind == ZSystemType::SType_Integer && systemTo->kind == ZSystemType::SType_Float)
    {
        out.type = ZExpressionLeaf::Double;
        out.token.valueDouble = (systemFrom->kind == ZSystemType::SType_Float) ? leaf.token.valueDouble : leaf.token.valueInt;
        return true;
    }

    // double converts only explicitly (?)
    if (systemFrom->kind == ZSystemType::SType_Float && systemTo->kind == ZSystemType::SType_Integer)
    {
        if (!isexplicit)
            return false;
        out.type = (typeTo == "bool") ? ZExpressionLeaf::Boolean : ZExpressionLeaf::Integer;
        out.token.valueInt = int(leaf.token.valueDouble);
        return true;
    }

    return false;
}

bool ZExpression::evaluateLeaf(ZExpressionLeaf& in, ZExpressionLeaf& out, QString& type)
{
    ZExpressionLeaf ileaf;
    QString ileaft;
    if (in.type == ZExpressionLeaf::Expression)
    {
        if (!in.expr->evaluate(ileaf, ileaft))
            return false;
        ileaft = ileaft.toLower();
        if (ileaft == "float")
            ileaft = "double";
        out = ileaf;
        type = ileaft;
        return true;
    }
    else if (in.type == ZExpressionLeaf::Integer)
    {
        type = "int";
        out = in;
        return true;
    }
    else if (in.type == ZExpressionLeaf::Double)
    {
        type = "double";
        out = in;
        return true;
    }
    else if (in.type == ZExpressionLeaf::Boolean)
    {
        int ires = int(in.token.valueInt);
        type = "bool";
        out.type = ZExpressionLeaf::Integer;
        out.token.type = Tokenizer::Integer;
        out.token.valueInt = ires;
        return true;
    }

    return false;
}

bool ZExpression::evaluate(ZExpressionLeaf& out, QString& type)
{
    switch (op)
    {
    default:
        return false;
    case Literal:
        out = leaves[0];
        type = typeFromLeaf(out);
        return true;
    case Cast:
    {
        ZExpressionLeaf evRes;
        QString evType;
        if (!evaluateLeaf(leaves[1], evRes, evType))
            return false;
        if (!evaluateCast(evRes, out, evType, leaves[0].token.value, true))
            return false;
        type = leaves[0].token.value;
        return true;
    }
    case Add:
    case Sub:
    case Mul:
    case Div:
    case BitAnd:
    case BitOr:
    case BitShl:
    case BitShr:
    case Xor:
    case CmpEq:
    case CmpNotEq:
    case CmpSomewhatEq:
    case CmpLT:
    case CmpGT:
    case CmpGTEQ:
    case CmpLTEQ:
    case CmpSpaceship:
    case LogicalAnd:
    case LogicalOr:
    case Modulo:
    {
        //
        ZExpressionLeaf evLeafLeft, evLeafRight;
        QString evTypeLeft, evTypeRight;
        if (!evaluateLeaf(leaves[0], evLeafLeft, evTypeLeft))
            return false;
        if (!evaluateLeaf(leaves[1], evLeafRight, evTypeRight))
            return false;

        bool isdoublemath = evTypeLeft == "double" || evTypeRight == "double";

        if (!isdoublemath)
        {
            // both should be ints
            ZExpressionLeaf cLeafLeft, cLeafRight;
            if (!evaluateCast(evLeafLeft, cLeafLeft, evTypeLeft, "int", false))
                return false;
            if (!evaluateCast(evLeafRight, cLeafRight, evTypeRight, "int", false))
                return false;
            int ileft = int(cLeafLeft.token.valueInt);
            int iright = int(cLeafRight.token.valueInt);

            int ires = 0;
            type = "int";
            switch (op)
            {
            case Mul:
                ires = ileft*iright;
                break;
            case Div:
                if (iright) ires = ileft/iright;
                else ires = 0;
                break;
            case Add:
                ires = ileft+iright;
                break;
            case Sub:
                ires = ileft-iright;
                break;
            case BitAnd:
                ires = ileft&iright;
                break;
            case BitOr:
                ires = ileft|iright;
                break;
            case BitShl:
                ires = ileft<<iright;
                break;
            case BitShr:
                ires = ileft>>iright;
                break;
            case Xor:
                ires = ileft^iright;
                break;
            case CmpEq:
                ires = ileft == iright;
                type = "bool";
                break;
            case CmpNotEq:
                ires = ileft != iright;
                type = "bool";
                break;
            case CmpSomewhatEq: // for ints
                ires = ileft == iright;
                type = "bool";
                break;
            case CmpLT:
                ires = ileft < iright;
                type = "bool";
                break;
            case CmpGT:
                ires = ileft > iright;
                type = "bool";
                break;
            case CmpLTEQ:
                ires = ileft <= iright;
                type = "bool";
                break;
            case CmpGTEQ:
                ires = ileft >= iright;
                type = "bool";
                break;
            case CmpSpaceship:
                ires = 0;
                if (ileft > iright)
                    ires = 1;
                if (ileft < iright)
                    ires = -1;
                break;
            case LogicalAnd:
                ires = ileft && iright;
                type = "bool";
                break;
            case LogicalOr:
                ires = ileft || iright;
                type = "bool";
                break;
            case Modulo:
                ires = ileft % iright;
                break;
            default:
                return false;
            }

            out.type = (type == "bool") ? ZExpressionLeaf::Boolean : ZExpressionLeaf::Integer;
            out.token.type = Tokenizer::Integer;
            out.token.valueInt = ires;
        }
        else
        {
            // both should be doubles
            ZExpressionLeaf cLeafLeft, cLeafRight;
            if (!evaluateCast(evLeafLeft, cLeafLeft, evTypeLeft, "double", false))
                return false;
            if (!evaluateCast(evLeafRight, cLeafRight, evTypeRight, "double", false))
                return false;
            double dleft = cLeafLeft.token.valueDouble;
            double dright = cLeafRight.token.valueDouble;

            int ires = 0;
            double dres = 0;
            type = "double";
            switch (op)
            {
            case Mul:
                dres = dleft*dright;
                break;
            case Div:
                if (dright != 0.0) dres = dleft/dright;
                else dres = 0;
                break;
            case Add:
                dres = dleft+dright;
                break;
            case Sub:
                dres = dleft-dright;
                break;
            case CmpEq:
                ires = dleft == dright;
                type = "bool";
                break;
            case CmpNotEq:
                ires = dleft != dright;
                type = "bool";
                break;
            case CmpSomewhatEq: // for doubles
                ires = fabs(dleft-dright) < 0.000001;
                type = "bool";
                break;
            case CmpLT:
                ires = dleft < dright;
                type = "bool";
                break;
            case CmpGT:
                ires = dleft > dright;
                type = "bool";
                break;
            case CmpLTEQ:
                ires = dleft <= dright;
                type = "bool";
                break;
            case CmpGTEQ:
                ires = dleft >= dright;
                type = "bool";
                break;
            case CmpSpaceship:
                ires = 0;
                if (dleft > dright)
                    ires = 1;
                if (dleft < dright)
                    ires = -1;
                type = "int";
                break;
            case LogicalAnd:
                ires = (dleft != 0.0) && (dright != 0.0);
                type = "bool";
                break;
            case LogicalOr:
                ires = (dleft != 0.0) || (dright != 0.0);
                type = "bool";
                break;
            case Modulo:
                dres = fmod(dleft, dright);
                type = "bool";
                break;
            default:
                return false;
            }

            if (type == "double")
            {
                out.type = ZExpressionLeaf::Double;
                out.token.type = Tokenizer::Double;
                out.token.valueDouble = dres;
            }
            else
            {
                out.type = (type == "bool") ? ZExpressionLeaf::Boolean : ZExpressionLeaf::Integer;
                out.token.type = Tokenizer::Integer;
                out.token.valueInt = ires;
            }
        }

        return true;
    }
    case UnaryMinus:
    case UnaryNeg:
    case UnaryNot:
    {
        ZExpressionLeaf evLeaf;
        QString evType;
        if (!evaluateLeaf(leaves[0], evLeaf, evType))
            return false;
        bool isdoublemath = (evType == "double");

        if (!isdoublemath)
        {
            int ires = int(evLeaf.token.valueInt);
            type = "int";
            switch (op)
            {
            case UnaryMinus:
                ires = -ires;
                break;
            case UnaryNeg:
                ires = ~ires;
                break;
            case UnaryNot:
                ires = !ires;
                break;
            default:
                return false;
            }

            out.type = (type == "bool") ? ZExpressionLeaf::Boolean : ZExpressionLeaf::Integer;
            out.token.type = Tokenizer::Integer;
            out.token.valueInt = ires;
        }
        else
        {
            int ires = 0;
            double dres = evLeaf.token.valueDouble;
            type = "double";
            switch (op)
            {
            case UnaryMinus:
                dres = -dres;
                break;
            case UnaryNeg: // not applicable on doubles
                return false;
            case UnaryNot:
                ires = (dres == 0.0);
                type = "bool";
                break;
            default:
                return false;
            }

            if (type == "bool")
            {
                out.type = ZExpressionLeaf::Boolean;
                out.token.type = Tokenizer::Integer;
                out.token.valueInt = ires;
            }
            else
            {
                out.type = ZExpressionLeaf::Double;
                out.token.type = Tokenizer::Double;
                out.token.valueDouble = dres;
            }
        }

        return true;
    }
    }
}

static bool mergeTernaryExpression(QList<ZExpressionLeaf>& leaves)
{
    // it finds all instances of ?, searches for corresponding : and combines it into a single expression
    for (int i = 0; i < leaves.size(); i++)
    {
        ZExpressionLeaf& leaf = leaves[i];
        if (leaf.type != ZExpressionLeaf::Token)
            continue;
        Tokenizer::Token& token = leaf.token;
        if (token.type == Tokenizer::Questionmark) // ternary start
        {
            bool leftisexpr = (i-1 >= 0 && leaves[i-1].type != ZExpressionLeaf::Token);
            bool rightisexpr = (i+1 < leaves.size() && leaves[i+1].type != ZExpressionLeaf::Token);
            bool thirdisexpr = (i+3 < leaves.size() && leaves[i+3].type != ZExpressionLeaf::Token);
            bool colonfound = (i+2 < leaves.size() && leaves[i+2].type == ZExpressionLeaf::Token && leaves[i+2].token.type == Tokenizer::Colon);
            if (!leftisexpr || !rightisexpr || !thirdisexpr || !colonfound)
                return false; // bad ternary, but operator found

            ZExpressionLeaf newleaf;
            newleaf.type = ZExpressionLeaf::Expression;
            newleaf.expr = new ZExpression(nullptr);
            newleaf.expr->op = ZExpression::Ternary;
            newleaf.expr->assign = false;
            newleaf.expr->leaves.append(leaves[i-1]);
            newleaf.expr->leaves.append(leaves[i+1]);
            newleaf.expr->leaves.append(leaves[i+3]);
            newleaf.expr->operatorTokens.append(leaves[i].token);
            newleaf.expr->operatorTokens.append(leaves[i+2].token);
            leaves[i-1] = newleaf;
            leaves.removeAt(i); // ?
            leaves.removeAt(i); // true expr
            leaves.removeAt(i); // :
            leaves.removeAt(i); // false expr
            i--;
            continue;
        }
    }

    return true;
}

static bool mergeBinaryExpressions(QList<ZExpressionLeaf>& leaves, ZExpression::Operator mergeop, bool right = false)
{
    // now merge expressions by operator priority and applicability (?)
    for (int i = right?(leaves.size()-1):0; right?(i >= 0):(i < leaves.size()); right?i--:i++)
    {
        ZExpressionLeaf& leaf = leaves[i];
        if (leaf.type != ZExpressionLeaf::Token)
            continue;
        Tokenizer::Token& token = leaf.token;
        ZExpression::Operator op = ZExpression::Invalid;
        switch (token.type)
        {
        case Tokenizer::Identifier:
            if (!token.value.compare("dot", Qt::CaseInsensitive))
                op = ZExpression::VectorDot;
            else if (!token.value.compare("cross", Qt::CaseInsensitive))
                op = ZExpression::VectorCross;
            else return false;
            break;
        case Tokenizer::OpAssign:
            op = ZExpression::Assign;
            break;
        case Tokenizer::OpAdd:
            op = ZExpression::Add;
            break;
        case Tokenizer::OpSubtract:
            op = ZExpression::Sub;
            break;
        case Tokenizer::OpMultiply:
            op = ZExpression::Mul;
            break;
        case Tokenizer::OpDivide:
            op = ZExpression::Div;
            break;
        case Tokenizer::OpAnd:
            op = ZExpression::BitAnd;
            break;
        case Tokenizer::OpOr:
            op = ZExpression::BitOr;
            break;
        case Tokenizer::OpLeftShift:
            op = ZExpression::BitShl;
            break;
        case Tokenizer::OpRightShift:
            op = ZExpression::BitShr;
            break;
        case Tokenizer::OpRightShiftUnsigned:
            op = ZExpression::BitShrUs;
            break;
        case Tokenizer::OpXor:
            op = ZExpression::Xor;
            break;
        case Tokenizer::OpEquals:
            op = ZExpression::CmpEq;
            break;
        case Tokenizer::OpNotEquals:
            op = ZExpression::CmpNotEq;
            break;
        case Tokenizer::OpSomewhatEquals:
            op = ZExpression::CmpSomewhatEq;
            break;
        case Tokenizer::OpLessThan:
            op = ZExpression::CmpLT;
            break;
        case Tokenizer::OpGreaterThan:
            op = ZExpression::CmpGT;
            break;
        case Tokenizer::OpLessOrEqual:
            op = ZExpression::CmpLTEQ;
            break;
        case Tokenizer::OpGreaterOrEqual:
            op = ZExpression::CmpGTEQ;
            break;
        case Tokenizer::OpSpaceship:
            op = ZExpression::CmpSpaceship;
            break;
        case Tokenizer::OpLogicalAnd:
            op = ZExpression::LogicalAnd;
            break;
        case Tokenizer::OpLogicalOr:
            op = ZExpression::LogicalOr;
            break;
        case Tokenizer::OpNegate:
            op = ZExpression::UnaryNeg;
            break;
        case Tokenizer::OpUnaryNot:
            op = ZExpression::UnaryNot;
            break;
        case Tokenizer::OpModulo:
            op = ZExpression::Modulo;
            break;
        case Tokenizer::OpIncrement:
            op = ZExpression::Increment;
            break;
        case Tokenizer::OpDecrement:
            op = ZExpression::Decrement;
            break;
        case Tokenizer::Questionmark:
        case Tokenizer::Colon:
            continue; // do not try to do anything with ternary here
        default:
            // unknown
            return false;
        }

        // check if current token is not assign, and if next token is assign.
        // if it is, then we have some weird assignment operator and we should not parse it here (entirely different priority)
        // this works only with binary.
        bool isAssign = false;
        Tokenizer::Token assignToken;
        if (op != ZExpression::Assign)
        {
            if (i+1 < leaves.size() && leaves[i+1].type == ZExpressionLeaf::Token && leaves[i+1].token.type == Tokenizer::OpAssign)
            {
                isAssign = true;
                // now, if we are currently parsing assignment, this is ok. but if not, we need to skip this sequence
                if (mergeop != ZExpression::Assign)
                    continue;
                // otherwise remove next operator. this is so that rest of the code works as intended
                assignToken = leaves[i+1].token;
                leaves.removeAt(i+1);
            }
        }

        bool leftisexpr = (i-1 >= 0 && leaves[i-1].type != ZExpressionLeaf::Token);
        bool rightisexpr = (i+1 < leaves.size() && leaves[i+1].type != ZExpressionLeaf::Token);

        if (op == ZExpression::Sub && !leftisexpr && rightisexpr)
            op = ZExpression::UnaryMinus;

        if (op != mergeop && !isAssign)
            continue;

        // check increment/decrement
        if (op == ZExpression::Increment)
        {
            if (rightisexpr && right)
            {
                ZExpressionLeaf newleaf;
                newleaf.type = ZExpressionLeaf::Expression;
                newleaf.expr = new ZExpression(nullptr);
                newleaf.expr->op = ZExpression::PreIncrement;
                newleaf.expr->leaves.append(leaves[i+1]);
                newleaf.expr->operatorTokens.append(token);
                leaves[i] = newleaf;
                leaves.removeAt(i+1);
                continue;
            }
            else if (leftisexpr && !right)
            {
                ZExpressionLeaf newleaf;
                newleaf.type = ZExpressionLeaf::Expression;
                newleaf.expr = new ZExpression(nullptr);
                newleaf.expr->op = ZExpression::PostIncrement;
                newleaf.expr->leaves.append(leaves[i-1]);
                newleaf.expr->operatorTokens.append(token);
                leaves[i-1] = newleaf;
                leaves.removeAt(i);
                i--;
                continue;
            }
        }
        else if (op == ZExpression::Decrement)
        {
            if (rightisexpr && right)
            {
                ZExpressionLeaf newleaf;
                newleaf.type = ZExpressionLeaf::Expression;
                newleaf.expr = new ZExpression(nullptr);
                newleaf.expr->op = ZExpression::PreDecrement;
                newleaf.expr->leaves.append(leaves[i+1]);
                newleaf.expr->operatorTokens.append(token);
                leaves[i] = newleaf;
                leaves.removeAt(i+1);
                continue;
            }
            else if (leftisexpr && !right)
            {
                ZExpressionLeaf newleaf;
                newleaf.type = ZExpressionLeaf::Expression;
                newleaf.expr = new ZExpression(nullptr);
                newleaf.expr->op = ZExpression::PostDecrement;
                newleaf.expr->leaves.append(leaves[i-1]);
                newleaf.expr->operatorTokens.append(token);
                leaves[i-1] = newleaf;
                leaves.removeAt(i);
                i--;
                continue;
            }
        }

        // check unary
        if (op == ZExpression::UnaryNeg || op == ZExpression::UnaryNot ||
                op == ZExpression::UnaryMinus)
        {
            if (!rightisexpr)
                continue;

            ZExpressionLeaf newleaf;
            newleaf.type = ZExpressionLeaf::Expression;
            newleaf.expr = new ZExpression(nullptr);
            newleaf.expr->op = op;
            newleaf.expr->leaves.append(leaves[i+1]);
            newleaf.expr->operatorTokens.append(token);
            leaves[i] = newleaf;
            leaves.removeAt(i+1);
            continue;
        }

        if (!leftisexpr || !rightisexpr)
            continue;

        ZExpressionLeaf newleaf;
        newleaf.type = ZExpressionLeaf::Expression;
        newleaf.expr = new ZExpression(nullptr);
        newleaf.expr->op = op;
        newleaf.expr->assign = isAssign || mergeop == ZExpression::Assign;
        newleaf.expr->leaves.append(leaves[i-1]);
        newleaf.expr->leaves.append(leaves[i+1]);
        newleaf.expr->operatorTokens.append(token);
        if (isAssign) newleaf.expr->operatorTokens.append(assignToken);
        leaves[i-1] = newleaf;
        leaves.removeAt(i);
        leaves.removeAt(i);
        if (right) i++;
        else i--;
    }

    return true;
}

// check if token is valid for operator combo (like +=, -=..)
static bool isValidForAssign(Tokenizer::TokenType type)
{
    switch (type)
    {
    case Tokenizer::OpAdd: // +=
    case Tokenizer::OpSubtract: // -=
    case Tokenizer::OpMultiply: // *=
    case Tokenizer::OpDivide: // /=
    case Tokenizer::OpOr: // |=
    case Tokenizer::OpAnd: // &=
    case Tokenizer::OpLeftShift: // <<=
    case Tokenizer::OpRightShift: // >>=
    case Tokenizer::OpRightShiftUnsigned: // >>>=
        return true;
    default:
        return false;
    }
}

ZExpression* Parser::parseExpression(TokenStream& stream, quint64 stopAtAnyOf)
{
    // for now, expect int + operator + int
    // then complicate things more. later
    bool didfail = false;
    Tokenizer::Token token;

    int cpos = stream.position();

    QList<ZExpressionLeaf> leaves;

    bool skipleft = false;
    while (true)
    {
        if (!skipleft)
        {
            // read first operand
            skipWhitespace(stream, true);
            if (!stream.readToken(token))
                break;

            // this is normal in case of post expressions
            if (token.type & stopAtAnyOf)
            {
                stream.setPosition(stream.position()-1);
                break;
            }

            ZExpression::Operator unaryOp = ZExpression::Invalid;
            if (token.type == Tokenizer::OpSubtract)
            {
                switch(token.type)
                {
                default:
                    break;
                case Tokenizer::OpSubtract:
                    unaryOp = ZExpression::UnaryMinus;
                    break;
                }

                skipWhitespace(stream, true);
                if (!stream.readToken(token))
                    break;
            }

            if (token.type == Tokenizer::OpenCurly)
            {
                QList<Tokenizer::Token> exprTokens;
                if (!consumeTokens(stream, exprTokens, Tokenizer::CloseCurly))
                    goto fail;

                skipWhitespace(stream, true);
                stream.readToken(token);
                if (token.type != Tokenizer::CloseCurly)
                    goto fail;

                // make expression from each comma
                ZExpressionLeaf leaf_array;
                leaf_array.type = ZExpressionLeaf::Expression;
                // split by commas
                int lastPos = 0;
                QList<ZExpression*> exprs;
                for (int i = 0; i <= exprTokens.size(); i++)
                {
                    if (i == exprTokens.size() || exprTokens[i].type == Tokenizer::Comma)
                    {
                        // since lastPos until i
                        QList<Tokenizer::Token> localExprTokens = exprTokens.mid(lastPos, i-lastPos);
                        TokenStream exprStream(localExprTokens);
                        ZExpression* subexpr = parseExpression(exprStream, 0);
                        if (!subexpr)
                        {
                            for (ZExpression* expr : exprs)
                                delete expr;
                            goto fail;
                        }
                        exprs.append(subexpr);
                        lastPos = i+1;
                    }
                }
                ZExpression* subexpr = new ZExpression(nullptr);
                for (ZExpression* expr : exprs)
                {
                    ZExpressionLeaf leaf;
                    leaf.type = ZExpressionLeaf::Expression;
                    leaf.expr = expr;
                    subexpr->leaves.append(leaf);
                }
                subexpr->op = ZExpression::ArrayInitialization;
                leaf_array.expr = subexpr;
                leaves.append(leaf_array);
            }
            else if (token.type == Tokenizer::OpenParen)
            {
                QList<Tokenizer::Token> specTokens;
                specTokens.append(token);
                QList<Tokenizer::Token> exprTokens;
                if (!consumeTokens(stream, exprTokens, Tokenizer::CloseParen))
                    goto fail;

                skipWhitespace(stream, true);
                stream.readToken(token);
                if (token.type != Tokenizer::CloseParen)
                    goto fail;
                specTokens.append(token);

                // check vector expression
                bool hasCommas = false;
                int level = 0;
                for (Tokenizer::Token& tok : exprTokens)
                {
                    if (tok.type == Tokenizer::OpenParen)
                        level++;
                    if (tok.type == Tokenizer::CloseParen)
                        level--;
                    if (tok.type == Tokenizer::Comma && level <= 0)
                    {
                        hasCommas = true;
                        break;
                    }
                }

                if (hasCommas)
                {
                    // make expression from each comma
                    ZExpressionLeaf leaf_vector;
                    leaf_vector.type = ZExpressionLeaf::Expression;
                    // split by commas
                    int lastPos = 0;
                    QList<ZExpression*> exprs;
                    for (int i = 0; i <= exprTokens.size(); i++)
                    {
                        if (i == exprTokens.size() || exprTokens[i].type == Tokenizer::Comma)
                        {
                            // since lastPos until i
                            if (i < exprTokens.size()) specTokens.append(exprTokens[i]);
                            QList<Tokenizer::Token> localExprTokens = exprTokens.mid(lastPos, i-lastPos);
                            TokenStream exprStream(localExprTokens);
                            ZExpression* subexpr = parseExpression(exprStream, 0);
                            if (!subexpr)
                            {
                                for (ZExpression* expr : exprs)
                                    delete expr;
                                goto fail;
                            }
                            exprs.append(subexpr);
                            lastPos = i+1;
                        }
                    }
                    ZExpression* subexpr = new ZExpression(nullptr);
                    for (ZExpression* expr : exprs)
                    {
                        ZExpressionLeaf leaf;
                        leaf.type = ZExpressionLeaf::Expression;
                        leaf.expr = expr;
                        subexpr->leaves.append(leaf);
                    }
                    subexpr->op = ZExpression::VectorInitialization;
                    leaf_vector.expr = subexpr;
                    leaf_vector.expr->specialTokens.append(specTokens);
                    leaves.append(leaf_vector);
                }
                else
                {
                    TokenStream exprStream(exprTokens);
                    ZExpression* subexpr = parseExpression(exprStream, 0);
                    if (!subexpr)
                        goto fail;

                    ZExpressionLeaf leaf_expr;
                    leaf_expr.type = ZExpressionLeaf::Expression;
                    leaf_expr.expr = subexpr;
                    leaf_expr.expr->specialTokens.append(specTokens);
                    leaves.append(leaf_expr);
                }
            }
            else if (token.type == Tokenizer::Integer)
            {
                ZExpressionLeaf leaf_left;
                leaf_left.type = ZExpressionLeaf::Integer;
                leaf_left.token = token;
                leaves.append(leaf_left);
            }
            else if (token.type == Tokenizer::Double)
            {
                ZExpressionLeaf leaf_left;
                leaf_left.type = ZExpressionLeaf::Double;
                leaf_left.token = token;
                leaves.append(leaf_left);
            }
            else if (token.type == Tokenizer::String || token.type == Tokenizer::Name)
            {
                ZExpressionLeaf leaf_left;
                leaf_left.type = ZExpressionLeaf::String;
                leaf_left.token = token;
                leaves.append(leaf_left);
            }
            else if (token.type == Tokenizer::Identifier)
            {
                // check for special identifiers
                QString idlower = (token.value.toLower());
                if (idlower == "true" || idlower == "false")
                {
                    ZExpressionLeaf leaf_left;
                    leaf_left.type = ZExpressionLeaf::Boolean;
                    leaf_left.token = token;
                    leaf_left.token.type = Tokenizer::Integer;
                    leaf_left.token.valueInt = (idlower == "true");
                    leaves.append(leaf_left);
                }
                else if (idlower == "bool" || idlower == "int" || idlower == "double" || idlower == "float")
                {
                    if (idlower == "float") idlower = "double";
                    Tokenizer::Token idtoken = token;
                    idtoken.value = idlower;
                    ZExpressionLeaf leaf_type;
                    leaf_type.type = ZExpressionLeaf::Identifier;
                    leaf_type.token = idtoken;
                    // we need to resolve primitive casts here. so that it's possible to evaluate const expressions
                    // non-POD types generally don't exist in const expressions
                    //
                    Tokenizer::Token next;
                    skipWhitespace(stream, true);
                    stream.readToken(next);
                    if (next.type != Tokenizer::OpenParen)
                        goto fail;

                    QList<Tokenizer::Token> exprTokens;
                    if (!consumeTokens(stream, exprTokens, Tokenizer::CloseParen))
                        goto fail;

                    skipWhitespace(stream, true);
                    stream.readToken(token);
                    if (token.type != Tokenizer::CloseParen)
                        goto fail;

                    TokenStream exprStream(exprTokens);
                    ZExpression* subexpr = parseExpression(exprStream, 0);
                    if (!subexpr)
                        goto fail;

                    ZExpressionLeaf leaf_expr;
                    leaf_expr.type = ZExpressionLeaf::Expression;
                    leaf_expr.expr = subexpr;
                    ZExpressionLeaf leaf_cast;
                    leaf_cast.type = ZExpressionLeaf::Expression;
                    leaf_cast.expr = new ZExpression(nullptr);
                    leaf_cast.expr->op = ZExpression::Cast;
                    leaf_cast.expr->leaves.append(leaf_type);
                    leaf_cast.expr->leaves.append(leaf_expr);
                    leaves.append(leaf_cast);
                }
                else
                {
                    QList<Tokenizer::Token> opTokens;
                    // either identifier or member access
                    Tokenizer::Token next;
                    int scpos = stream.position();
                    skipWhitespace(stream, true);
                    stream.readToken(next);
                    if (next.type == Tokenizer::Dot)
                    {
                        opTokens.append(next);
                        // member access
                        ZExpressionLeaf leaf_left;
                        leaf_left.type = ZExpressionLeaf::Expression;
                        ZExpression* expr = new ZExpression(nullptr);
                        expr->op = ZExpression::Member;
                        leaf_left.expr = expr;
                        ZExpressionLeaf leaf_root;
                        leaf_root.type = ZExpressionLeaf::Identifier;
                        leaf_root.token = token;
                        expr->leaves.append(leaf_root);
                        while (true)
                        {
                            skipWhitespace(stream, true);
                            if (!stream.readToken(token))
                                break;
                            if (token.type != Tokenizer::Identifier)
                                break;
                            ZExpressionLeaf member_leaf;
                            member_leaf.type = ZExpressionLeaf::Identifier;
                            member_leaf.token = token;
                            expr->leaves.append(member_leaf);
                            skipWhitespace(stream, true);
                            if (!stream.readToken(token))
                                break;
                            if (token.type != Tokenizer::Dot)
                            {
                                stream.setPosition(stream.position()-1);
                                break;
                            }
                            opTokens.append(token);
                        }
                        expr->operatorTokens.append(opTokens);
                        leaves.append(leaf_left);
                    }
                    else
                    {
                        stream.setPosition(scpos);
                        ZExpressionLeaf leaf_left;
                        leaf_left.type = ZExpressionLeaf::Identifier;
                        leaf_left.token = token;
                        leaves.append(leaf_left);
                    }
                }
            }
            else if (token.type == Tokenizer::OpSubtract ||
                     token.type == Tokenizer::OpUnaryNot ||
                     token.type == Tokenizer::OpNegate ||
                     token.type == Tokenizer::OpIncrement ||
                     token.type == Tokenizer::OpDecrement)
            {
                /* fall through */
                stream.setPosition(stream.position()-1);
            }
            else goto fail;

            if (unaryOp != ZExpression::Invalid)
            {
                ZExpressionLeaf un_leaf;
                un_leaf.type = ZExpressionLeaf::Expression;
                un_leaf.expr = new ZExpression(nullptr);
                un_leaf.expr->op = unaryOp;
                ZExpressionLeaf& last = leaves.last();
                un_leaf.expr->leaves.append(last);
                last = un_leaf;
            }
        }

        skipleft = false;

        // read operator if any
        skipWhitespace(stream, true);
        if (!stream.readToken(token))
            break;
        if (token.type & stopAtAnyOf)
        {
            stream.setPosition(stream.position()-1);
            break;
        }

        switch (token.type)
        {
        // vector operators
        case Tokenizer::Identifier:
        {
            if (!token.value.compare("dot", Qt::CaseInsensitive) ||
                    !token.value.compare("cross", Qt::CaseInsensitive))
            {
                ZExpressionLeaf leaf_op;
                leaf_op.type = ZExpressionLeaf::Token;
                leaf_op.token = token;
                leaves.append(leaf_op);
                break;
            }

            goto fail; // bad syntax
        }
        // binary expressions
        case Tokenizer::OpAdd:
        case Tokenizer::OpSubtract:
        case Tokenizer::OpMultiply:
        case Tokenizer::OpDivide:
        case Tokenizer::OpAnd:
        case Tokenizer::OpOr:
        case Tokenizer::OpLeftShift:
        case Tokenizer::OpRightShift:
        case Tokenizer::OpRightShiftUnsigned:
        case Tokenizer::OpXor:
        case Tokenizer::OpEquals:
        case Tokenizer::OpNotEquals:
        case Tokenizer::OpSomewhatEquals:
        case Tokenizer::OpLessThan:
        case Tokenizer::OpGreaterThan:
        case Tokenizer::OpLessOrEqual:
        case Tokenizer::OpGreaterOrEqual:
        case Tokenizer::OpSpaceship:
        case Tokenizer::OpLogicalAnd:
        case Tokenizer::OpLogicalOr:
        // unary expressions
        case Tokenizer::OpNegate:
        case Tokenizer::OpUnaryNot:
        case Tokenizer::OpModulo:
        case Tokenizer::OpAssign:
        case Tokenizer::OpIncrement:
        case Tokenizer::OpDecrement:
        case Tokenizer::Questionmark:
        case Tokenizer::Colon:
        {
            ZExpressionLeaf leaf_op;
            leaf_op.type = ZExpressionLeaf::Token;
            leaf_op.token = token;
            leaves.append(leaf_op);
            if (isValidForAssign(token.type) && stream.peekToken(token) && token.type == Tokenizer::OpAssign)
                skipleft = true;
            break;
        }
        case Tokenizer::OpenSquare: // array access
        {
            QList<Tokenizer::Token> specTokens;
            ZExpressionLeaf& last = leaves.last();
            bool validForArray = (last.type == ZExpressionLeaf::Identifier || last.type == ZExpressionLeaf::Expression);
            if (!validForArray) goto fail; // really bad coding
            //
            // read some repeating [] expressions until we find something that isn't a [

            QList<ZExpressionLeaf> arrsubscripts;
            while (true)
            {
                specTokens.append(token);
                // read in subscript
                QList<Tokenizer::Token> subTokens;
                if (!consumeTokens(stream, subTokens, Tokenizer::CloseSquare))
                {
                    for (int i = 0; i < arrsubscripts.size(); i++)
                        delete arrsubscripts[i].expr;
                    goto fail;
                }
                //
                // make sure we did find a close square
                if (!stream.expectToken(token, Tokenizer::CloseSquare))
                {
                    for (int i = 0; i < arrsubscripts.size(); i++)
                        delete arrsubscripts[i].expr;
                    goto fail;
                }
                specTokens.append(token);
                // parse expression under this subscript
                TokenStream exprStream(subTokens);
                ZExpression* expr = parseExpression(exprStream, 0);
                if (!expr)
                {
                    for (int i = 0; i < arrsubscripts.size(); i++)
                        delete arrsubscripts[i].expr;
                    goto fail;
                }
                ZExpressionLeaf subleaf;
                subleaf.type = ZExpressionLeaf::Expression;
                subleaf.expr = expr;
                arrsubscripts.append(subleaf);
                // check for next subscript
                skipWhitespace(stream, true);
                if (stream.peekToken(token) && token.type == Tokenizer::OpenSquare)
                    continue;
                break;
            }
            // read multiple subscripts, store
            ZExpression* arrexpr = new ZExpression(nullptr);
            arrexpr->op = ZExpression::ArraySubscript;
            arrexpr->leaves.append(last);
            arrexpr->specialTokens.append(specTokens);
            for (int i = 0; i < arrsubscripts.size(); i++)
                arrexpr->leaves.append(arrsubscripts[i]);
            ZExpressionLeaf arrlleaf;
            arrlleaf.type = ZExpressionLeaf::Expression;
            arrlleaf.expr = arrexpr;
            last = arrlleaf;
            skipleft = true;
            break;
        }
        case Tokenizer::OpenParen: // function call
        {
            QList<Tokenizer::Token> specTokens;
            specTokens.append(token);
            ZExpressionLeaf& last = leaves.last();
            bool validForCall = (last.type == ZExpressionLeaf::Identifier || last.type == ZExpressionLeaf::Expression);
            if (!validForCall) goto fail; // really bad coding
            //
            // read some expressions separated by comma until we find a closing parenthesis
            // for now just expect paren

            QList<ZExpressionLeaf> callargs;
            // read tokens until call expression ends
            while (true)
            {
                // read in expression
                QList<Tokenizer::Token> exprTokens;
                if (!consumeTokens(stream, exprTokens, Tokenizer::CloseParen|Tokenizer::Comma))
                {
                    for (int i = 0; i < callargs.size(); i++)
                        delete callargs[i].expr;
                    goto fail;
                }

                bool nonwhitespace = false;
                for (int i = 0; i < exprTokens.size(); i++)
                {
                    Tokenizer::TokenType tt = exprTokens[i].type;
                    if (tt != Tokenizer::Whitespace && tt != Tokenizer::Newline)
                    {
                        nonwhitespace = true;
                        break;
                    }
                }

                skipWhitespace(stream, true);
                stream.readToken(token);

                TokenStream exprStream(exprTokens);
                ZExpression* subexpr = parseExpression(exprStream, 0);
                if ((!subexpr && nonwhitespace) || (token.type != Tokenizer::CloseParen && token.type != Tokenizer::Comma))
                {
                    if (subexpr)
                        delete subexpr;
                    for (int i = 0; i < callargs.size(); i++)
                        delete callargs[i].expr;
                    goto fail;
                }

                specTokens.append(token);

                if (nonwhitespace)
                {
                    ZExpressionLeaf argleaf;
                    argleaf.type = ZExpressionLeaf::Expression;
                    argleaf.expr = subexpr;
                    callargs.append(argleaf);
                }

                if (token.type == Tokenizer::CloseParen)
                    break;
            }

            ZExpression* callexpr = new ZExpression(nullptr);
            callexpr->op = ZExpression::Call;
            callexpr->leaves.append(last);
            callexpr->specialTokens.append(specTokens);
            for (int i = 0; i < callargs.size(); i++)
                callexpr->leaves.append(callargs[i]);
            ZExpressionLeaf calleaf;
            calleaf.type = ZExpressionLeaf::Expression;
            calleaf.expr = callexpr;
            last = calleaf;
            skipleft = true;
            break;
        }
        case Tokenizer::Dot: // member access off expression
        {
            ZExpressionLeaf& last = leaves.last();
            bool validForMember = (last.type == ZExpressionLeaf::Identifier || last.type == ZExpressionLeaf::Expression);
            if (!validForMember) goto fail; // really bad coding
            //
            QList<ZExpressionLeaf> members;
            members.append(last);
            while (true)
            {
                skipWhitespace(stream, true);
                if (!stream.readToken(token))
                    break;
                if (token.type != Tokenizer::Identifier)
                    break;
                ZExpressionLeaf member_leaf;
                member_leaf.type = ZExpressionLeaf::Identifier;
                member_leaf.token = token;
                members.append(member_leaf);
                skipWhitespace(stream, true);
                if (!stream.readToken(token))
                    break;
                if (token.type != Tokenizer::Dot)
                {
                    stream.setPosition(stream.position()-1);
                    break;
                }
            }
            ZExpression* memberexpr = new ZExpression(nullptr);
            memberexpr->op = ZExpression::Member;
            memberexpr->leaves = members;
            ZExpressionLeaf memberleaf;
            memberleaf.type = ZExpressionLeaf::Expression;
            memberleaf.expr = memberexpr;
            last = memberleaf;
            skipleft = true;
            break;
        }
        default:
            goto fail;
        }
    }

    // vector expressions
    if (!mergeBinaryExpressions(leaves, ZExpression::VectorDot)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::VectorCross)) goto fail;
    // post increment
    if (!mergeBinaryExpressions(leaves, ZExpression::Increment, false)) goto fail;
    // post decrement
    if (!mergeBinaryExpressions(leaves, ZExpression::Decrement, false)) goto fail;
    // pre increment
    if (!mergeBinaryExpressions(leaves, ZExpression::Increment, true)) goto fail;
    // pre decrement
    if (!mergeBinaryExpressions(leaves, ZExpression::Decrement, true)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::UnaryNeg, true)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::UnaryNot, true)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::UnaryMinus, true)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Mul)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Div)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Modulo)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Add)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Sub)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::BitShl)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::BitShr)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::BitShrUs)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::BitAnd)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Xor)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::BitOr)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpEq)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpNotEq)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpSomewhatEq)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpLT)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpGT)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpLTEQ)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpGTEQ)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::CmpSpaceship)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::LogicalAnd)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::LogicalOr)) goto fail;
    // ternary expression is below all other expressions except assignment
    if (!mergeTernaryExpression(leaves)) goto fail;
    if (!mergeBinaryExpressions(leaves, ZExpression::Assign)) goto fail;

    // using "goto fail" instead of return-whatever, guarantees that all expressions are deleted correctly
    goto finish;
fail:
    didfail = true;
    goto finish;

finish:
    if (leaves.size() != 1 || didfail)
    {
        stream.setPosition(cpos);
        for (int i = 0; i < leaves.size(); i++)
        {
            if (leaves[i].type == ZExpressionLeaf::Expression)
                delete leaves[i].expr;
        }

        return nullptr;
    }
    else
    {
        if (leaves.size())
        {
            if (leaves[0].type == ZExpressionLeaf::Expression)
                return leaves[0].expr;
            else
            {
                ZExpressionLeaf& leaf = leaves[0];
                if (leaf.type == ZExpressionLeaf::Identifier)
                {
                    ZExpression* expr = new ZExpression(nullptr);
                    expr->op = ZExpression::Identifier;
                    expr->leaves.append(leaf);
                    return expr;
                }
                else
                {
                    switch (leaf.type)
                    {
                    case ZExpressionLeaf::Integer:
                    case ZExpressionLeaf::Boolean:
                    case ZExpressionLeaf::String:
                    case ZExpressionLeaf::Double:
                    {
                        ZExpression* expr = new ZExpression(nullptr);
                        expr->op = ZExpression::Literal;
                        expr->leaves.append(leaf);
                        return expr;
                    }
                    default:
                        break;
                    }
                }
            }
        }

        return nullptr;
    }
}

static QString operatorToString(ZExpression::Operator op)
{
    switch (op)
    {
    case ZExpression::Invalid:
        return "invalid";
    case ZExpression::Assign:
        return "assign";
    case ZExpression::Add:
        return "add";
    case ZExpression::Sub:
        return "sub";
    case ZExpression::Mul:
        return "mul";
    case ZExpression::Div:
        return "div";
    case ZExpression::Decrement:
        return "decrement";
    case ZExpression::Increment:
        return "increment";
    case ZExpression::UnaryMinus:
        return "unary_sub";
    case ZExpression::UnaryNeg:
        return "unary_neg";
    case ZExpression::UnaryNot:
        return "unary_not";
    case ZExpression::Member:
        return "member";
    case ZExpression::Call:
        return "call";
    case ZExpression::Cast:
        return "cast";
    case ZExpression::BitAnd:
        return "bit_and";
    case ZExpression::BitOr:
        return "bit_or";
    case ZExpression::BitShl:
        return "bit_shl";
    case ZExpression::BitShr:
        return "bit_shr";
    case ZExpression::CmpEq:
        return "cmp_eq";
    case ZExpression::CmpNotEq:
        return "cmp_neq";
    case ZExpression::CmpGT:
        return "cmp_gt";
    case ZExpression::CmpLT:
        return "cmp_lt";
    case ZExpression::CmpGTEQ:
        return "cmp_gteq";
    case ZExpression::CmpLTEQ:
        return "cmp_lteq";
    case ZExpression::CmpSomewhatEq:
        return "cmp_somewhateq";
    case ZExpression::CmpSpaceship:
        return "cmp_spaceship";
    case ZExpression::Literal:
        return "literal";
    case ZExpression::LogicalAnd:
        return "logical_and";
    case ZExpression::LogicalOr:
        return "logical_or";
    case ZExpression::ArrayInitialization:
        return "array";
    case ZExpression::VectorInitialization:
        return "vector";
    default:
        return "unknown "+QString::number(op);
    }
}

void Parser::dumpExpression(ZExpression* expr, int level)
{
    if (!expr)
        return;
    QString pre = "";
    for (int i = 0; i < level; i++)
        pre += "  ";
    qDebug("%sExpression [%s] [leaves = %d]", pre.toUtf8().data(), operatorToString(expr->op).toUtf8().data(), expr->leaves.size());
    for (int i = 0; i < expr->leaves.size(); i++)
    {
        ZExpressionLeaf& leaf = expr->leaves[i];
        switch (leaf.type)
        {
        case ZExpressionLeaf::Integer:
            qDebug("%s Leaf [int] = %d", pre.toUtf8().data(), leaf.token.valueInt);
            break;
        case ZExpressionLeaf::Double:
            qDebug("%s Leaf [double] = %.2f", pre.toUtf8().data(), leaf.token.valueDouble);
            break;
        case ZExpressionLeaf::Boolean:
            qDebug("%s Leaf [bool] = %d", pre.toUtf8().data(), leaf.token.valueInt);
            break;
        case ZExpressionLeaf::Invalid:
            qDebug("%s Leaf [invalid]", pre.toUtf8().data());
            break;
        case ZExpressionLeaf::Expression:
            qDebug("%s Leaf [expression] = ", pre.toUtf8().data());
            dumpExpression(leaf.expr, level+1);
            break;
        case ZExpressionLeaf::Token:
            qDebug("%s Leaf [token] = %s", pre.toUtf8().data(), leaf.token.value.toUtf8().data());
            break;
        case ZExpressionLeaf::Identifier:
            qDebug("%s Leaf [identifier] = %s", pre.toUtf8().data(), leaf.token.value.toUtf8().data());
            break;
        case ZExpressionLeaf::String:
            qDebug("%s Leaf [string] = %s", pre.toUtf8().data(), leaf.token.value.toUtf8().data());
            break;
        default:
            qDebug("%s Leaf [unknown %d]", pre.toUtf8().data(), leaf.type);
            break;
        }
    }
}

//
static QString getFullFieldName(ZTreeNode* node)
{
    QString parents = node->identifier;
    ZTreeNode* p = node->parent;
    while (p)
    {
        if (p->type() == ZTreeNode::Class || p->type() == ZTreeNode::Struct)
            parents = p->identifier + "." + parents;
        p = p->parent;
    }
    return parents;
}

void Parser::highlightExpression(ZExpression* expr, ZTreeNode* parent, ZStruct* context)
{
    for (Tokenizer::Token& tok : expr->operatorTokens)
        parsedTokens.append(ParserToken(tok, ParserToken::Operator));
    for (Tokenizer::Token& tok : expr->specialTokens)
        parsedTokens.append(ParserToken(tok, ParserToken::SpecialToken));

    // here we also set expression type :)

    // produces smart syntax highlighting based on various contexts:
    // parent = block that contains this expression
    // context = struct that contains this expression (for types)
    if (expr->op == ZExpression::Call)
    {
        // leaf 0 = what to call
        // leaf 1..N = arguments
        if (expr->leaves.size() && expr->leaves[0].type == ZExpressionLeaf::Identifier && expr->leaves[0].token.value.toLower() == "new")
        {
            for (int i = 1; i < expr->leaves.size(); i++)
            {
                // leaf in a Call is always an expression. if it's a string, make it a type name
                ZExpressionLeaf& leaf = expr->leaves[i];
                ZExpression* leafExpr = leaf.expr;
                if (leafExpr->op == ZExpression::Literal && leafExpr->leaves.size() && leafExpr->leaves[0].type == ZExpressionLeaf::String)
                {
                    Tokenizer::Token& tok = leafExpr->leaves[0].token;
                    ZTreeNode* resolved = resolveType(tok.value, context);
                    parsedTokens.append(ParserToken(tok, ParserToken::TypeName, resolved, tok.value));
                    // set type of this expression to resolved type
                    expr->resultType.type = tok.value;
                    expr->resultType.reference = resolved;
                }
                else
                {
                    highlightExpression(leafExpr, parent, context);
                }
            }
            // in this case, "new" is also a keyword
            parsedTokens.append(ParserToken(expr->leaves[0].token, ParserToken::Keyword));
            return;
        }
    }
    else if (expr->op == ZExpression::Member)
    {
        // resolve symbol
        // in a Member, all leaves are identifiers
        ZTreeNode* lastcls = nullptr;
        ZTreeNode* lastfound = nullptr;
        bool fullfound = true;
        bool isstatic = false;
        for (int i = 0; i < expr->leaves.size(); i++)
        {
            ZExpressionLeaf& leaf = expr->leaves[i];
            if (i == 0) // first identifier in member expression is a local (or field or whatever)
            {
                // if it's not an identifier, resolve expression...
                if (leaf.type == ZExpressionLeaf::Expression)
                {
                    highlightExpression(leaf.expr, parent, context);
                    lastcls = leaf.expr->resultType.reference;
                    lastfound = nullptr;
                    continue;
                }
                QString firstSymbol = leaf.token.value;
                ZTreeNode* resolved = resolveSymbol(firstSymbol, parent, context);
                if (resolved)
                {
                    ParserToken::TokenType t = ParserToken::Local;
                    switch (resolved->type())
                    {
                    case ZTreeNode::LocalVariable:
                        t = (resolved->parent && resolved->parent->type() == ZTreeNode::Method) ? ParserToken::Argument : ParserToken::Local;
                        if (!firstSymbol.compare("self", Qt::CaseInsensitive) || !firstSymbol.compare("invoker", Qt::CaseInsensitive) ||
                                !firstSymbol.compare("super", Qt::CaseInsensitive))
                            t = ParserToken::Keyword;
                        break;
                    case ZTreeNode::Field:
                        t = ParserToken::Field;
                        break;
                    case ZTreeNode::Method:
                        t = ParserToken::Method;
                        break;
                    default:
                        break;
                    }
                    parsedTokens.append(ParserToken(leaf.token, t, resolved, leaf.token.value));
                    ZCompoundType ft;
                    if (resolved->type() == ZTreeNode::Method)
                    {
                        ZMethod* method = reinterpret_cast<ZMethod*>(resolved);
                        ft = method->returnTypes[0];
                    }
                    else if (resolved->type() == ZTreeNode::Field)
                    {
                        ZField* field = reinterpret_cast<ZField*>(resolved);
                        ft = field->fieldType;
                    }
                    else if (resolved->type() == ZTreeNode::LocalVariable)
                    {
                        ZLocalVariable* local = reinterpret_cast<ZLocalVariable*>(resolved);
                        if (!local->hasType && local->children.size() && local->children[0]->type() == ZTreeNode::Expression)
                        {
                            ZExpression* localExpr = reinterpret_cast<ZExpression*>(local->children[0]);
                            ft = localExpr->resultType;
                        }
                        else
                        {
                            ft = local->varType;
                        }
                    }
                    lastcls = ft.reference;
                    lastfound = resolved;
                }
                else
                {
                    // check if it's a class
                    ZTreeNode* typeFound = resolveType(firstSymbol, context);
                    if (!typeFound)
                    {
                        fullfound = false;
                        break;
                    }
                    // found a type: switch mode
                    isstatic = true;
                    lastcls = typeFound;
                    lastfound = nullptr;
                    // also mark this type as type
                    parsedTokens.append(ParserToken(leaf.token, ParserToken::TypeName, typeFound, leaf.token.value));
                }
            }
            else if (lastcls)
            {
                // find field/method
                for (ZTreeNode* node : lastcls->children)
                {
                    if ((node->type() == ZTreeNode::Method || node->type() == ZTreeNode::Field || node->type() == ZTreeNode::Constant || node->type() == ZTreeNode::Struct) &&
                            !node->identifier.compare(leaf.token.value, Qt::CaseInsensitive))
                    {
                        // check if static
                        bool fisstatic = false;
                        if (node->type() == ZTreeNode::Field)
                        {
                            ZField* field = reinterpret_cast<ZField*>(node);
                            fisstatic = field->flags.contains("static");
                        }
                        else if (node->type() == ZTreeNode::Method)
                        {
                            ZMethod* method = reinterpret_cast<ZMethod*>(node);
                            fisstatic = method->flags.contains("static");
                        }
                        else if (node->type() == ZTreeNode::Constant)
                        {
                            fisstatic = true; // but with this we cannot have typed constants
                        }
                        // matched
                        // mark this field
                        ParserToken::TokenType t = ParserToken::Field;
                        if (node->type() == ZTreeNode::Method)
                            t = ParserToken::Method;
                        else if (node->type() == ZTreeNode::Constant)
                            t = ParserToken::ConstantName;
                        else if (node->type() == ZTreeNode::Struct)
                            t = ParserToken::TypeName;
                        parsedTokens.append(ParserToken(leaf.token, t, node, getFullFieldName(node)));
                        // find type of this field
                        // if it's a constant, there can be no type...
                        if (node->type() == ZTreeNode::Constant)
                        {
                            lastcls = nullptr;
                            // todo mark as constant
                            continue;
                        }
                        // if it's a struct, use type directly. I'm not sure it works correctly
                        if (isstatic && node->type() == ZTreeNode::Struct)
                        {
                            lastcls = node;
                            parsedTokens.append(ParserToken(leaf.token, ParserToken::TypeName, node, leaf.token.value));
                            continue;
                        }
                        // if it's a field, we have field type.
                        ZCompoundType ft;
                        if (node->type() == ZTreeNode::Method)
                        {
                            ZMethod* method = reinterpret_cast<ZMethod*>(node);
                            ft = method->returnTypes[0];
                            // todo mark as method access
                        }
                        else if (node->type() == ZTreeNode::Field)
                        {
                            ZField* field = reinterpret_cast<ZField*>(node);
                            ft = field->fieldType;
                            // todo mark as field access
                        }
                        if (fisstatic != isstatic) // if we need static and it's not static, then we cannot really use this
                        {
                            fullfound = false;
                            break;
                        }
                        lastcls = ft.reference;
                        lastfound = node;
                    }
                }
            }
            else
            {
                fullfound = false;
                break; // todo make a warning about unresolved identifier along the chain
            }
        }

        if (fullfound && lastfound)
        {
            // type of this expression will be either type of last field, or type of method return value
            if (lastfound->type() == ZTreeNode::Field)
            {
                ZField* field = reinterpret_cast<ZField*>(lastfound);
                expr->resultType = field->fieldType;
            }
            else if (lastfound->type() == ZTreeNode::Method)
            {
                ZMethod* method = reinterpret_cast<ZMethod*>(lastfound);
                expr->resultType = method->returnTypes[0];
            }
            else if (lastfound->type() == ZTreeNode::Constant)
            {
                // todo: detect constant type. this is not available yet.
                // this is the same as literal type
            }
        }

        return;
    }

    static QList<QString> keywords = QList<QString>() << "true" << "false" << "null";

    for (ZExpressionLeaf& leaf : expr->leaves)
    {
        switch (leaf.type)
        {
        case ZExpressionLeaf::Boolean:
            parsedTokens.append(ParserToken(leaf.token, ParserToken::Keyword));
            break;
        case ZExpressionLeaf::Identifier:
        {
            if (keywords.contains(leaf.token.value.toLower()))
                parsedTokens.append(ParserToken(leaf.token, ParserToken::Keyword));
            ZTreeNode* resolved = resolveSymbol(leaf.token.value, parent, context);
            if (resolved)
            {
                ParserToken::TokenType t = ParserToken::Local;
                switch (resolved->type())
                {
                case ZTreeNode::LocalVariable:
                    t = (resolved->parent && resolved->parent->type() == ZTreeNode::Method) ? ParserToken::Argument : ParserToken::Local;
                    break;
                case ZTreeNode::Field:
                    t = ParserToken::Field;
                    break;
                case ZTreeNode::Method:
                    t = ParserToken::Method;
                    break;
                default:
                    break;
                }
                parsedTokens.append(ParserToken(leaf.token, t, resolved, leaf.token.value));
            }
            break;
        }
        case ZExpressionLeaf::Expression:
            highlightExpression(leaf.expr, parent, context);
            break;
        case ZExpressionLeaf::Integer:
        case ZExpressionLeaf::Double:
            parsedTokens.append(ParserToken(leaf.token, ParserToken::Number));
            break;
        case ZExpressionLeaf::String:
            parsedTokens.append(ParserToken(leaf.token, ParserToken::String));
            break;
        default:
            break;
        }
    }

    // call expression has the type of whatever is being called
    if (expr->op == ZExpression::Call && expr->leaves.size())
    {
        // normally either identifier or expression
        ZExpressionLeaf& leaf = expr->leaves[0];
        if (leaf.type == ZExpressionLeaf::Expression)
        {
            expr->resultType = leaf.expr->resultType;
        }
        else if (leaf.type == ZExpressionLeaf::Identifier)
        {
            ZTreeNode* resolved = resolveSymbol(leaf.token.value, parent, context);
            if (resolved && resolved->type() == ZTreeNode::Method)
            {
                ZMethod* method = reinterpret_cast<ZMethod*>(resolved);
                expr->resultType = method->returnTypes[0];
            }
        }
    }
}
