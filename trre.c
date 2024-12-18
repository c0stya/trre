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
#define OP_RNG		'-' << 8

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

// #define PUSH(stack, item) 	(*(stack)++ = (item))
// #define POP(stack) 		(*--(stack))

#define STACK_MAX_CAPACITY	1000000
#define MAX_OUTPUT		  10000

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

struct trre_chunk {
    struct trre_state *head;
    struct trre_state *tail;
};

ssize_t stack_top 	= -1;
ssize_t stack_capacity 	= 0;

struct trre_stack_item {
    struct trre_state *state;
    unsigned int i;
    unsigned int o;
    unsigned int mode;
};

void stack_resize(struct trre_stack_item *stack, ssize_t new_capacity) {
    stack = realloc(stack, new_capacity * sizeof(struct trre_stack_item));
    if (stack == NULL) {
	fprintf(stderr, "error: stack memory allocation failed\n");
	exit(EXIT_FAILURE);
    }
    stack_capacity = new_capacity;
}

void stack_push(struct trre_stack_item *stack, struct trre_stack_item item) {
    if (stack_top + 1 == stack_capacity) {
        if (stack_capacity * 2 > STACK_MAX_CAPACITY) {
	    fprintf(stderr, "error: stack max capacity reached\n");
	    exit(EXIT_FAILURE);
	}
	stack_resize(stack, stack_capacity * 2);
    }
    stack[++stack_top] = item;
}

struct trre_stack_item stack_pop(struct trre_stack_item *stack) {
    if (stack_top == -1) {
	fprintf(stderr, "error: stack underflow\n");
        exit(EXIT_FAILURE);
    }
    return stack[stack_top--];
}


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
	case OP_RNG:
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


void preprocess_escape(const char *src, uint16_t *dst) {
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

/* In the main cycle we restore the concatenation. Whenever we see two characters or closing parenthesis/brackets
 * with a character we inject the CAT symbol.
 *
*/


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

struct trre_preprocess_state {
    int code;
    const uint16_t *src;
    uint16_t *dst;
};


struct trre_preprocess_state
parse_brackets(const uint16_t *src, uint16_t *dst) {
    int prev = 0;

    *dst++ = L_PR;				// brackets -> parenthesis

    for(src++; *src != EOS; src++) {
    	if (*src == '-') {
	    *dst++ = OP_RNG;
	    prev = 0;
    	}
    	else if (*src == OP_COL) {
	    *dst++ = *src;
	    prev = 0;
    	}
	else if (*src < 256) {
	    if (prev) {
	    	*dst++ = OP_ALT;
	    } else
		prev = 1;
	    *dst++ = *src;
	}
	else if (*src == R_BR) {
	    *dst++ = R_PR;			// brackets -> parenthesis
	    return (struct trre_preprocess_state){0, src, dst};
	} else
	    break;
    }

    return (struct trre_preprocess_state){-1, src, dst};
}


struct trre_preprocess_state
parse_iteration(const uint16_t *src, uint16_t *dst) {
    uint16_t count=0, count_g=0, count_l=0;
    int next = 0;

    for(src++; *src != EOS; src++) {		// shift {
	if (*src >= '0' && *src <='9') {
	    count = count*10 + *src - '0';
	} else if (*src == ',') {
	    // todo: add a sentinel for a double comma
	    count_g = count;
	    count = 0;
	    next = 1;
	} else if (*src == R_CB) {
	    if (next < 1) {			// one operand
		count_g = count;
		count_l = count;
	    } else				// two operands
		count_l = count;

	    *dst++ = OP_ITER;
	    *dst++ = count_g;
	    *dst++ = count_l;

	    return (struct trre_preprocess_state){0, src, dst};

	} else {
	    break;				// syntax error
	}
    }

    return (struct trre_preprocess_state){-1, src, dst};
}


int preprocess_normalize(const uint16_t *src, uint16_t *dst) {
    int prev = 0;
    struct trre_preprocess_state ret;

    for( ;*src != EOS; src++) {

	if (*src == L_CB) {
	    ret = parse_iteration(src, dst);
	    if (ret.code != 0) {
		fprintf(stderr, "error: failed to parse iteration\n");
		exit(EXIT_FAILURE);
	    }
	    src = ret.src;
	    dst = ret.dst;
	    prev = 1;				// inject cat
	    continue;				// skip }
	} else if (*src == L_BR) {
	    if (prev)				// inject left cat explicitly
		*dst++ = OP_CAT;
	    ret = parse_brackets(src, dst);
	    if (ret.code != 0) {
		fprintf(stderr, "error: failed to parse brackets\n");
		exit(EXIT_FAILURE);
	    }
	    src = ret.src;
	    dst = ret.dst;
	    prev = 1;				// inject right cat implicitly
	    continue;				// skip ]
	}

	if ((*src < 256 || *src == ANY || *src == L_PR) && prev)
	    *dst++ = OP_CAT;

	prev = (*src < 256 || *src == ANY || *src == R_PR || *src == OP_STAR || *src == OP_PLUS || *src == OP_QUEST);
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


struct trre_chunk chunk(struct trre_state *head, struct trre_state *tail)
{
	struct trre_chunk chnk = {head, tail};
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
    struct trre_chunk stack[10000], *stackp, ch0, ch1;
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
    struct trre_state *state=start;
    struct trre_stack_item *stack, stack_item;
    int i=0, o=0, mode=PRODCONS;
    int stop=0;
    char output[MAX_OUTPUT];

    stack = malloc(n_states * sizeof(struct trre_stack_item));
    stack_top = -1;
    stack_capacity = n_states;

    while (!stop && (state != NULL || stack_top > -1)) {
    	if (state == NULL) {
    	    stack_item = stack_pop(stack);
	    state = stack_item.state;
	    i = stack_item.i;
	    o = stack_item.o;
	    mode = stack_item.mode;
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
	    	    stack_item = (struct trre_stack_item){state->nextb, i, o, mode};
	    	    stack_push(stack, stack_item);
		}
		state = state->next;
		break;
	    case(ST_ITER):
	    	if (state->nextc->nextb) {
	    	    stack_item = (struct trre_stack_item){state->nextc->nextb, i, o, mode};
	    	    stack_push(stack, stack_item);
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

    free(stack);
    return 0;
}


int main(int argc, char **argv)
{
    FILE *fp;
    char *trre_expr;
    size_t trre_len = 0, input_len = 0;
    ssize_t read;
    char *line = NULL, *input_fn;
    enum trre_infer_mode infer_mode = MODE_SCAN;

    uint16_t *prep, *infix, *postfix;
    struct trre_state *state;

    int opt, debug=0;

    while ((opt = getopt(argc, argv, "dm")) != -1) {
       switch (opt) {
       case 'd':
	   debug = 1;
	   break;
       case 'm':
       	   infer_mode = MODE_MATCH;
	   break;
       default: /* '?' */
	   fprintf(stderr, "Usage: %s [-d] [-m] expr [file]\n",
		   argv[0]);
	   exit(EXIT_FAILURE);
       }
    }

    if (optind >= argc) {
       fprintf(stderr, "error: missing trre expression\n");
       exit(EXIT_FAILURE);
    }

    // printf("argc: %d\n", argc);
    // printf("optind: %d\n", optind);
    trre_expr = argv[optind];
    trre_len = strlen(trre_expr);

    if (optind == argc - 2) {		// filename provided
	input_fn = argv[optind + 1];

	fp = fopen(input_fn, "r");
	if (fp == NULL) {
	    fprintf(stderr, "error: can not open file %s\n", input_fn);
	    exit(EXIT_FAILURE);
	}
    } else
    	fp = stdin;

    prep = malloc((trre_len+1)*sizeof(uint16_t));
    if (!prep) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    infix = malloc(2*trre_len*sizeof(uint16_t));
    postfix = malloc(2*trre_len*sizeof(uint16_t));

    preprocess_escape(trre_expr, prep);
    preprocess_normalize(prep, infix);

    if (debug) {
	fprintf(stderr, "debug: infix: \t");
	for(uint16_t *s = infix; *s != EOS; s++)
	    fprintf(stderr, "%c", *s > 255 ? *s >> 8 : *s);
	fprintf(stderr, "\n");
    }

    infix_to_postfix(infix, postfix);

    if (debug) {
	fprintf(stderr, "debug: postifix: \t");
	for(uint16_t *s = postfix; *s != EOS; s++)
	    fprintf(stderr, "%c", *s > 255 ? *s >> 8 : *s);
	fprintf(stderr, "\n");
    }

    state = postfix_to_nft(postfix);

    if(state == NULL){
	fprintf(stderr, "error: syntax");
	exit(EXIT_FAILURE);
    }

    while ((read = getline(&line, &input_len, fp)) != -1) {
        line[read-1] = '\0';  // is it valid?
	infer(state, line, infer_mode);
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
