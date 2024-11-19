#ifndef SFLEXH_H
#define SFLEXH_H

#define SFLEX_USE_STRINGS 0x01

#define LexToken(token) (token.start)

// small helper when using strncmp and printf, due to the way tokens are stored.
#define TKPF(tk_) (int)LexerTokenLength((tk_)), (tk_)->start

typedef enum {
    TT_NONE,
    TT_IDENTIFIER,
    TT_STRING,
    TT_LPAREN,
    TT_RPAREN,
    TT_NUMBER,
    TT_SEMICOLON,
    TT_COLON,
    TT_PERIOD,
    TT_COMMA,
    TT_LBRACE,
    TT_RBRACE,
    TT_EQUALS,
    TT_KEYWORD,
    TT_TYPE,

    TT_PLUS,
    TT_MINUS,
    TT_STAR,
    TT_SLASH,

} TokenType;

typedef struct {
    char *start;
    char *end;

    int file_line;
    int file_col;

    TokenType type;
} LexerToken;

typedef struct {
    LexerToken *tokens;
    int token_buffer_size;
    int token_amt;

    int current_line;
    char *_line_start_ptr;

    char *newb;
    char *data;
} Lexer;

const char *LexerTokenTypeStr(TokenType type);
long LexerTokenLength(LexerToken *token);
void LexerSetType(Lexer *inst, LexerToken *token);

Lexer LexerLex(char *data, const char *specials, int flags);
void LexerDestroy(Lexer *inst);

#endif
