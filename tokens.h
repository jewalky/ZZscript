// DEFINE_TOKEN(name[, string])

DEFINE_TOKEN1(0, Identifier)    // meow
DEFINE_TOKEN1(1, Integer)       // -666
DEFINE_TOKEN1(2, Double)        // 1.5
DEFINE_TOKEN1(3, String)        // "..."
DEFINE_TOKEN1(4, Name)          // '...'

DEFINE_TOKEN1(5, LineComment)   // // blablabla
DEFINE_TOKEN1(6, BlockComment)  // /* blablabla */
DEFINE_TOKEN1(7, Whitespace)    // newline, tab, space

DEFINE_TOKEN1(8, Invalid)

DEFINE_TOKEN2(9, Preprocessor, "#")
DEFINE_TOKEN2(10, Newline, "\n")

DEFINE_TOKEN2(11, OpenCurly, "{")
DEFINE_TOKEN2(12, CloseCurly, "}")

DEFINE_TOKEN2(13, OpenParen, "(")
DEFINE_TOKEN2(14, CloseParen, ")")

DEFINE_TOKEN2(15, OpenSquare, "[")
DEFINE_TOKEN2(16, CloseSquare, "]")

DEFINE_TOKEN2(17, Dot, ".")
DEFINE_TOKEN2(18, Comma, ",")

// operators
DEFINE_TOKEN2(19, OpEquals, "==")
DEFINE_TOKEN2(20, OpSomewhatEquals, "~==")
DEFINE_TOKEN2(21, OpNotEquals, "!=")
DEFINE_TOKEN2(22, OpLessThan, "<")
DEFINE_TOKEN2(23, OpGreaterThan, ">")
DEFINE_TOKEN2(24, OpLessOrEqual, "<=")
DEFINE_TOKEN2(25, OpGreaterOrEqual, ">=")
DEFINE_TOKEN2(26, Questionmark, "?")
DEFINE_TOKEN2(27, Colon, ":")
DEFINE_TOKEN2(28, DoubleColon, "::")
DEFINE_TOKEN2(29, OpAdd, "+")
DEFINE_TOKEN2(30, OpSubtract, "-")
DEFINE_TOKEN2(31, OpMultiply, "*")
DEFINE_TOKEN2(32, OpDivide, "/")
DEFINE_TOKEN2(33, OpLeftShift, "<<")
DEFINE_TOKEN2(34, OpRightShift, ">>")
DEFINE_TOKEN2(35, OpNegate, "~")
DEFINE_TOKEN2(36, OpXor, "^")
DEFINE_TOKEN2(37, OpAnd, "&")
DEFINE_TOKEN2(38, OpOr, "|")
DEFINE_TOKEN2(39, OpStringConcat, "..")
DEFINE_TOKEN2(40, OpUnaryNot, "!")

DEFINE_TOKEN2(41, Semicolon, ";")

DEFINE_TOKEN2(42, OpAssign, "=")
DEFINE_TOKEN2(43, OpModulo, "%")

DEFINE_TOKEN2(44, OpLogicalAnd, "&&")
DEFINE_TOKEN2(45, OpLogicalOr, "||")

DEFINE_TOKEN2(46, OpSpaceship, "<>=") // apparently not classic <=>

DEFINE_TOKEN2(47, Ellipsis, "...")
