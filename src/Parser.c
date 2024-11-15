#include "Parser.h"
#include "Lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



Parser ParserInit(Lexer lexer)
{
    Parser parser;
    parser.lexer = lexer;
    parser.token_index = 0;
    return parser;
}

static void ThrowError(char *msg)
{
    printf("[ERROR]: %s\n", msg);
    exit(1);
}

void CheckExpect(Token *token, TokenType type)
{
    if (token->type == type) {
        return;
    }

    printf("[ERROR]: expected %s and found %s (%.*s)\n", LexerTokenTypeStr(type), LexerTokenTypeStr(token->type), (int)LexerTokenLength(token), token->start);

    exit(1);
}

Token *EatRaw(Parser *parser)
{
    Token *token = &parser->lexer.tokens[parser->token_index++];
    return token;
}

Token *Eat(Parser *parser, TokenType expect)
{
    Token *token = &parser->lexer.tokens[parser->token_index++];
    printf("Eating %s\n", LexerTokenTypeStr(token->type));
    CheckExpect(token, expect);
    return token;
}

Token *CurrentToken(Parser *parser)
{
    return &parser->lexer.tokens[parser->token_index];
}

Token *PeekToken(Parser *parser, int peek)
{
    return &parser->lexer.tokens[parser->token_index + peek];
}

// node creation functions

#define NewN(ntype, name) ntype *name = (ntype *)malloc(sizeof(ntype))

NodeBinOp *NewBinOp()
{
    NewN(NodeBinOp, node);

    node->base.type = NT_BINOP;
    node->left = NULL;
    node->op = NULL;
    node->right = NULL;

    return node;
}

NodeLiteral *NewLiteral()
{
    NewN(NodeLiteral, node);

    node->base.type = NT_LITERAL;
    node->token = NULL;

    return node;
}

NodeUnaryOp *NewUnaryOp()
{
    NewN(NodeUnaryOp, node);

    node->base.type = NT_UNARYOP;
    node->node = NULL;
    node->op = NULL;

    return node;
}

NodeBlock *NewBlock()
{
    NewN(NodeBlock, node);

    node->base.type = NT_BLOCK;
    node->statement_buf_size = 64;
    node->statements = malloc(sizeof(Node *) * node->statement_buf_size);
    node->statement_count = 0;

    return node;
}

NodeAssign *NewAssign()
{
    NewN(NodeAssign, node);

    node->base.type = NT_ASSIGN;
    node->left = NULL;
    node->right = NULL;

    return node;
}

NodeVar *NewVar()
{
    NewN(NodeVar, node);

    node->base.type = NT_VAR;
    node->value = NULL;

    return node;
}

NodeDeclare *NewDeclare()
{
    NewN(NodeDeclare, node);

    node->base.type = NT_DECLARE;
    node->type = NULL;
    node->variable = NULL;

    return node;
}

NodeFuncDeclare *NewFuncDeclare()
{
    NewN(NodeFuncDeclare, node);

    node->base.type = NT_FUNC_DECLARE;
    node->declaration = NULL;
    node->arguments = NULL;
    node->argument_count = 0;
    node->block = NULL;

    return node;
}

NodeReturn *NewReturn()
{
    NewN(NodeReturn, node);

    node->base.type = NT_RETURN;
    node->value = NULL;

    return node;
}



Node *ParseAssignment(Parser *pr);
Node *ParseVariable(Parser *pr);
Node *ParseDeclaration(Parser *pr);


Node *ParseFactor(Parser *pr);

Node *ParseTerm(Parser *pr)
{
    Node *node = ParseFactor(pr);

    Token *ctok;

    while ((ctok = CurrentToken(pr)) && (ctok->type == TT_STAR || ctok->type == TT_SLASH)) {
        if (ctok->type == TT_STAR) {
            Eat(pr, TT_STAR);
        }
        else if (ctok->type == TT_SLASH) {
            Eat(pr, TT_SLASH);
        }

        NodeBinOp *dir_node = NewBinOp();
        dir_node->left = node;
        dir_node->op = ctok;
        dir_node->right = ParseFactor(pr);
        node = (Node *)dir_node;
    }

    return node;
}

Node *ParseExpr(Parser *pr)
{
    Node *node = ParseTerm(pr);

    Token *ctok;

    while ((ctok = CurrentToken(pr)) && (ctok->type == TT_PLUS || ctok->type == TT_MINUS)) {
        if (ctok->type == TT_PLUS) {
            Eat(pr, TT_PLUS);
        }
        else if (ctok->type == TT_MINUS) {
            Eat(pr, TT_MINUS);
        }

        NodeBinOp *dir_node = NewBinOp();

        dir_node->left = node;
        dir_node->op = ctok;
        dir_node->right = ParseTerm(pr);

        node = (Node *)dir_node;
    }

    return node;
}

Node *ParseFactor(Parser *pr)
{
    Token *tk = CurrentToken(pr);

    if (tk->type == TT_PLUS || tk->type == TT_MINUS) {
        EatRaw(pr);

        NodeUnaryOp *node = NewUnaryOp();
        node->op = tk;
        node->node = ParseFactor(pr);
        return (Node *)node;
    }
    else if (tk->type == TT_NUMBER) {
        Eat(pr, TT_NUMBER);

        NodeLiteral *node = NewLiteral();
        node->token = tk;

        return (Node *)node;
    }
    else if (tk->type == TT_LPAREN) {
        Eat(pr, TT_LPAREN);
        Node *node = ParseExpr(pr);
        Eat(pr, TT_RPAREN);
        return node;
    }
    else {
        Node *node = ParseVariable(pr);
        return node;
    }
    return NULL;
}



Node *ParseBlock(Parser *pr);

Node *ParseReturn(Parser *pr)
{
    NodeReturn *ret = NewReturn();

    Eat(pr, TT_KEYWORD);

    ret->value = ParseExpr(pr);

    return (Node *)ret;
}

Node *ParseKeyword(Parser *pr)
{
    Token *token = CurrentToken(pr);
    if (!strncmp(token->start, "return", LexerTokenLength(token))) {
        return ParseReturn(pr);
    }
}

Node *ParseStatement(Parser *pr)
{
    Token *token = CurrentToken(pr);
    Node *node = NULL;

    if (token->type == TT_TYPE) {
        node = ParseDeclaration(pr);

        // we declared a function with a block, skip semicolon
        if (node->type == NT_FUNC_DECLARE && ((NodeFuncDeclare *)node)->block) {
            return node;
        }

        // we have an assignment same line as our declaration
        if (CurrentToken(pr)->type == TT_EQUALS) {
            // roll back the token to our identifier
            pr->token_index--;

            // end the statement, the next read will pick up the
            // 'x = [value]' statement
            return node;
        }
    }
    else if (token->type == TT_LBRACE) {
        return ParseBlock(pr);
    }
    else if (token->type == TT_IDENTIFIER) {
        node = ParseAssignment(pr);
    }
    else if (token->type == TT_KEYWORD) {
        node = ParseKeyword(pr);
    }
    else if (token->type == TT_NONE || token->type == TT_RBRACE) {
        return NULL;
    }
    else if (token->type == TT_SEMICOLON) {
        Eat(pr, TT_SEMICOLON);
        return node;
    }
    else {
        printf("[ERROR]: Unknown statement in block!\n");
        exit(1);
    }

    Eat(pr, TT_SEMICOLON);

    return node;
}

Node *ParseDeclaration(Parser *pr)
{
    NodeDeclare *declare = NewDeclare();
    declare->type = Eat(pr, TT_TYPE);
    declare->variable = ParseVariable(pr);

    Token *ctok = CurrentToken(pr);
    // if our next token is a left parenthesis, swtich to a function declaration
    if (ctok->type == TT_LPAREN) {
        NodeFuncDeclare *fdecl = NewFuncDeclare();
        fdecl->declaration = declare;

        Eat(pr, TT_LPAREN);

        // arguments!
        if (CurrentToken(pr)->type != TT_RPAREN) {
            int arg_size = 8;

            fdecl->arguments = malloc(sizeof(Node *) * arg_size);

            do {
                Node *arg = ParseDeclaration(pr);

                fdecl->arguments[fdecl->argument_count++] = (NodeDeclare *)arg;

                if (fdecl->argument_count > arg_size) {
                    arg_size *= 2;
                    fdecl->arguments = realloc(fdecl->arguments, sizeof(Node *) * arg_size);
                }
            } while (CurrentToken(pr)->type == TT_COMMA && Eat(pr, TT_COMMA));

            if (fdecl->argument_count < arg_size) {
                fdecl->arguments = realloc(fdecl->arguments, sizeof(Node *) * fdecl->argument_count);
            }
        }

        Eat(pr, TT_RPAREN);

        // start of function definition
        if (CurrentToken(pr)->type == TT_LBRACE) {
            fdecl->block = (NodeBlock *)ParseBlock(pr);
        }

        return (Node *)fdecl;
    }


    return (Node *)declare;
}

Node *ParseAssignment(Parser *pr)
{
    NodeAssign *assign = NewAssign();


    assign->left = ParseVariable(pr);
    assign->op = Eat(pr, TT_EQUALS);
    assign->right = ParseExpr(pr);

    return (Node *)assign;
}

Node *ParseVariable(Parser *pr)
{
    NodeVar *var = NewVar();
    var->value = Eat(pr, TT_IDENTIFIER);
    return (Node *)var;
}


NodeBlock *ParseStatementList(Parser *pr)
{
    NodeBlock *block = NewBlock();

    Node *statement;

    while ((statement = ParseStatement(pr))) {
        block->statements[block->statement_count++] = statement;

        // resize the amount of statements
        if (block->statement_count > block->statement_buf_size) {
            block->statement_buf_size *= 2;
            block->statements = realloc(block->statements, sizeof(Node *) * block->statement_buf_size);
        }
    }

    // fix overshoot, large codebases can balloon this significantly
    if (block->statement_buf_size > block->statement_count) {
        block->statement_buf_size = block->statement_count;
        block->statements = realloc(block->statements, sizeof(Node *) * block->statement_buf_size);
    }

    return block;
}



Node *ParseBlock(Parser *pr)
{
    Eat(pr, TT_LBRACE);
    NodeBlock *block = ParseStatementList(pr);
    Eat(pr, TT_RBRACE);
    return (Node *)block;
}

Node *ParseProgram(Parser *pr)
{
    Node *node = (Node *)ParseStatementList(pr);
    return node;
}

Node *Parse(Parser *pr)
{
    return ParseProgram(pr);
}

void ParserPrintAST(Node *ast, int indent)
{
    int i;
    for (i = 0; i < indent; i++) {
        printf("    ");
    }

    if (ast->type == NT_LITERAL) {
        NodeLiteral *lit = (NodeLiteral *)ast;
        printf("LITERAL (%.*s)\n", (int)LexerTokenLength(lit->token), lit->token->start);
    }
    else if (ast->type == NT_UNARYOP) {
        NodeUnaryOp *unary = (NodeUnaryOp *)ast;
        printf("UNARYOP %.*s\n", (int)LexerTokenLength(unary->op), unary->op->start);
        ParserPrintAST(unary->node, indent + 1);
    }
    else if (ast->type == NT_BINOP) {
        NodeBinOp *binop = (NodeBinOp *)ast;
        printf("BINOP %.*s\n", (int)LexerTokenLength(binop->op), binop->op->start);
        ParserPrintAST(binop->left, indent + 1);
        ParserPrintAST(binop->right, indent + 1);
    }
    else if (ast->type == NT_BLOCK) {
        printf("BLOCK\n");
        NodeBlock *block = (NodeBlock *)ast;

        for (i = 0; i < block->statement_count; i++) {
            ParserPrintAST(block->statements[i], indent + 1);
        }
    }
    else if (ast->type == NT_ASSIGN) {
        NodeAssign *assign = (NodeAssign *)ast;
        printf("ASSIGN %.*s\n", (int)LexerTokenLength(assign->op), assign->op->start);
        ParserPrintAST(assign->left, indent + 1);
        ParserPrintAST(assign->right, indent + 1);
    }
    else if (ast->type == NT_DECLARE) {
        NodeDeclare *declare = (NodeDeclare *)ast;
        printf("DECLARE %.*s\n", (int)LexerTokenLength(declare->type), declare->type->start);
        ParserPrintAST(declare->variable, indent + 1);
    }
    else if (ast->type == NT_VAR) {
        NodeVar *var = (NodeVar *)ast;
        printf("VARIABLE %.*s\n", (int)LexerTokenLength(var->value), var->value->start);
    }
    else if (ast->type == NT_FUNC_DECLARE) {
        NodeFuncDeclare *fdecl = (NodeFuncDeclare *)ast;

        Token *name = ((NodeVar *)fdecl->declaration->variable)->value;
        Token *type = fdecl->declaration->type;
        printf("FUNCDECL %.*s -> %.*s\n", (int)LexerTokenLength(name), name->start, (int)LexerTokenLength(type), type->start);

        int i;
        for (i = 0; i < fdecl->argument_count; i++) {
            ParserPrintAST((Node *)fdecl->arguments[i], indent + 1);
        }

        if (fdecl->block) {
            ParserPrintAST((Node *)fdecl->block, indent + 1);
        }
    }
    else if (ast->type == NT_RETURN) {
        NodeReturn *ret = (NodeReturn *)ast;

        printf("RETURN\n");
        ParserPrintAST(ret->value, indent + 1);
    }
    else {
        printf("UNKNOWN %d\n", ast->type);
    }
}
