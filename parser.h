#ifndef PARSER_H
#define PARSER_H

#include <QPair>
#include "tokenizer.h"

struct ZTreeNode
{
    enum NodeType
    {
        Generic,
        FileRoot,
        Include,
        Enum,
        Class,
        Struct,
        Field,
        Method,
        Code,
        Expression,
        ForCycle,
        WhileCycle,
        Condition,
        Constant
    };

    ZTreeNode* parent;
    QString identifier;
    QList<ZTreeNode*> children;
    bool isValid;
    QString error;

    explicit ZTreeNode(ZTreeNode* p) { parent = p; isValid = false; }
    virtual ~ZTreeNode();

    virtual NodeType type() { return Generic; }
};

struct ZFileRoot : public ZTreeNode
{
    ZFileRoot(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return FileRoot; }

    // this is the API version to use for parsing and compat separation
    // if not specified, defaults to 2.8 IIRC
    QString version;
};

struct ZInclude : public ZTreeNode
{
    ZInclude(ZTreeNode* p) : ZTreeNode(p)
    {
        reference = nullptr;
    }

    virtual NodeType type() { return Include; }

    QString location;
    ZFileRoot* reference;
};

struct ZStruct;
struct ZCompoundType
{
    QString type;
    ZStruct* reference;
    QList<ZCompoundType> arguments; // example: Array<Actor>
    QList<int> arrayDimensions; // example: string s[8];
};

struct ZField : public ZTreeNode
{
    ZField(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Field; }

    ZCompoundType fieldType;
    QList<QString> flags;
    QString version;
    QString deprecated;
};

struct ZExpression;
struct ZMethod : public ZTreeNode
{
    ZMethod(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Method; }

    struct Argument
    {
        QString name;
        ZExpression* defaultValue;
        ZCompoundType type;
    };

    QList<ZCompoundType> returnTypes;
    QList<Argument> arguments;
    QList<QString> flags;
    QString version;
    QString deprecated;
    bool hasEllipsis;

    // children = parsed method statements (expressions, etc)
    // tokens = after parseObjectFields, but before parseObjectMethods
    QList<Tokenizer::Token> tokens;
};

struct ZStruct : public ZTreeNode
{
    ZStruct(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Struct; }

    QString version;
    QString deprecated;
    QList<QString> flags;
    // after parseRoot, but before parseObjectFields
    QList<Tokenizer::Token> tokens;

    // children = ZField, ZConstant, ZProperty.. (for classes)
};

struct ZClass : public ZStruct
{
    ZClass(ZTreeNode* p) : ZStruct(p)
    {
        parentReference = extendReference = replaceReference = nullptr;
    }

    virtual NodeType type() { return Class; }

    QString parentName;
    QString extendName;
    QString replaceName;

    ZClass* parentReference;
    ZClass* extendReference;
    ZClass* replaceReference;

    QList<ZClass*> extensions;
    QList<ZClass*> childrenReferences;
    QList<ZClass*> replacedByReferences; // this is used later for checking

    // todo: class and actor magic:
    // - flags (actor flags)
    // - props
    // - default props (i.e. set props)
    // - states
};

struct ZExpressionLeaf
{
    // either identifier or literal
    // literal types:
    //  - int
    //  - double
    //  - bool
    //  - string
    //  - vector (not supported)

    enum
    {
        Invalid,
        Identifier,
        Integer,
        Double,
        Boolean,
        String,
        Vector,
        Expression,
        Token // this is only for parsing
    } type;

    Tokenizer::Token token;
    ZExpression* expr;

    ZExpressionLeaf() {};
    explicit ZExpressionLeaf(const Tokenizer::Token& t) : token(t) {}
};

struct ZExpression : public ZTreeNode
{
    ZExpression(ZTreeNode* p) : ZTreeNode(p) {}
    virtual ~ZExpression();

    virtual NodeType type() { return Expression; }

    enum Operator
    {
        // arithmetic base
        Add,
        Sub,
        Mul,
        Div,
        Modulo,

        UnaryNot,
        UnaryMinus,
        UnaryNeg,
        Identifier,
        Member,
        Call,
        Literal,
        Array,
        Cast,

        // bitwise
        BitOr,
        BitAnd,
        BitShr,
        BitShl,
        Xor,

        // logical
        LogicalAnd,
        LogicalOr,
        // assignment
        // comparison
        CmpLT,
        CmpGT,
        CmpLTEQ,
        CmpGTEQ,
        CmpSpaceship,
        CmpEq,
        CmpNotEq,
        CmpSomewhatEq,
        // todo

        Invalid = -1
    };

    Operator op;

    QList<Tokenizer::Token> operatorTokens;
    QList<ZExpressionLeaf> leaves;

    bool evaluate(ZExpressionLeaf& out, QString& type);
    bool evaluateLeaf(ZExpressionLeaf& in, ZExpressionLeaf& out, QString& type);
};

struct ZEnum : public ZTreeNode
{
    ZEnum(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Enum; }

    QString version;
    QList< QPair<QString, ZExpression*> > values;
};

struct ParserToken
{
    Tokenizer::Token token;

    ParserToken()
    {
        startsAt = endsAt = 0;
        type = Text;
    }

    ParserToken(Tokenizer::Token tok)
    {
        token = tok;
        startsAt = token.startsAt;
        endsAt = token.endsAt;
    }

    enum TokenType
    {
        Invalid,
        Text,
        Comment,
        Preprocessor,
        Keyword,
        PrimitiveName, // int, bool...
        TypeName, // class names
        ConstantName,
        Number,
        String,
        Field,
        Local,
        Method,
        Argument,
        Operator,
        SpecialToken
    };

    int startsAt;
    int endsAt;
    TokenType type;
};

class Parser
{
public:
    explicit Parser(QList<Tokenizer::Token> tokens);
    virtual ~Parser();
    // parse() populates initial values (includes, root enums, classes, structs)
    bool parse();
    // setTypeInformation() is used pretty much to concatenate classes from included files into this one.
    // expected usage is that the outside code will call parse() on all includes, then generate combined list of types and do deep parsing.
    void setTypeInformation(QList<ZStruct*> types);
    // parseClassFields and parseStructFields will parse fields and method signatures inside objects
    // (and substructs)
    bool parseClassFields(ZClass* cls) { return parseObjectFields(cls, cls); }
    bool parseStructFields(ZStruct* struc) { return parseObjectFields(nullptr, struc); }
    // parseClassMethods and parseStructMethods will parse method bodies (knowing all possible types and fields at this point)
    bool parseClassMethods(ZClass* cls) { return parseObjectMethods(cls, cls); }
    bool parseStructMethods(ZStruct* struc) { return parseObjectMethods(nullptr, struc); }

    // Parser operates at File level
    //
    ZFileRoot* root;
    QList<ParserToken> parsedTokens;

    void reportError(QString err);
    void reportWarning(QString warn);

private:
    QList<Tokenizer::Token> tokens;
    QList<ZStruct*> types;

    bool skipWhitespace(TokenStream& stream, bool newline);
    bool consumeTokens(TokenStream& stream, QList<Tokenizer::Token>& out, quint64 stopAtAnyOf);

    ZExpression* parseExpression(TokenStream& stream, quint64 stopAtAnyOf);
    void dumpExpression(ZExpression* expr, int level);

    // these occur at the root scope
    bool parseRoot(TokenStream& stream);
    ZClass* parseClass(TokenStream& stream, bool extend);
    ZStruct* parseStruct(TokenStream& stream);
    ZEnum* parseEnum(TokenStream& stream);

    // this occurs in the class and struct body
    bool parseCompoundType(TokenStream& stream, ZCompoundType& type);
    bool parseObjectFields(ZClass* cls, ZStruct* struc);

    // this occurs in methods
    bool parseObjectMethods(ZClass* cls, ZStruct* struc);
};

#endif // PARSER_H
