#include "tokenizer.h"

QList<Tokenizer::TokenInfo> Tokenizer::TokenInfos;
QMap<int, Tokenizer::TokenInfo*> Tokenizer::TokenInfosByNum;

bool Tokenizer::compareTokenLength(const Tokenizer::TokenInfo& a, const Tokenizer::TokenInfo& b)
{
    return (a.content.length() > b.content.length());
}

Tokenizer::Tokenizer(QString input) : data(input)
{
    stream.setString(&data);

    if (!TokenInfos.size())
    {
        // initialize the list.
        #define DEFINE_TOKEN1(num, token) TokenInfos.append({ name: #token, number: num, content: "" });
        #define DEFINE_TOKEN2(num, token, c) TokenInfos.append({ name: #token, number: num, content: c });
        #include "tokens.h"
        #undef DEFINE_TOKEN2
        #undef DEFINE_TOKEN1

        // sort tokens by content length
        std::sort(TokenInfos.begin(), TokenInfos.end(), compareTokenLength);
        for (QList<TokenInfo>::iterator it = TokenInfos.begin(); it != TokenInfos.end(); ++it)
        {
            TokenInfo* info = &(*it);
            TokenInfosByNum[info->number] = info;
        }
    }

    lastPos = 0;
    curLine = 1;
    maxLine = 1;
    for (QChar c : input)
    {
        if (c == '\n')
            maxLine++;
    }
}

QString Tokenizer::tokenToString(quint64 token)
{
    bool many = false;
    QString output = "";
    for (int i = 0; i < 64; i++)
    {
        if ((1ull<<i) & token)
        {
            // find token in TokenInfos
            if (TokenInfosByNum.contains(i))
            {
                TokenInfo* info = TokenInfosByNum[i];
                if (output.length())
                {
                    output += ", ";
                    many = true;
                }
                output += info->name;
            }
        }
    }

    if (many) return "["+output+"]";
    return output;
}

int Tokenizer::length()
{
    return data.length(); // ?
}

void Tokenizer::setPosition(int pos)
{
    stream.seek(pos);
    // find line number
    // remember that our stream is a string. cheap iteration works
    curLine = 1;
    int xpos = 0;
    for (QChar c : *stream.string())
    {
        if (c == '\n')
            curLine++;
        xpos++;
        if (xpos >= pos)
            break;
    }
}

int Tokenizer::position()
{
    return int(stream.pos());
}

bool Tokenizer::tryReadWhitespace(Tokenizer::Token& out)
{
    int cpos = lastPos = position();
    QChar c;
    stream >> c;

    QString whitespace = " \r\t\u00A0";
    QString outv = "";

    if (whitespace.contains(c))
    {
        if (c == '\n')
            curLine++;

        outv += c;
        while (true)
        {
            stream >> c;
            if (whitespace.contains(c))
            {
                outv += c;
                continue;
            }

            if (c.isNull())
                break;
            setPosition(position()-1);
            break;
        }

        out.type = Whitespace;
        out.startsAt = cpos;
        out.endsAt = position();
        out.line = line();
        out.value = outv;
        return true;
    }

    setPosition(cpos);
    return false;
}

bool Tokenizer::tryReadIdentifier(Tokenizer::Token& out)
{
    int cpos = lastPos = position();
    QChar c;
    stream >> c;

    if (c.isNull())
        return false;

    QString outv = "";

    if ((c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '_'))
    {
        outv += c;
        while (true)
        {
            stream >> c;

            if (c.isNull())
                break;

            if ((c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c == '_') ||
                (c >= '0' && c <= '9'))
            {
                outv += c;
                continue;
            }

            setPosition(position()-1);
            break;
        }

        out.type = Identifier;
        out.startsAt = cpos;
        out.endsAt = position();
        out.line = line();
        out.value = outv;
        return true;
    }

    setPosition(cpos);
    return false;
}

bool Tokenizer::tryReadNumber(Tokenizer::Token& out)
{
    int cpos = lastPos = position();
    QChar c;
    stream >> c;

    if (c.isNull())
        return false;

    bool isint, isdouble, isoctal, ishex, isnegative, isexponent;
    bool dotisvalid = true;

    QString outv;

    /*
    // check for minus sign
    isnegative = false;
    if (c == '-')
    {
        isnegative = true;
        outv += c;
        stream >> c;
        if (c.isNull())
            return false;
    }
    */

    // check for initial double marker
    isdouble = false;
    if (c == '.')
    {
        isdouble = true;
        dotisvalid = false;
        outv += c;
        stream >> c;
        if (c.isNull())
            return false;
    }

    if (c >= '0' && c <= '9')
    {
        isint = !isdouble;
        isexponent = false;
        isoctal = false;
        ishex = false;

        outv += c;

        // peek in front for hex
        if (c == '0')
        {
            QChar _c;
            stream >> _c;
            if (_c == 'x' || _c == 'X') // is uppercase allowed?
            {
                ishex = true;
                outv += _c;
            }
            else if (!_c.isNull())
                setPosition(position()-1);
        }

        isoctal = (c == '0') && !ishex;

        //
        while (true)
        {
            stream >> c;
            if ((c >= '0' && c <= '9') ||
                (!isexponent && ishex && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))))
            {
                if (c >= '8') isoctal = false;
                outv += c; // regular number
            }
            else if (!isexponent && (c == 'e' || c == 'E'))
            {
                isint = false;
                isdouble = true;
                outv += c;
                // check for negative exponent
                QChar _c;
                stream >> _c;
                if (_c == '-')
                    outv += _c;
                else if (!_c.isNull())
                    setPosition(position()-1);
                isexponent = true;
                dotisvalid = false;
            }
            else if (dotisvalid && c == '.')
            {
                isint = false;
                isdouble = true;
                outv += c;
                dotisvalid = false;
            }
            else
            {
                if (c.isNull())
                    break;
                setPosition(position()-1);
                break;
            }
        }

        out.type = isdouble ? Double : Integer;
        out.startsAt = cpos;
        out.endsAt = position();
        out.line = line();
        out.value = outv;
        if (!isdouble && (ishex || isoctal))
        {
            bool ok;
            int base = 10;
            assert (ishex != isoctal);
            if (ishex) base = 16;
            else if (isoctal) base = 8;
            out.valueInt = out.value.toLongLong(&ok, base);
            out.valueDouble = out.valueInt;
            if (!ok)
                out.type = Invalid;
        }
        else
        {
            bool ok;
            out.valueDouble = out.value.toDouble(&ok);
            out.valueInt = int(out.valueDouble);
            if (!ok)
                out.type = Invalid;
        }

        return true;
    }

    setPosition(cpos);
    return false;
}

bool Tokenizer::tryReadStringOrComment(Token& out, bool allowstring, bool allowname, bool allowblock, bool allowline)
{
    int cpos = lastPos = position();
    QChar c;
    stream >> c;

    QString outv;

    switch (c.unicode())
    {
        case '/': // comment
        {
            if (!allowblock && !allowline) break;
            QChar cnext;
            stream >> cnext;
            if (cnext == '/')
            {
                if (!allowline) break;
                // line comment: read until newline but not including it
                while (true)
                {
                    stream >> c;
                    if (c.isNull())
                        break;
                    if (c == '\n')
                    {
                        setPosition(position()-1);
                        break;
                    }

                    outv += c;
                }

                out.type = LineComment;
                out.startsAt = cpos;
                out.endsAt = position();
                out.line = line();
                out.value = outv;
                return true;
            }
            else if (cnext == '*')
            {
                if (!allowblock) break;
                // block comment: read until closing sequence
                while (true)
                {
                    stream >> c;
                    if (c == '*' || c.isNull())
                    {
                        if (c.isNull())
                            break;
                        if (c == '\n')
                            curLine++;
                        QChar cnext;
                        stream >> cnext;
                        if (cnext == '/' || cnext.isNull())
                            break;
                        setPosition(position()-1);
                    }

                    outv += c;
                }

                out.type = BlockComment;
                out.startsAt = cpos;
                out.endsAt = position();
                out.line = line();
                out.value = outv;
                return true;
            }
            break;
        }

        case '"':
        case '\'':
        {
            if ((c == '"' && !allowstring) || (c == '\'' && !allowname)) break;
            TokenType type = (c == '"') ? String : Name;
            while (true)
            {
                // todo: parse escape sequences properly
                stream >> c;
                if (c == '\\') // escape sequence. right now, do nothing
                {
                    outv += c; // include the "\"
                    stream >> c;
                    if (!c.isNull())
                        outv += c;
                }
                else if ((c == '"' && type == String) || (c == '\'' && type == Name) || c.isNull())
                {
                    out.type = type;
                    out.startsAt = cpos;
                    out.endsAt = position();
                    out.line = line();
                    out.value = outv;
                    if (c.isNull())
                        out.isValid = false;
                    return true;
                }
                else outv += c;
                if (c == '\n')
                    curLine++;
            }
        }

        default:
            break;
    }

    setPosition(cpos);
    return false;
}

bool Tokenizer::tryReadNamedToken(Token& out)
{
    int cpos = lastPos = position();
    int largestLen = TokenInfos[0].content.length();
    QString stok = stream.read(largestLen).toLower();
    for (QList<TokenInfo>::iterator it = TokenInfos.begin(); it != TokenInfos.end(); ++it)
    {
        const TokenInfo& info = (*it);
        if (!info.content.length())
            break;
        if (stok.startsWith(info.content.toLower()))
        {
            if (info.number == int(Tokenizer::Newline))
                curLine++;
            out.type = TokenType(1ull << info.number);
            out.startsAt = cpos;
            out.value = info.content;
            setPosition(cpos + info.content.length());
            out.endsAt = position();
            out.line = line();
            return true;
        }
    }

    setPosition(cpos);
    return false;
}

bool Tokenizer::expectToken(Token& out, quint64 oneOf)
{
    int cpos = position();

    if (oneOf & Whitespace)
    {
        if (tryReadWhitespace(out))
            return true;
    }

    if (oneOf & Identifier)
    {
        if (tryReadIdentifier(out))
            return true;
    }

    if (oneOf & (LineComment|BlockComment|String|Name))
    {
        if (tryReadStringOrComment(out, oneOf & String, oneOf & Name, oneOf & BlockComment, oneOf & LineComment))
            return true;
    }

    if (oneOf & (Integer|Double))
    {
        if (tryReadNumber(out))
            return true;
    }

    int largestLen = TokenInfos[0].content.length();
    QString stok = stream.read(largestLen).toLower();
    for (QList<TokenInfo>::iterator it = TokenInfos.begin(); it != TokenInfos.end(); ++it)
    {
        const TokenInfo& info = (*it);
        if (!((1ull << info.number) & oneOf))
            continue;
        if (!info.content.length())
            break;
        if (stok.startsWith(info.content.toLower()))
        {
            out.type = TokenType(1ull << info.number);
            out.value = info.content;
            out.startsAt = cpos;
            setPosition(cpos + info.content.length());
            out.endsAt = position();
            out.line = line();
            return true;
        }
    }

    // token was not found
    setPosition(cpos);
    return false;
}

bool Tokenizer::readToken(Token& out, bool shortCircuit)
{
    if (stream.atEnd())
        return false;

    if (tryReadWhitespace(out))
        return true;

    if (!shortCircuit)
    {
        if (tryReadIdentifier(out))
            return true;
        if (tryReadNumber(out))
            return true;
    }

    if (tryReadStringOrComment(out, true, true, true, true))
        return true;

    if (!shortCircuit)
    {
        if (tryReadNamedToken(out))
            return true;
    }

    out.type = Invalid;
    out.value = stream.read(1);
    out.startsAt = position()-1;
    out.endsAt = out.startsAt+1;
    out.line = line();;
    out.isValid = false;
    return true;
}

QList<Tokenizer::Token> Tokenizer::readAllTokens()
{
    QList<Token> tokens;
    Token tok;
    while (readToken(tok))
        tokens.append(tok);
    return tokens;
}

TokenStream::TokenStream(QList<Tokenizer::Token>& toklst) : _tokens(toklst)
{
    _pos = 0;
}

int TokenStream::length()
{
    return _tokens.size();
}

void TokenStream::setPosition(int position)
{
    _pos = position;
}

int TokenStream::position()
{
    return _pos;
}

bool TokenStream::expectToken(Tokenizer::Token& out, quint64 oneOf)
{
    out.type = Tokenizer::Invalid;
    out.value = "EOS";
    if (!isPositionValid())
        return false;
    Tokenizer::Token& next = _tokens[_pos];
    out = next;

    if (out.type & oneOf)
    {
        _pos++;
        return true;
    }

    return false;
}

bool TokenStream::readToken(Tokenizer::Token& out)
{
    out.type = Tokenizer::Invalid;
    out.value = "EOS";
    out.line = _tokens.size() ? _tokens.last().line : 1;
    if (!isPositionValid())
        return false;
    Tokenizer::Token& next = _tokens[_pos];
    out = next;
    _pos++;
    return true;
}

bool TokenStream::peekToken(Tokenizer::Token& out)
{
    out.type = Tokenizer::Invalid;
    out.value = "EOS";
    out.line = _tokens.size() ? _tokens.last().line : 1;
    if (!isPositionValid())
        return false;
    out = _tokens[_pos];
    return true;
}

bool TokenStream::isPositionValid()
{
    return (_tokens.size() > 0 && _pos >= 0 && _pos < _tokens.size());
}
