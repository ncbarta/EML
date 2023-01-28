#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {ident, number, lparen, rparen, 
          times, // symbol *
          slash, // symbol \ not added yet
          plus, // symbol +
          minus, //symbol -
          eql, //symbol ==
          neq, // !=
          lss, // <
          leq,// <=
          gtr, // >
          geq, // >= 
          callsym, // not added yet 
          beginsym, // not added yet 
          semicolon, // ;
          endsym, 
          ifsym, whilesym, becomes, thensym, dosym, constsym, 
          comma, //:
          varsym, procsym, period, oddsym, 
          not, // !
          eq // =
} Symbol;

Symbol sym;
int peek;
void getsym(void);
void error(const char*);
void expression(void);
void program(void);
void nexttok(void);

#define is_num(c) ((c) >= '0' && (c) <= '9')
#define is_letter(c) ((c) <= 'a' && (c) <= 'z' || (c) >= 'A' && (c) <= 'Z')

int main(void)
{
  program();
  return 0;
}

void nexttok(void)
{
  peek = getchar();
}

void getsym(void)
{
  for(;;) {
    nexttok();
    if(peek == ' ' || peek == '\t') continue;
    else if(peek == EOF) break;
    else break;
  }
  //static char buf[256];
  switch(peek) {
  case '+': sym = plus; return;
  case '-': sym = minus; return;
  case '*': sym = times; return;
  case ';': sym = semicolon; return;
  case ':': sym = comma; return;
  case '=': 
    nexttok();
    if(peek == '=')
      sym = eql;
    else
      sym = eq;
    return;
  case '!': 
    nexttok();
    if(peek == '=')
      sym = neq;
    else
      sym = not;
    return;
  case '<':
    nexttok();
    if(peek == '=')
      sym = leq;
    else
      sym = lss;
    return;
  case '>':
    nexttok();
    if(peek == '=')
      sym = geq;
    else
      sym = gtr;
    return;
  }
  if(is_num(peek)) {
    sym = number;
    return;
  }
}

int accept(Symbol s) {
    if (sym == s) {
        getsym();
        return 1;
    }
    return 0;
}

int expect(Symbol s) {
    if (accept(s))
        return 1;
    error("expect: unexpected symbol");
    return 0;
}

void factor(void) {
    if (accept(ident)) {
        ;
    } else if (accept(number)) {
        ;
    } else if (accept(lparen)) {
        expression();
        expect(rparen);
    } else {
        error("factor: syntax error");
        getsym();
    }
}

void term(void) {
    factor();
    while (sym == times || sym == slash) {
        getsym();
        factor();
    }
}

void expression(void) {
    if (sym == plus || sym == minus)
        getsym();
    term();
    while (sym == plus || sym == minus) {
        getsym();
        term();
    }
}

void condition(void) {
    if (accept(oddsym)) {
        expression();
    } else {
        expression();
        if (sym == eql || sym == neq || sym == lss || sym == leq || sym == gtr || sym == geq) {
            getsym();
            expression();
        } else {
            error("condition: invalid operator");
            getsym();
        }
    }
}

void statement(void) {
    if (accept(ident)) {
        expect(becomes);
        expression();
    } else if (accept(callsym)) {
        expect(ident);
    } else if (accept(beginsym)) {
        do {
            statement();
        } while (accept(semicolon));
        expect(endsym);
    } else if (accept(ifsym)) {
        condition();
        expect(thensym);
        statement();
    } else if (accept(whilesym)) {
        condition();
        expect(dosym);
        statement();
    } else {
        error("statement: syntax error");
        getsym();
    }
}

void block(void) {
    if (accept(constsym)) {
        do {
            expect(ident);
            expect(eql);
            expect(number);
        } while (accept(comma));
        expect(semicolon);
    }
    if (accept(varsym)) {
        do {
            expect(ident);
        } while (accept(comma));
        expect(semicolon);
    }
    while (accept(procsym)) {
        expect(ident);
        expect(semicolon);
        block();
        expect(semicolon);
    }
    statement();
}

void program(void) {
    getsym();
    block();
    expect(period);
}

void error(const char *msg)
{
  fprintf(stderr,"error: %s\n",msg);
  exit(1);
}