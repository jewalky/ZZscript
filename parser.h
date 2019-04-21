#ifndef PARSER_H
#define PARSER_H

#include <QPair>
#include <QPointer>
#include <QSharedPointer>
#include "tokenizer.h"

class ZTreeNode : public QObject
{
    Q_OBJECT
public:
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
        CodeBlock,
        Expression,
        ForCycle,
        WhileCycle,
        Condition,
        Constant,
        Property,
        LocalVariable,
        ExecutionControl,
        SystemType
    };

    QSharedPointer<ZTreeNode> parent;
    QString identifier;
    QList<QSharedPointer<ZTreeNode>> children;
    bool isValid;
    QString error;

    explicit ZTreeNode(QSharedPointer<ZTreeNode> p) { parent = p; isValid = false; }
    virtual ~ZTreeNode();

    virtual NodeType type() { return Generic; }
};

class Parser;
class ZFileRoot : public ZTreeNode
{
    Q_OBJECT
public:

    ZFileRoot(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return FileRoot; }

    // this is the API version to use for parsing and compat separation
    // if not specified, defaults to 2.8 IIRC
    QString version;
    QString fullPath;
    QString relativePath;

    //
    Parser* parser;
};

class ZInclude : public ZTreeNode
{
    Q_OBJECT
public:

    ZInclude(QSharedPointer<ZTreeNode> p) : ZTreeNode(p)
    {
        reference = nullptr;
    }

    virtual NodeType type() { return Include; }

    QString location;
    QSharedPointer<ZFileRoot> reference;
};

class ZExpression;
class ZStruct;
class ZSystemType : public ZTreeNode
{
    Q_OBJECT
public:

    ZSystemType(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    ZSystemType(const ZSystemType& other) : ZTreeNode(nullptr)
    {
        (*this)=other;
    }

    ZSystemType& operator=(const ZSystemType& other)
    {
        kind = other.kind;
        size = other.size;
        replaceType = other.replaceType;
        return *this;
    }

    virtual NodeType type() { return SystemType; }

    enum SystemTypeKind
    {
        SType_String,
        SType_Integer,
        SType_Float,
        SType_Vector,
        SType_Array,
        SType_Class,
        SType_Readonly,
        SType_Void,
        SType_Object
    };

    SystemTypeKind kind;
    int size;
    QString replaceType;

    ZSystemType() : ZTreeNode(nullptr) {}
    ZSystemType(QString tname, SystemTypeKind tkind, int tsize, QString treplaceType = "")
        : ZTreeNode(nullptr), kind(tkind), size(tsize), replaceType(treplaceType)
    {
        identifier = tname;
    }
};

struct ZCompoundType
{
    QString type;
    QSharedPointer<ZTreeNode> reference;
    QList<ZCompoundType> arguments; // example: Array<Actor>
    QList<QSharedPointer<ZExpression>> arrayDimensions; // example: string s[8]; or string s[SIZE];

    ZCompoundType()
    {
        reference = nullptr;
    }

    void destroy()
    {
        // not needed anymore
    }

    bool isSystem()
    {
        return (reference && reference->type() == ZTreeNode::SystemType);
    }
};

class ZField : public ZTreeNode
{
    Q_OBJECT
public:

    ZField(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return Field; }

    virtual ~ZField()
    {
        fieldType.destroy();
    }

    ZCompoundType fieldType;
    QList<QString> flags;
    QString version;
    QString deprecated;
    int lineNumber;

    // children = (if any) = const array expression
};

class ZLocalVariable : public ZTreeNode
{
    Q_OBJECT
public:

    ZLocalVariable(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return LocalVariable; }

    virtual ~ZLocalVariable()
    {
        varType.destroy();
    }

    bool hasType; // false if "let"
    ZCompoundType varType;
    QList<QString> flags; // "const"
    int lineNumber;

    // children = (if any) = initializer expression
};

class ZCodeBlock : public ZTreeNode
{
    Q_OBJECT
public:

    ZCodeBlock(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return CodeBlock; }

    // code block holds ZLocalVariables, ZExpressions, and various cycles (each of them also has a code block)
};

class ZExecutionControl : public ZTreeNode
{
    Q_OBJECT
public:

    ZExecutionControl(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return ExecutionControl; }

    //
    enum Type
    {
        CtlReturn,
        CtlBreak,
        CtlContinue
    };

    Type ctlType;
    // children = (in case of return) = return expressions
};

class ZForCycle : public ZTreeNode
{
    Q_OBJECT
public:

    ZForCycle(QSharedPointer<ZTreeNode> p) : ZTreeNode(p)
    {
        condition = nullptr;
    }

    virtual NodeType type() { return ForCycle; }

    //
    QList<QSharedPointer<ZTreeNode>> initializers;
    QSharedPointer<ZExpression> condition;
    QList<QSharedPointer<ZExpression>> step;

    virtual ~ZForCycle();

    // children = code block
};

class ZCondition : public ZTreeNode
{
    Q_OBJECT
public:

    ZCondition(QSharedPointer<ZTreeNode> p) : ZTreeNode(p)
    {
        condition = nullptr;
        elseBlock = nullptr;
    }

    virtual NodeType type() { return Condition; }

    //
    QSharedPointer<ZExpression> condition;

    //
    QSharedPointer<ZTreeNode> elseBlock;

    virtual ~ZCondition();
};

class ZConstant : public ZTreeNode
{
    Q_OBJECT
public:

    ZConstant(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return Constant; }

    // children = expression
    int lineNumber;
};

class ZProperty : public ZTreeNode
{
    Q_OBJECT
public:

    ZProperty(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return Property; }

    // identifier = prop name
    // property <name> : <field1> [, <field2> ... ]
    QList<QString> fields;
    int lineNumber;
};

class ZMethod : public ZTreeNode
{
    Q_OBJECT
public:

    ZMethod(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return Method; }

    virtual ~ZMethod()
    {
        // not needed anymore
    }

    QList<ZCompoundType> returnTypes;
    QList<QSharedPointer<ZLocalVariable>> arguments;
    QList<QString> flags;
    QString version;
    QString deprecated;
    bool hasEllipsis;
    int lineNumber;

    // children = parsed method statements (expressions, etc)
    // tokens = after parseObjectFields, but before parseObjectMethods
    QList<Tokenizer::Token> tokens;
};

class ZStruct : public ZTreeNode
{
    Q_OBJECT
public:

    ZStruct(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) { self = nullptr; }
    virtual ~ZStruct();
    virtual NodeType type() { return Struct; }

    QString version;
    QString deprecated;
    QList<QString> flags;
    // after parseRoot, but before parseObjectFields
    QList<Tokenizer::Token> tokens;
    int lineNumber;

    QSharedPointer<ZLocalVariable> self;

    // children = ZField, ZConstant, ZProperty.. (for classes)
};

class ZClass : public ZStruct
{
    Q_OBJECT
public:

    ZClass(QSharedPointer<ZTreeNode> p) : ZStruct(p)
    {
        parentReference = extendReference = replaceReference = nullptr;
    }

    virtual NodeType type() { return Class; }

    QString parentName;
    QString extendName;
    QString replaceName;

    QSharedPointer<ZClass> parentReference;
    QSharedPointer<ZClass> extendReference;
    QSharedPointer<ZClass> replaceReference;

    QList<QSharedPointer<ZClass>> extensions;
    QList<QSharedPointer<ZClass>> childrenReferences;
    QList<QSharedPointer<ZClass>> replacedByReferences; // this is used later for checking

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
        Expression,
        Token // this is only for parsing
    } type;

    Tokenizer::Token token;
    QSharedPointer<ZExpression> expr;

    ZExpressionLeaf() { expr = nullptr; };
    explicit ZExpressionLeaf(const Tokenizer::Token& t) : token(t) { expr = nullptr; }
};

class ZExpression : public ZTreeNode
{
    Q_OBJECT
public:

    ZExpression(QSharedPointer<ZTreeNode> p) : ZTreeNode(p)
    {
        op = Invalid;
        assign = false;
    }

    virtual ~ZExpression();

    virtual NodeType type() { return Expression; }

    enum Operator
    {
        //
        Assign,

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
        Cast,
        ArraySubscript,

        // bitwise
        BitOr,
        BitAnd,
        BitShr,
        BitShrUs,
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

        // post/pre increments
        Increment, // this is used in the merge function
        Decrement, // this is used in the merge function
        PostIncrement, // this and the next 3 are stored into the resulting object
        PreIncrement,
        PostDecrement,
        PreDecrement,

        //
        VectorInitialization,
        ArrayInitialization,

        // magic
        Ternary,

        // more magic
        VectorDot,
        VectorCross,

        Invalid = -1
    };

    Operator op;
    bool assign;

    QList<Tokenizer::Token> operatorTokens;
    QList<Tokenizer::Token> specialTokens;
    QList<ZExpressionLeaf> leaves;

    ZCompoundType resultType;

    bool evaluate(ZExpressionLeaf& out, QString& type);
    bool evaluateLeaf(ZExpressionLeaf& in, ZExpressionLeaf& out, QString& type);
};

class ZEnum : public ZTreeNode
{
    Q_OBJECT
public:

    ZEnum(QSharedPointer<ZTreeNode> p) : ZTreeNode(p) {}
    virtual NodeType type() { return Enum; }

    QString version;
    QList< QPair<QString, QSharedPointer<ZExpression>> > values;
    int lineNumber;
};

struct ParserToken
{
    Tokenizer::Token token;

    ParserToken()
    {
        startsAt = endsAt = 0;
        type = Text;
        reference = nullptr;
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

    ParserToken(Tokenizer::Token tok, TokenType type, QSharedPointer<ZTreeNode> ref = nullptr, QString refPath = "")
    {
        token = tok;
        startsAt = token.startsAt;
        endsAt = token.endsAt;
        this->type = type;
        reference = ref;
        referencePath = refPath;
    }

    int startsAt;
    int endsAt;
    TokenType type;
    QString referencePath;
    QSharedPointer<ZTreeNode> reference;
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
    void setTypeInformation(QList<QSharedPointer<ZTreeNode>> types);
    // parseClassFields and parseStructFields will parse fields and method signatures inside objects
    // (and substructs)
    bool parseClassFields(QSharedPointer<ZClass> cls) { return parseObjectFields(cls, cls); }
    bool parseStructFields(QSharedPointer<ZStruct> struc) { return parseObjectFields(nullptr, struc); }
    // parseClassMethods and parseStructMethods will parse method bodies (knowing all possible types and fields at this point)
    bool parseClassMethods(QSharedPointer<ZClass> cls) { return parseObjectMethods(cls, cls); }
    bool parseStructMethods(QSharedPointer<ZStruct> struc) { return parseObjectMethods(nullptr, struc); }

    // Parser operates at File level
    //
    QSharedPointer<ZFileRoot> root;
    QList<ParserToken> parsedTokens;

    void reportError(QString err);
    void reportWarning(QString warn);

    //
    static QSharedPointer<ZSystemType> resolveSystemType(QString name);

    QList<QSharedPointer<ZTreeNode>> getOwnTypeInformation();
    QList<QSharedPointer<ZTreeNode>> getTypeInformation();

private:
    QList<Tokenizer::Token> tokens;
    QList<QSharedPointer<ZTreeNode>> types;
    // System type info. Initialized once
    static QList<ZSystemType> systemTypes;

    bool skipWhitespace(TokenStream& stream, bool newline);
    bool consumeTokens(TokenStream& stream, QList<Tokenizer::Token>& out, quint64 stopAtAnyOf);

    QSharedPointer<ZExpression> parseExpression(TokenStream& stream, quint64 stopAtAnyOf);
    void dumpExpression(QSharedPointer<ZExpression> expr, int level);

    // these occur at the root scope
    bool parseRoot(TokenStream& stream);
    QSharedPointer<ZClass> parseClass(TokenStream& stream, bool extend);
    QSharedPointer<ZStruct> parseStruct(TokenStream& stream);
    QSharedPointer<ZEnum> parseEnum(TokenStream& stream);

    // this occurs in the class and struct body
    bool parseCompoundType(TokenStream& stream, ZCompoundType& type, QSharedPointer<ZStruct> context);
    bool parseObjectFields(QSharedPointer<ZClass> cls, QSharedPointer<ZStruct> struc);

    // this occurs in methods
    bool parseObjectMethods(QSharedPointer<ZClass> cls, QSharedPointer<ZStruct> struc);
    // parent = outer code block or loop/condition
    // context = nearest outer class
    QSharedPointer<ZCodeBlock> parseCodeBlock(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context);
    QSharedPointer<ZForCycle> parseForCycle(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context);
    QSharedPointer<ZCondition> parseCondition(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context);
    // magic
    void highlightExpression(QSharedPointer<ZExpression> expr, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context);

    enum
    {
        Stmt_Initializer = 0x0001, // int i = 1;
        Stmt_Expression = 0x0002, // i = 1;
        Stmt_Cycle = 0x0004, // for (int i = 1; ...)
        Stmt_CycleControl = 0x0008, // break;
        Stmt_Return = 0x0010, // return i;
        Stmt_Condition = 0x0020, // if (i == 666) or switch(i)

        // predefined
        Stmt_CycleInitializer = Stmt_Initializer|Stmt_Expression,
        Stmt_Function = Stmt_Initializer|Stmt_Expression|Stmt_Cycle|Stmt_Return|Stmt_Condition
    };
    QList<QSharedPointer<ZTreeNode>> parseStatement(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context, quint64 flags, quint64 stopAtAnyOf);
    QSharedPointer<ZCodeBlock> parseCodeBlockOrLine(TokenStream& stream, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context, QSharedPointer<ZTreeNode> recip);

    // helper
    QSharedPointer<ZTreeNode> resolveType(QString name, QSharedPointer<ZStruct> context = nullptr, bool onlycontext = false);
    QSharedPointer<ZTreeNode> resolveSymbol(QString name, QSharedPointer<ZTreeNode> parent, QSharedPointer<ZStruct> context);
};

#endif // PARSER_H
