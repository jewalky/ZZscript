#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <QString>
#include <QList>
#include <QMap>
#include <QTextStream>

class Tokenizer
{
public:
    Tokenizer(QString input);

    enum TokenType
    {
        // now this is ZDoom-like
        #define DEFINE_TOKEN1(num, token) token = 1ull<<num##ull,
        #define DEFINE_TOKEN2(num, token, c) token = 1ull<<num##ull,
        #include "tokens.h"
        #undef DEFINE_TOKEN1
        #undef DEFINE_TOKEN2
    };

    static QString tokenToString(quint64 token);

    struct Token
    {
        TokenType type;
        QString value;
        qint64 valueInt;
        double valueDouble;
        bool isValid;
        int startsAt;
        int endsAt;
        int line;

        Token()
        {
            isValid = true;
            valueInt = 0;
            valueDouble = 0.0;
            type = Invalid;
        }

        QString toString()
        {
            return QString("<Token.%1 (%2)>").arg(tokenToString(type), value);
        }

        char* toCString()
        {
            return toString().toUtf8().data();
        }
    };

    //
    bool expectToken(Token& out, quint64 oneOf);
    bool readToken(Token& out, bool shortCircuit = false);

    //
    int length();
    void setPosition(int pos);
    int position();
    int lastPosition() { return lastPos; }
    int line() { return curLine; }

    QList<Token> readAllTokens();

private:
    struct TokenInfo
    {
        QString name;
        int number;
        QString content;
    };

    static QList<TokenInfo> TokenInfos;
    static QMap<int, TokenInfo*> TokenInfosByNum;
    static bool compareTokenLength(const Tokenizer::TokenInfo& a, const Tokenizer::TokenInfo& b);

    //
    bool tryReadWhitespace(Token& out);
    bool tryReadIdentifier(Token& out);
    bool tryReadNumber(Token& out);
    bool tryReadStringOrComment(Token& out, bool allowstring, bool allowname, bool allowblock, bool allowline);
    bool tryReadNamedToken(Token& out);

    QString data;
    QTextStream stream;

    int lastPos;
    int curLine;
    int maxLine;
};

class TokenStream
{
public:
    TokenStream(QList<Tokenizer::Token>& toklst);

    int length();
    void setPosition(int _pos);
    int position();

    bool expectToken(Tokenizer::Token& out, quint64 oneOf);
    bool readToken(Tokenizer::Token& out);
    bool peekToken(Tokenizer::Token& out);

    bool isPositionValid();

private:
    int _pos;
    QList<Tokenizer::Token> _tokens;
};

#endif // TOKENIZER_H
