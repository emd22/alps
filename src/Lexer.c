#include "Lexer.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <ctype.h>

#define TOKEN_BUFFER_START 256

// #define SFLEX_IS_WHITESPACE(ch) (ch == ' ' || ch == '\t' || ch == '\n')
#define SFLEX_IS_WHITESPACE(inst, ch) LexerIsWhitespace(inst, (ch))
#define SFLEX_IS_STRING(ch, flags) ((flags & SFLEX_USE_STRINGS) && (ch == '"' || ch == '\''))
#define SFLEX_IS_SPECIAL(buf, ch) (strchr(buf, ch) != NULL)

static int LexGetFileColumn(Lexer *inst)
{
    return (int)(inst->newb - inst->_line_start_ptr);
}

static LexerToken *LexTokenGetNext(Lexer *inst) {
    if (inst->token_amt+1 > inst->token_buffer_size) {
        inst->token_buffer_size *= 2;
        inst->tokens = (LexerToken *)realloc(inst->tokens, inst->token_buffer_size);
    }

    LexerToken *token = &inst->tokens[inst->token_amt++];
    token->file_line = inst->current_line + 1;
    token->file_col = LexGetFileColumn(inst);

    return token;
}

const char *LexerTokenTypeStr(TokenType type) {
    switch (type) {
        case TT_NONE:
            return "none";
        case TT_IDENTIFIER:
            return "Identifier";
        case TT_STRING:
            return "String";
        case TT_LPAREN:
            return "LParen";
        case TT_RPAREN:
            return "RParen";
        case TT_NUMBER:
            return "Number";
        case TT_SEMICOLON:
            return "Semicolon";
        case TT_COLON:
            return "Colon";
        case TT_PERIOD:
            return "Period";
        case TT_COMMA:
            return "Comma";
        case TT_LBRACE:
            return "LBrace";
        case TT_RBRACE:
            return "RBrace";
        case TT_EQUALS:
            return "Equals";
        case TT_KEYWORD:
            return "Keyword";
        case TT_TYPE:
            return "Type";

        case TT_PLUS:
            return "Plus";
        case TT_MINUS:
            return "Minus";
        case TT_STAR:
            return "Star";
        case TT_SLASH:
            return "Slash";

        default:
            return "Unknown";
    }
    return "Unknown";
}

bool LexerApplySingleCharType(LexerToken *token)
{
    char ch = token->start[0];
    switch (ch) {
    case ';':
        token->type = TT_SEMICOLON;
        break;
    case ':':
        token->type = TT_COLON;
        break;
    case ',':
        token->type = TT_COMMA;
        break;
    case '.':
        token->type = TT_PERIOD;
        break;
    case '{':
        token->type = TT_LBRACE;
        break;
    case '}':
        token->type = TT_RBRACE;
        break;
    case '(':
        token->type = TT_LPAREN;
        break;
    case ')':
        token->type = TT_RPAREN;
        break;
    case '=':
        token->type = TT_EQUALS;
        break;
    case '+':
        token->type = TT_PLUS;
        break;
    case '-':
        token->type = TT_MINUS;
        break;
    case '*':
        token->type = TT_STAR;
        break;
    case '/':
        token->type = TT_SLASH;
        break;
    default:
        return false;
    }
    return true;
}


static void ThrowError(Lexer *inst, const char *msg)
{
    printf("[ERROR] [line %d]: ", inst->current_line);
    printf("%s", msg);
    exit(1);
}

bool IfIsKeyword(LexerToken *token, const char *keywords[], int keyword_count)
{
    int i;
    for (i = 0; i < keyword_count; i++) {
        if (LexerTokenLength(token) == strlen(keywords[i]) && !strncmp(token->start, keywords[i], LexerTokenLength(token))) {
            return true;
        }
    }
    return false;
}

void LexerCheckKeywords(LexerToken *token)
{
    const char *keywords[] = {
        "if", "return", "for", "while",
        "struct",
    };

    if (IfIsKeyword(token, keywords, 5)) {
        token->type = TT_KEYWORD;
    }

    const char *types[] = {
        "int", "str"
    };

    if (IfIsKeyword(token, types, 2)) {
        token->type = TT_TYPE;
    }
}

void LexerSetType(Lexer *inst, LexerToken *token)
{
    int length = LexerTokenLength(token);

    token->type = TT_NONE;

    bool decimal_found = false;

    // parse numbers
    int i;
    for (i = 0; i < length; i++) {
        if (isnumber(token->start[i])) {
            token->type = TT_NUMBER;
        }
        else if (token->start[i] == '.') {
            if (decimal_found) {
                ThrowError(inst, "Invalid number format!\n");
            }
            token->type = TT_NUMBER;
            decimal_found = true;
        }
        else {
            token->type = TT_IDENTIFIER;
            break;
        }
    }


    if (token->type == TT_NUMBER) {
        return;
    }

    if (token->start[0] == '"' || token->start[0] == '\'') {
        token->type = TT_STRING;
        return;
    }

    // parse single character tokens
    if (length == 1 && LexerApplySingleCharType(token)) {
        return;
    }

    LexerCheckKeywords(token);

    if (token->type == TT_NONE && token->end - token->start > 0 && isalpha(token->start[0])) {
        token->type = TT_IDENTIFIER;
    }
}

long LexerTokenLength(LexerToken *token)
{
    return token->end - token->start;
}

void LexerDestroy(Lexer *inst) {
    if (inst == NULL)
        return;

    if (inst->tokens != NULL)
        free(inst->tokens);

    inst->token_amt = 0;
    inst->token_buffer_size = 0;
}

bool LexerIsWhitespace(Lexer *inst, char ch)
{
    if (ch == '\n') {
        inst->current_line++;
        inst->_line_start_ptr = inst->newb;
    }
    return (ch == ' ' || ch == '\t' || ch == '\n');
}

Lexer LexerLex(char *data, const char *specials, int flags) {
    char ch, lastch;
    // Setup sflex structure
    Lexer inst;

    inst.token_buffer_size = TOKEN_BUFFER_START;
    inst.token_amt = 0;
    inst.tokens = (LexerToken *)malloc(sizeof(LexerToken ) * inst.token_buffer_size);
    inst.data = data;

    inst.current_line = 0;
    inst._line_start_ptr = data;

    bool in_string = false;

    // skip initial whitespace
    while (SFLEX_IS_WHITESPACE(&inst, (ch = (*data)))) {
        data++;
    }

    // char *newb = data;
    inst.newb = data;
    LexerToken *token = NULL;

    while ((ch = *(inst.newb))) {
        // lex comments
        if (ch == '/' && *(inst.newb + 1) == '/') {
            // skip until our next newline or until the end of file
            while ((ch = *(++inst.newb)) && ch != '\n' && ch != EOF);

            // sklp any whitespace after, until the next statement
            while (SFLEX_IS_WHITESPACE(&inst, *(++inst.newb)));
            // if we have a current token, set it to the new start
            if (token) {
                token->start = inst.newb;
            }

            continue;
        }

        if (SFLEX_IS_STRING(ch, flags))
            in_string = !in_string;

        // first character in string
        if (inst.newb == data) {
            // token is not initialized, create new one
            token = LexTokenGetNext(&inst);
            //
            if (!SFLEX_IS_WHITESPACE(&inst, ch)) {
                token->start = inst.newb;
            }
        }

        // handle whitespace
        if ((SFLEX_IS_WHITESPACE(&inst, ch)) && (!in_string)) {
            if (token != NULL)
                token->end = inst.newb;

            while (SFLEX_IS_WHITESPACE(&inst, *(inst.newb+1)))
                inst.newb++;

            LexerSetType(&inst, token);
            token = LexTokenGetNext(&inst);
            token->start = inst.newb + 1;
        }
        // handle 'special' characters
        else {
            if (specials != NULL && SFLEX_IS_SPECIAL(specials, ch) && !in_string) {
                if (!SFLEX_IS_WHITESPACE(&inst, lastch) && !SFLEX_IS_SPECIAL(specials, lastch) && (token != NULL)) {
                    token->end = inst.newb;

                    LexerSetType(&inst, token);
                    token = LexTokenGetNext(&inst);
                }
                token->start = inst.newb;
                token->end = inst.newb+1;

                LexerSetType(&inst, token);
                token = LexTokenGetNext(&inst);

                while (SFLEX_IS_WHITESPACE(&inst, *(inst.newb + 1)))
                    inst.newb++;

                token->start = inst.newb + 1;
            }
        }
        lastch = ch;
        inst.newb++;
    }

    // chop off empty token at end
    inst.token_amt--;
    return inst;
}
