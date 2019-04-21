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

struct ZExpression;
struct ZStruct;
struct ZSystemType : public ZTreeNode
{
    ZSystemType(ZTreeNode* p) : ZTreeNode(p) {}

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
    ZTreeNode* reference;
    QList<ZCompoundType> arguments; // example: Array<Actor>
    QList<ZExpression*> arrayDimensions; // example: string s[8]; or string s[SIZE];

    ZCompoundType()
    {
        reference = nullptr;
    }

    void destroy()
    {
        for (ZExpression* expr : arrayDimensions)
            delete expr;
        arrayDimensions.clear();
        for (ZCompoundType& t : arguments)
            t.destroy();
    }

    bool isSystem()
    {
        return (reference && reference->type() == ZTreeNode::SystemType);
    }
};

struct ZField : public ZTreeNode
{
    ZField(ZTreeNode* p) : ZTreeNode(p) {}
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

struct ZLocalVariable : public ZTreeNode
{
    ZLocalVariable(ZTreeNode* p) : ZTreeNode(p) {}
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

struct ZCodeBlock : public ZTreeNode
{
    ZCodeBlock(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return CodeBlock; }

    // code block holds ZLocalVariables, ZExpressions, and various cycles (each of them also has a code block)
};

struct ZExecutionControl : public ZTreeNode
{
    ZExecutionControl(ZTreeNode* p) : ZTreeNode(p) {}
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

struct ZForCycle : public ZTreeNode
{
    ZForCycle(ZTreeNode* p) : ZTreeNode(p)
    {
        condition = nullptr;
    }

    virtual NodeType type() { return ForCycle; }

    //
    QList<ZTreeNode*> initializers;
    ZExpression* condition;
    QList<ZExpression*> step;

    virtual ~ZForCycle();

    // children = code block
};

struct ZCondition : public ZTreeNode
{
    ZCondition(ZTreeNode* p) : ZTreeNode(p)
    {
        condition = nullptr;
        elseBlock = nullptr;
    }

    virtual NodeType type() { return Condition; }

    //
    ZExpression* condition;

    //
    ZTreeNode* elseBlock;

    virtual ~ZCondition();
};

struct ZConstant : public ZTreeNode
{
    ZConstant(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Constant; }

    // children = expression
    int lineNumber;
};

struct ZProperty : public ZTreeNode
{
    ZProperty(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Property; }

    // identifier = prop name
    // property <name> : <field1> [, <field2> ... ]
    QList<QString> fields;
    int lineNumber;
};

struct ZMethod : public ZTreeNode
{
    ZMethod(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Method; }

    virtual ~ZMethod()
    {
        for (ZCompoundType& rt : returnTypes)
            rt.destroy();
        for (ZLocalVariable* arg : arguments)
            delete arg;
    }

    QList<ZCompoundType> returnTypes;
    QList<ZLocalVariable*> arguments;
    QList<QString> flags;
    QString version;
    QString deprecated;
    bool hasEllipsis;
    int lineNumber;

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
    int lineNumber;

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
        Expression,
        Token // this is only for parsing
    } type;

    Tokenizer::Token token;
    ZExpression* expr;

    ZExpressionLeaf() { expr = nullptr; };
    explicit ZExpressionLeaf(const Tokenizer::Token& t) : token(t) { expr = nullptr; }
};

struct ZExpression : public ZTreeNode
{
    ZExpression(ZTreeNode* p) : ZTreeNode(p)
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

struct ZEnum : public ZTreeNode
{
    ZEnum(ZTreeNode* p) : ZTreeNode(p) {}
    virtual NodeType type() { return Enum; }

    QString version;
    QList< QPair<QString, ZExpression*> > values;
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

    ParserToken(Tokenizer::Token tok, TokenType type, ZTreeNode* ref = nullptr, QString refPath = "")
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
    ZTreeNode* reference;
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
    void setTypeInformation(QList<ZTreeNode*> types);
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

    //
    static ZSystemType* resolveSystemType(QString name);

private:
    QList<Tokenizer::Token> tokens;
    QList<ZTreeNode*> types;
    // System type info. Initialized once
    static QList<ZSystemType> systemTypes;

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
    bool parseCompoundType(TokenStream& stream, ZCompoundType& type, ZStruct* context);
    bool parseObjectFields(ZClass* cls, ZStruct* struc);

    // this occurs in methods
    bool parseObjectMethods(ZClass* cls, ZStruct* struc);
    // parent = outer code block or loop/condition
    // context = nearest outer class
    ZCodeBlock* parseCodeBlock(TokenStream& stream, ZTreeNode* parent, ZStruct* context);
    ZForCycle* parseForCycle(TokenStream& stream, ZTreeNode* parent, ZStruct* context);
    ZCondition* parseCondition(TokenStream& stream, ZTreeNode* parent, ZStruct* context);
    // magic
    void highlightExpression(ZExpression* expr, ZTreeNode* parent, ZStruct* context);

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
    QList<ZTreeNode*> parseStatement(TokenStream& stream, ZTreeNode* parent, ZStruct* context, quint64 flags, quint64 stopAtAnyOf);
    ZCodeBlock* parseCodeBlockOrLine(TokenStream& stream, ZTreeNode* parent, ZStruct* context, ZTreeNode* recip);

    // helper
    ZTreeNode* resolveType(QString name, ZStruct* context = nullptr, bool onlycontext = false);
    ZTreeNode* resolveSymbol(QString name, ZTreeNode* parent, ZStruct* context);
};

#endif // PARSER_H
