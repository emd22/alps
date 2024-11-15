#ifndef CML_PARSER_H
#define CML_PARSER_H

#include "Lexer.h"

typedef struct {
    Lexer lexer;
    int token_index;
} Parser;

typedef LexerToken Token;
// typedef void Node;

typedef enum {
    NT_LITERAL,
    NT_BINOP,
    NT_UNARYOP,
    NT_BLOCK,
    NT_ASSIGN,
    NT_VAR,
    NT_DECLARE,
    NT_FUNC_DECLARE,
    NT_RETURN,
} NodeType;

typedef struct {
    NodeType type;
} Node;

typedef struct {
    Node base;

    Node *left;
    Token *op;
    Node *right;
} NodeBinOp;

typedef struct {
    Node base;

    Token *op;
    Node *node;
} NodeUnaryOp;

typedef struct {
    Node base;

    Token *token;
} NodeLiteral;



typedef struct {
    Node base;

    Node **statements;
    int statement_count;
    int statement_buf_size;
} NodeBlock;

typedef struct {
    Node base;

    Node *left;
    Token *op;
    Node *right;
} NodeAssign;

typedef struct {
    Node base;

    Token *type;
    Node *variable;
} NodeDeclare;

typedef struct {
    Node base;

    NodeDeclare *declaration;

    NodeDeclare **arguments;
    int argument_count;

    NodeBlock *block;
} NodeFuncDeclare;

typedef struct {
    Node base;

    Token *value;
} NodeVar;

typedef struct {
    Node base;

    Node *value;
} NodeReturn;

Parser ParserInit(Lexer lexer);
Node *Parse(Parser *pr);
void ParserPrintAST(Node *ast, int indent);

#endif
