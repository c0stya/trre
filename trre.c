#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>

#define OP_ALT		'|' << 8
#define OP_CAT		',' << 8
#define OP_COL		':' << 8
#define OP_STAR		'*' << 8
#define OP_PLUS		'+' << 8
#define OP_QUEST	'?' << 8
#define OP_ITER		'I' << 8

#define L_PR	'(' << 8
#define R_PR	')' << 8
#define L_BR	'[' << 8
#define R_BR	']' << 8
#define L_CB	'{' << 8
#define R_CB	'}' << 8

#define EOS	'$' << 8
#define EPS	'E' << 8
#define ANY	'.' << 8

#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define SHIFT(c) ((c) << 8)

#define PUSH(stack, item) 	(*(stack)++ = (item))
#define POP(stack) 		(*--(stack))


enum trre_state_type {
    ST_CHAR,
    ST_CHAR_ANY,
    ST_SPLIT,
    ST_JOIN,
    ST_FINAL,
    ST_ITER,
    ST_MODE
};

enum trre_infer_mode {
    MODE_SCAN,
    MODE_MATCH,
};

struct trre_state {
    enum trre_state_type type;
    char val;
    struct trre_state *next;
    struct trre_state *nextb;
    struct trre_state *nextc;
};

// static int step_id = 0;
static int n_states = 0;

#define PRODCONS 	0
#define CONS		1
#define PROD 		2

struct tr_chunk {
    struct trre_state *head;
    struct trre_state *tail;
};


bool is_operator(const char c) {
    return (
    	    c == '|' ||
    	    c == '.' ||
    	    c == '+' ||
    	    c == '*' ||
    	    c == '?' ||
    	    c == ':' ||
    	    c == '(' ||
    	    c == ')' ||
    	    c == '{' ||
    	    c == '}' ||
    	    c == '[' ||
    	    c == ']'
    ) ? 1: 0;
}


int precedence(const uint16_t op) {
    switch (op) {
	case OP_ALT:
	    return 1;
	case OP_CAT:
	    return 2;
	case OP_PLUS:
	case OP_ITER:
	case OP_STAR:
	    return 3;
	case OP_COL:
	    return 4;
	default:
	    return -1;
    }
}


void parse_escape(const char *src, uint16_t *dst) {
    bool escape = false;
    for(; *src != '\0'; src++) {
        if (escape) {
            *dst++ = (uint16_t)*src;
            escape = false;
        } else if (*src == '\\')
	    escape = true;
	else
	    *dst++ = is_operator(*src) ? SHIFT((uint16_t)*src) : (uint16_t)*src;
    }
    *dst = EOS; // Terminate the destination array
}


void parse_inject_cat(const uint16_t *src, uint16_t *dst) {
    int prev = 0;
    int inside = 0;	// in-block indicator
    for( ;*src != EOS; src++) {
    	if (!inside) // skip [] {} blocks
	    if ((*src < 256 || *src == ANY || *src == L_PR) && prev)
		*dst++ = OP_CAT;

	// rough approach. todo: add syntax validation.
	if (*src == L_BR || *src == L_CB)
	    inside = 1;
	else if (*src == R_BR || *src == R_CB)
	    inside = 0;

	prev = (*src < 256 || *src == ANY || *src == R_PR || *src == OP_STAR || *src == OP_PLUS || *src == OP_QUEST);
	*dst++ = *src;
    }
    *dst = EOS;
}


// ugly solution. todo: re-design the logic
/*
void normalize(const uint16_t *src, uint16_t *dst) {
    const uint16_t *prev = NULL;
    int prev = 0;
    for(;;) {
    	if ( 	((prev == NULL || (*prev != R_PR && *prev > 255)) && *src == TRD) ||	// left eps: (:a)
    		((prev != NULL && *prev == TRD) && (*src != L_PR && *src > 255)))	// right eps: (a:)
		    *dst++ = EPS;
	if ( 	prev != NULL &&
		(*src < 256 || *src == L_BR || *src == L_PR) &&
		(*prev < 256 || *prev == R_BR || *prev == R_PR || *prev == STR || *prev == PLS || *prev == QST))
		    *dst++ = CAT;
	prev = src;
	// TODO: refactor below
	if (*src == EOS) {
	    *dst = EOS;
	    return;
	}
	*dst++ = *src++;
    }
}
*/


int parse_iteration(const uint16_t *src, uint16_t *dst) {
    int inside=0;
    uint16_t count=0, count_g=0, count_l=0;

    for(; *src != EOS; src++) {
	if (*src == L_CB)  {
	    inside = 1;
	} else if (inside) {
	    if (*src >= '0' && *src <='9') {
		count = count*10 + *src - '0';
	    }
	    if (*src == ',') {
		// todo: add a sentinel for a double comma
		count_g = count;
		count = 0;
		inside += 1;
	    }
	    if (*src == R_CB) {
	    	if (inside == 1) {			// one operand
	    	    count_g = count;
		    count_l = count;
		} else					// two operands
		    count_l = count;

		*dst++ = OP_ITER;
		*dst++ = count_g;
		*dst++ = count_l;

		count = 0;
		inside = 0;
	    }
	} else
	    *dst++ = *src;
    }
    *dst = EOS;
    return 0;
}


int infix_to_postfix(const uint16_t *src, uint16_t *dst) {
    uint16_t stack[10000];
    int top = -1;

    for(; *src != EOS; src++) {
	switch(*src) {
	    case L_PR:
		stack[++top] = *src;
		break;
	    case R_PR:
		while(top > -1 && stack[top] != L_PR)
		    *dst++ = stack[top--];
		if (top > -1 && stack[top] != L_PR)
		    return -1;	 	// syntax error
		else
		    top--;	     	// remove '(' from stack
		break;
	    //case OP_ITER:
	    case OP_ALT:
	    case OP_CAT:
	    //TODO: check the following
	    //case '*':
	    //case '+':
	    case OP_COL:
		while(top > -1 && precedence(stack[top]) >= precedence(*src))
		    *dst++ = stack[top--];
		stack[++top] = *src;
		break;
	    default:
		*dst++ = *src;
		break;
	}
    }

    while (top > -1)
	*dst++ = stack[top--];
    *dst = EOS;
    return 0;
}


struct tr_chunk chunk(struct trre_state *head, struct trre_state *tail)
{
	struct tr_chunk chnk = {head, tail};
	return chnk;
}

struct trre_state * create_state(struct trre_state *next, struct trre_state *nextb, int type) {
    struct trre_state *state;

    state = malloc(sizeof(struct trre_state));
    state->next = next;
    state->nextb = nextb;
    state->type = type;

    n_states++;

    return state;
}

struct trre_state* postfix_to_nft(const uint16_t * postfix) {
    struct trre_state *state, *left, *right, *prev;
    struct trre_state *state_cons, *state_prod, *state_prodcons;
    const uint16_t *c = postfix;
    struct tr_chunk stack[10000], *stackp, ch0, ch1;
    int g_iter, l_iter;

    stackp = stack;

    #define push(s) *stackp++ = s
    #define pop() *--stackp

    for(; *c != EOS; c++) {
    	switch(*c & 0xff00) {
	    case OP_STAR:
		ch0 = pop();
		state = create_state(NULL, ch0.head, ST_SPLIT);
		ch0.tail->next = state;
	    	push(chunk(state, state));
	    	break;
	    case OP_PLUS:
		ch0 = pop();
		state = create_state(NULL, ch0.head, ST_SPLIT);
		ch0.tail->next = state;
	    	push(chunk(ch0.head, state));
	    	break;
	    case OP_QUEST:
		ch0 = pop();
		right = create_state(NULL, NULL, ST_JOIN);
		left = create_state(right, ch0.head, ST_SPLIT);
	    	ch0.tail->next = right;
	    	push(chunk(left, right));
	    	break;
	    case OP_ALT:
	    	ch1 = pop();
	    	ch0 = pop();
		left = create_state(ch0.head, ch1.head, ST_SPLIT);
		right = create_state(NULL, NULL, ST_JOIN);
		ch0.tail->next = right;
		ch1.tail->next = right;
	    	push(chunk(left, right));
		break;
	    case OP_ITER:
	    	g_iter = (uint16_t)*++c;
	    	l_iter = (uint16_t)*++c;
	    	assert (*c != EOS);

		ch0 = pop();
		right = create_state(NULL, NULL, ST_SPLIT);
		prev = right;

		for (int i=0; i < MAX(g_iter, l_iter); i++) {
		    state = create_state(ch0.head, ch0.tail, ST_ITER);
		    state->nextc = create_state(prev, NULL, ST_ITER);
		    if (i == 0 && l_iter == 0)
			right->nextb = state;	// *-closure
		    if (MAX(g_iter, l_iter) - i > g_iter)
			(state->nextc)->nextb = right;
		    prev = state;
		}
		push(chunk(prev, right));
	    	break;
	    case OP_CAT:		// cat
	    	ch1 = pop();
	    	ch0 = pop();
	    	ch0.tail->next = ch1.head;
	    	push(chunk(ch0.head, ch1.tail));
	    	break;
	    case OP_COL:
	    	ch1 = pop();
	    	ch0 = pop();

		state_cons = create_state(ch0.head, NULL, ST_MODE);
		state_cons->val = CONS;
		state_prod = create_state(ch1.head, NULL, ST_MODE);
		state_prod->val = PROD;
		state_prodcons = create_state(NULL, NULL, ST_MODE);
		state_prodcons->val = PRODCONS;

	    	ch0.tail->next = state_prod;
	    	ch1.tail->next = state_prodcons;

	    	push(chunk(state_cons, state_prodcons));
	    	break;
	    case ANY:
		state = create_state(NULL, NULL, ST_CHAR_ANY);
		state->val = 0;
		push(chunk(state, state));
		break;
	    case EPS:
		state = create_state(NULL, NULL, ST_JOIN);
		//state->val =(char)*c;
		push(chunk(state, state));
	    	break;
	    default:
		state = create_state(NULL, NULL, ST_CHAR);
		state->val =(char)*c;
		push(chunk(state, state));
	    	break;
        }
    }

    ch0 = pop();
    assert(stackp == stack);

    state = malloc(sizeof(struct trre_state));
    state->type = ST_FINAL;
    state->next = NULL;
    ch0.tail->next = state;

    #undef push
    #undef pop

    return ch0.head;
}


/* Run NFA to determine whether it matches s. */
int infer(struct trre_state *start, char *input, enum trre_infer_mode infer_mode)
{
    struct trre_state *state=start, **states, **st;
    int *indices, *it, i=0, o=0;
    int mode=PRODCONS;
    int stop=0;
    char output[10000];

    states = malloc(10 * n_states * sizeof(struct trre_state *));
    indices = malloc(10 * (n_states) * sizeof(int));

    st = states;
    it = indices;

    while (!stop && (state != NULL || st != states)) {
    	if (state == NULL) {
	    state = POP(st);
	    mode = POP(it);
	    o = POP(it);
	    i = POP(it);
	}
	switch (state->type) {
	    case(ST_CHAR):
		if(mode == CONS && state->val == input[i] && input[i] != '\0') {
		    i++;
		}
		else if(mode == PRODCONS && state->val == input[i] && input[i] != '\0') {
		    output[o] = state->val;
		    i++; o++;
		}
		else if(mode == PROD) {
		    output[o] = state->val;
		    o++;
		} else {
		    state = NULL;
		    break;
		}
		state = state->next;
		break;
	    case(ST_CHAR_ANY):
		if(mode == CONS && input[i] != '\0') {
		    i++;
		}
		else if(mode == PRODCONS && input[i] != '\0') {
		    output[o] = input[i];
		    i++; o++;
		}
		else if(mode == PROD) {
		    // what is the proper interpretation?
		    output[o] =  input[i];
		    o++;
		} else {
		    state = NULL;
		    break;
		}
		state = state->next;
		break;
	    case(ST_SPLIT):
	    	if (state->nextb) {
		    PUSH(st, state->nextb);
		    PUSH(it, i);
		    PUSH(it, o);
		    PUSH(it, mode);
		}
		state = state->next;
		break;
	    case(ST_ITER):
	    	if (state->nextc->nextb) {
		    PUSH(st, state->nextc->nextb);
		    PUSH(it, i);
		    PUSH(it, o);
		    PUSH(it, mode);
		}
	    	// re-link the connection: tail -> next gate
	    	(state->nextb)->next = (state->nextc)->next; // res
	    	state = state->next;
		break;
	    case(ST_JOIN):
		state = state->next;
		break;
	    case(ST_MODE):
	    	mode = state->val;
		state = state->next;
		break;
	    case(ST_FINAL):
		if (input[i] == '\0') {
		    output[o] = '\0';
		    fprintf(stdout, "%s\n", output);
		    if (infer_mode == MODE_SCAN)
			stop = 1;
		}
		state = state->next;
		break;
	    }
    }

    free(states);
    free(indices);

    return 0;
}


int main(int argc, char **argv)
{
    FILE * fp;
    char * line = NULL;
    size_t trre_len = 0;
    size_t input_len = 0;
    ssize_t read;

    uint16_t *prep, *infix, *infix2, *postfix;
    struct trre_state *state;

    if(argc == 3){
	fp = fopen(argv[2], "r");
	if (fp == NULL) {
	    fprintf(stderr, "error: can not open file %s\n", argv[2]);
	    exit(EXIT_FAILURE);
	}
    } else if (argc == 2) {
    	fp = stdin;
    } else {
	fprintf(stderr, "usage: trre <trregexp> <filename>\n");
	exit(EXIT_FAILURE);
    }

    trre_len = strlen(argv[1]);
    printf("len: %ld", trre_len);

    prep = malloc((trre_len+1)*sizeof(uint16_t));
    if (!prep) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    infix = malloc((trre_len+1)*sizeof(uint16_t)*2);
    infix2 = malloc((trre_len+1)*sizeof(uint16_t)*2);
    postfix = malloc((trre_len+1)*sizeof(uint16_t)*2);

    parse_escape(argv[1], prep);
    parse_inject_cat(prep, infix);
    parse_iteration(infix, infix2);
    for(uint16_t *s = infix2; *s != EOS; s++)
	printf("%c", *s > 255 ? *s >> 8 : *s);
    printf("\n");
    infix_to_postfix(infix2, postfix);
    for(uint16_t *s = postfix; *s != EOS; s++)
	printf("%c", *s > 255 ? *s >> 8 : *s);
    printf("\n");
    state = postfix_to_nft(postfix);

    if(state == NULL){
	fprintf(stderr, "error: syntax");
	exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &input_len, fp)) != -1) {
        line[read-1] = '\0';  // is it valid?
	infer(state, line, MODE_MATCH);
        //printf("given: %s\n", line);
    }

    free(prep);
    free(infix);
    free(postfix);

    fclose(fp);
    if (line)
        free(line);
    exit(EXIT_SUCCESS);

    return 0;
}


#undef PUSH
#undef POP
