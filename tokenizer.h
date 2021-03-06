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

        char c[128];
        const char* toCString()
        {
            const QByteArray& a = toString().toUtf8();
            const char* d = a.constData();
            int sz = a.size();
            if (sz > 127)
                sz = 127;
            memset((void*)c, 0, 128);
            memcpy((void*)c, d, sz);
            return c;
        }

        void makeLower()
        {
            value = value.toLower();
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
    int line();

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
    int dataPos;
    QList<int> dataLineNumbers;

    int lastPos;
    int maxLine;

    inline QChar readChar()
    {
        if (dataPos >= data.length() || dataPos < 0)
            return 0;
        QChar o = data[dataPos];
        dataPos++;
        return o;
    }

    inline QStringRef readString(int len)
    {
        int start = dataPos;
        int end = len;
        if (start > data.length())
            start = dataPos;
        if (start+len > data.length())
            end = data.length()-dataPos;
        if (start < 0)
        {
            len += start;
            start = 0;
        }
        if (len < 0)
            len = 0;
        return QStringRef(&data, start, end);
    }
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
