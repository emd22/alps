#include "Parser.h"
#include "Lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

Node *ParseAssignment(Parser *pr, NodeVar *override_var);
Node *ParseVariable(Parser *pr);
NodeFuncDeclare *ParseFuncDeclaration(Parser *pr);
Node *ParseDeclaration(Parser *pr);
Node *ParseFactor(Parser *pr);
Node *ParseFuncCall(Parser *pr);

Parser ParserInit(Lexer lexer)
{
    Parser parser;
    parser.lexer = lexer;
    parser.token_index = 0;
    return parser;
}

static void ThrowError(Parser *pr, char *msg, ...)
{
    Token *token = &pr->lexer.tokens[pr->token_index];

    va_list ap;
    va_start(ap, msg);
    printf("[ERROR] [%d,%d]: ", token->file_line, token->file_col);
    vprintf(msg, ap);
    va_end(ap);

    exit(1);
}

void CheckExpect(Parser *pr, Token *token, TokenType type)
{
    if (token->type == type) {
        return;
    }

    // printf("[ERROR]: expected %s and found %s (%.*s)\n", LexerTokenTypeStr(type), LexerTokenTypeStr(token->type), (int)LexerTokenLength(token), token->start);
    ThrowError(pr, "Expected %s and found %s (%.*s)\n", LexerTokenTypeStr(type), LexerTokenTypeStr(token->type), TKPF(token));
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
    CheckExpect(parser, token, expect);
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

NodeFuncCall *NewFuncCall()
{
    NewN(NodeFuncCall, node);

    node->base.type = NT_FUNC_CALL;
    node->func = NULL;
    node->arguments = NULL;
    node->argument_count = 0;

    return node;
}


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
    else if (tk->type == TT_NUMBER || tk->type == TT_STRING) {
        EatRaw(pr);

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
        if (PeekToken(pr, 1)->type == TT_LPAREN) {
            return ParseFuncCall(pr);
        }
        return ParseVariable(pr);
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
    return NULL;
}

static char *LoadFile_(char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return NULL;

    fseek(fp, 0, SEEK_END);
    unsigned file_size = ftell(fp);
    rewind(fp);

    char *buffer = (char *)malloc(file_size);
    fread(buffer, 1, file_size, fp);
    fclose(fp);
    return buffer;
}

Node *ParseFuncCall(Parser *pr)
{
    NodeFuncCall *call = NewFuncCall();

    NodeVar *var = NewVar();
    var->value = Eat(pr, TT_IDENTIFIER);

    call->func = var;

    Eat(pr, TT_LPAREN);

    // arguments!
    if (CurrentToken(pr)->type != TT_RPAREN) {
        int arg_size = 8;

        call->arguments = malloc(sizeof(Node *) * arg_size);

        do {
            Node *arg = ParseExpr(pr);

            call->arguments[call->argument_count++] = (Node *)arg;

            if (call->argument_count > arg_size) {
                arg_size *= 2;
                call->arguments = realloc(call->arguments, sizeof(Node *) * arg_size);
            }
        } while (CurrentToken(pr)->type == TT_COMMA && Eat(pr, TT_COMMA));

        if (call->argument_count < arg_size) {
            call->arguments = realloc(call->arguments, sizeof(Node *) * call->argument_count);
        }
    }

    Eat(pr, TT_RPAREN);

    if (!strncmp(call->func->value->start, "include", LexerTokenLength(call->func->value))) {
        Lexer lexer;
        char path[256];
        Token *path_token = ((NodeLiteral *)call->arguments[0])->token;
        strncpy(path, path_token->start + 1, LexerTokenLength(path_token) - 2);
        char *data = LoadFile_(path);
        if (data == NULL) {
            ThrowError(pr, "Could not load '%s'!\n", path);
        }
        lexer = LexerLex(data, "+-*/=:;,.(){}", SFLEX_USE_STRINGS);


        Parser newpr = ParserInit(lexer);

        Node *ast = Parse(&newpr);
        return ast;
    }

    return (Node *)call;
}

Node *ParseStatement(Parser *pr)
{
    Token *token = CurrentToken(pr);
    Node *node = NULL;

    // when a variable is newly declared and assigned to in the same line, we set this variable
    // to the NodeVar of the variable. The assign will be picked up as a new statement without
    // extra jank to find the variable name. (this is still jank)
    static NodeVar *newly_declared_var = NULL;

    if (token->type == TT_LBRACE) {
        return ParseBlock(pr);
    }
    else if (token->type == TT_IDENTIFIER) {
        Node *vdecl =  ParseDeclaration(pr);

        if (vdecl != NULL) {
            // let the statement go through, require a semicolon
            node = vdecl;

            // we have an assignment same line as our declaration
            if (CurrentToken(pr)->type == TT_EQUALS) {
                // end the statement, the next read will pick up the
                // 'x = [value]' statement
                newly_declared_var = (NodeVar *)((NodeDeclare *)vdecl)->variable;

                // return early as we finished our first statement. The next will be the
                // caught by the assign, and that will finish our line.
                return node;
            }
        }
        else if (PeekToken(pr, 1)->type == TT_LPAREN) {
            node = ParseFuncCall(pr);
        }
        else {
            node = ParseAssignment(pr, NULL);
        }
    }
    // this is for inline variable declaration and then assignment!
    else if (token->type == TT_EQUALS && newly_declared_var) {
        node = ParseAssignment(pr, newly_declared_var);
        newly_declared_var = NULL;
    }

    else if (token->type == TT_KEYWORD) {
        // keep this out of the ParseKeyword function so
        // we can break out early.
        NodeFuncDeclare *fdecl = ParseFuncDeclaration(pr);
        if (fdecl) {
            return (Node *)fdecl;
        }

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
        // printf("[ERROR]: Unknown statement in block!\n");
        ThrowError(pr, "Unknown statement (%.*s) in block!\n", TKPF(token));
        exit(1);
    }

    Eat(pr, TT_SEMICOLON);

    return node;
}

static void AssertReturnStatement_(Parser *pr, NodeFuncDeclare *fdecl)
{
    int i;
    for (i = 0; i < fdecl->block->statement_count; i++) {
        if (fdecl->block->statements[i]->type == NT_RETURN) {
            break;
        }
    }
    if (i >= fdecl->block->statement_count) {
        // printf("[ERROR]: No return statement in function!\n");
        ThrowError(pr, "No return statement in function!\n");
        exit(1);
    }
}

static void ParseArgumentsDeclaration_(Parser *pr, NodeFuncDeclare *fdecl)
{
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
}

NodeFuncDeclare *ParseFuncDeclaration(Parser *pr)
{
    Token *ctok = CurrentToken(pr);

    if (ctok->type != TT_KEYWORD || strncmp(ctok->start, "fn", LexerTokenLength(ctok))) {
        return NULL;
    }

    Eat(pr, TT_KEYWORD);

    NodeDeclare *declare = NewDeclare();
    declare->variable = ParseVariable(pr);

    NodeFuncDeclare *fdecl = NewFuncDeclare();
    fdecl->declaration = declare;

    Eat(pr, TT_LPAREN);
    ParseArgumentsDeclaration_(pr, fdecl);
    Eat(pr, TT_RPAREN);

    // after arguments, parse type
    declare->type = Eat(pr, TT_TYPE);

    // start of function definition
    if (CurrentToken(pr)->type == TT_LBRACE) {
        fdecl->block = (NodeBlock *)ParseBlock(pr);
    }

    AssertReturnStatement_(pr, fdecl);

    return fdecl;
}

Node *ParseDeclaration(Parser *pr)
{
    // NodeFuncDeclare *fdecl = ParseFuncDeclaration(pr);
    // if (fdecl) {
    //     return (Node *)fdecl;
    // }

    if (CurrentToken(pr)->type != TT_IDENTIFIER || PeekToken(pr, 1)->type != TT_TYPE) {
        return NULL;
    }

    Node *vdecl = ParseVariable(pr);
    if (vdecl == NULL) {
        return NULL;
    }

    NodeDeclare *declare = NewDeclare();
    declare->variable = vdecl;
    declare->type = Eat(pr, TT_TYPE);

    return (Node *)declare;
}

Node *ParseAssignment(Parser *pr, NodeVar *override_var)
{
    NodeAssign *assign = NewAssign();

    assign->left = override_var ? (Node *)override_var : ParseVariable(pr);
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
    else if (ast->type == NT_FUNC_CALL) {
        NodeFuncCall *call = (NodeFuncCall *)ast;

        printf("FUNCCALL %.*s\n", TKPF(call->func->value));
        int i;
        for (i = 0; i < call->argument_count; i++) {
            ParserPrintAST((Node *)call->arguments[i], indent + 1);
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
