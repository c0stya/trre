#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>


int prec(char c) {
    switch (c) {
    	case '|': 		return 1;
    	case '-': case '.': 	return 2;
    	case ':':		return 3;
	case '?': case '*':
	case '+': case 'I': 	return 4;
	case '\\':		return 5;
    }
    return -1;
}

struct node {
    unsigned char type;
    unsigned char val;
    struct node * l;
    struct node * r;
};

#define push(stack, item) 	*stack++ = item
#define pop(stack) 		*--stack
#define top(stack) 		*(stack-1)

#define STACK_MAX_CAPACITY	1000000

static unsigned char operators[1024];
static struct node *operands[1024];

static unsigned char *opr = operators;
static struct node **opd = operands;

static char* output;
static size_t output_capacity=32;


struct node * create_node(unsigned char type, struct node *l, struct node *r) {
    struct node *node = malloc(sizeof(struct node));
    if (!node) {
	fprintf(stderr, "error: node memory allocation failed\n");
	exit(EXIT_FAILURE);
    }
    node->type = type;
    node->val = 0;
    node->l = l;
    node->r = r;
    return node;
}

// alias-helper
struct node * create_nodev(unsigned char type, unsigned char val) {
    struct node *node = create_node(type, NULL, NULL);
    node->val = val;
    return node;
}

enum infer_mode {
    SCAN,
    MATCH,
};


// reduce immediately
void reduce_postfix(char op, int ng) {
    struct node *l, *r;

    switch(op) {
	case '*': case '+': case '?':
	    l = pop(opd);
	    r = create_node(op, l, NULL);
	    r->val = ng;
	    push(opd, r);
	    break;
	default:
	    fprintf(stderr, "error: unexpected postfix operator\n");
	    exit(EXIT_FAILURE);
    }
}


void reduce() {
    char op;
    struct node *l, *r;

    op = pop(opr);
    switch(op) {
	case '|': case '.': case ':': case '-':
	    r = pop(opd);
	    l = pop(opd);
	    push(opd, create_node(op, l, r));
	    break;
	case '(':
	    fprintf(stderr, "error: unmached parenthesis\n");
	    exit(EXIT_FAILURE);
    }
}


void reduce_op(char op) {
    while(opr != operators && prec(top(opr)) >= prec(op))
        reduce();
    push(opr, op);
}

char* parse_curly_brackets(char *expr) {
    int state = 0, ng = 0;
    int count = 0, lv = 0;
    struct node *l, *r;
    char c;

    while ((c = *expr) != '\0') {
        if (c >= '0' && c <= '9') {
            count = count*10 + c - '0';
	} else if (c == ',') {
            lv = count;
            count = 0;
            state += 1;
	} else if (c == '}') {
	    if(*(expr+1) == '?') {			// safe to do, we have the '/0' sentinel
		ng = 1;
		expr++;
	    }
            if (state == 0)
            	lv = count;
	    else if (state > 1) {
		fprintf(stderr, "error: more then one comma in curly brackets\n");
		exit(EXIT_FAILURE);
	    }

	    r = create_nodev(lv, count);
	    l = create_node('I', pop(opd), r);
	    l->val = ng;
	    push(opd, l);

            return expr;
        } else {
	    fprintf(stderr, "error: unexpected symbol in curly brackets: %c\n", c);
	    exit(EXIT_FAILURE);
        }
        expr++;
    }
    fprintf(stderr, "error: unmached curly brackets");
    exit(EXIT_FAILURE);
}

char* parse_square_brackets(char *expr) {
    char c;
    int state = 0;

    while ((c = *expr) != '\0') {
        if (state == 0) {              			   // expect operand
            switch(c) {
		case ':': case '-': case '[': case ']':
		    fprintf(stderr, "error: unexpected symbol in square brackets: %c", c);
		    exit(EXIT_FAILURE);
		default:
		    push(opd, create_nodev('c', c));       // push operand
		    state = 1;
	    }
	} else {                       		   	   // expect operator
            switch(c) {
		case ':': case '-':
		    reduce_op(c);
		    state = 0;
		    break;
		case ']':
		    while (opr != operators && top(opr) != '[')
			reduce();
		    --opr;              // remove [ from the stack
		    return expr;
		default:                // implicit alternation
		    reduce_op('|');
		    state = 0;
		    continue;		// skip expr increment
	    }
	}
        expr++;
    }
    fprintf(stderr, "error: unmached square brackets");
    exit(EXIT_FAILURE);
}


struct node * parse(char *expr) {
    unsigned char c;
    int state = 0;

    while ((c = *expr) != '\0') {
        if (state == 0) {                     	// expect operand
            switch(c) {
		case '(':
		    push(opr, c);
		    break;
		case '[':
		    push(opr, c);
		    expr = parse_square_brackets(expr+1);
		    state = 1;
		    break;
		case '\\':
		    push(opd, create_nodev('c', *++expr));
		    state = 1;
		    break;
		case '.':
		    push(opd, create_node('-',
		    		create_nodev('c', 0),
		    		create_nodev('c', 255)));
		    state = 1;
		    break;
		case ':':					// epsilon as an implicit left operand
		    push(opd, create_nodev('e', c));
		    state = 1;
		    continue;					// stay in the same position in expr
		case '|': case '*': case '+': case '?':
		case ')': case '{': case '}':
		    if (opr != operators && top(opr) == ':') { 	// epsilon as an implicit right operand
			push(opd, create_nodev('e', c));
			state = 1;
			continue;				// stay in the same position in expr
		    } else {
			fprintf(stderr, "error: unexpected symbol %c\n", c);
			exit(EXIT_FAILURE);
		    }
		default:
		    push(opd, create_nodev('c', c));
		    state = 1;
            }
	} else {               					// expect postfix or binary operator
	    switch (c) {
	    case '*': case '+': case '?':
                if(*(expr+1) == '?') {				// safe to do, we have the '/0' sentinel
		    reduce_postfix(c, 1);			// found non-greedy modifier
		    expr++;
                }
		else
		    reduce_postfix(c, 0);
                break;
            case '|':
                reduce_op(c);
                state = 0;
                break;
            case ':':
                if (*(expr+1) == '\0') {		// implicit epsilon as a right operand
                    push(opd, create_nodev('e', c));
                }
		reduce_op(c);
		state = 0;
		break;
            case '{':
                expr = parse_curly_brackets(expr+1);
                break;
            case ')':
                while (opr != operators && top(opr) != '(')
                    reduce();
                if (opr == operators || top(opr) != '(') {
		    fprintf(stderr, "error: unmached parenthesis");
		    exit(EXIT_FAILURE);
		}
                --opr;                       	// remove ( from the stack
                break;
            default:                            // implicit cat
                reduce_op('.');
                state = 0;
                continue;
            }
	}
        expr++;
    }

    while (opr != operators) {
        reduce();
    }
    assert (operands != opd);

    return pop(opd);
}


void plot_ast(struct node *n) {
    struct node *stack[1024];
    struct node **sp = stack;

    printf("digraph G {\n\tsplines=true; rankdir=TD;\n");

    push(sp, n);
    while (sp != stack) {
        n = pop(sp);

        if (n->type == 'c') {
            //printf("ntype: %c", n->type);
            printf("\t\"%p\" [peripheries=2, label=\"%u\"];\n", (void*)n, n->val);
        } else {
            printf("\t\"%p\" [label=\"%c\"];\n", (void*)n, n->type);
        }

        if (n->l) {
            printf("\t\"%p\" -> \"%p\";\n", (void*)n, (void*)n->l);
            push(sp, n->l);
        }

        if (n->r) {
            printf("\t\"%p\" -> \"%p\" [label=\"%c\"];\n", (void*)n, (void*)n->r, 'r');
            push(sp, n->r);
        }
    }
    printf("}\n");
}


enum nstate_type {
    PROD,
    CONS,
    SPLIT,
    SPLITNG,
    JOIN,
    FINAL
};


// nft state
struct nstate {
    enum nstate_type type;
    unsigned char val;
    unsigned char mode;
    struct nstate *nexta;
    struct nstate *nextb;
    uint8_t visited;
};


struct nstate* create_nstate(enum nstate_type type, struct nstate *nexta, struct nstate *nextb) {
    struct nstate *state;

    state = malloc(sizeof(struct nstate));
    if (state == NULL) {
        fprintf(stderr, "error: nft state memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    state->type = type;
    state->nexta = nexta;
    state->nextb = nextb;
    state->val = 0;
    state->visited = 0;

    return state;
}


struct nchunk {
    struct nstate * head;
    struct nstate * tail;
};


struct nchunk chunk(struct nstate *head, struct nstate *tail) {
    struct nchunk chunk;
    chunk.head = head;
    chunk.tail = tail;
    return chunk;
}


struct nchunk nft(struct node *n, char mode) {
    struct nstate *split, *psplit, *join;
    struct nstate *cstate, *pstate, *state, *head, *tail, *final;
    struct nchunk l, r;
    int llv, lrv, rlv;
    int lb, rb;

    if (n == NULL)
    	return chunk(NULL, NULL);

    switch(n->type) {
	case '.':
	    l = nft(n->l, mode);
	    r = nft(n->r, mode);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '|':
	    l = nft(n->l, mode);
	    r = nft(n->r, mode);
	    split = create_nstate(SPLITNG, l.head, r.head);
	    join = create_nstate(JOIN, NULL, NULL);
	    l.tail->nexta = join;
	    r.tail->nexta = join;
	    return chunk(split, join);
	case '*':
	    l = nft(n->l, mode);
	    split = create_nstate(n->val ? SPLITNG : SPLIT, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(split, split);
	case '?':
	    l = nft(n->l, mode);
	    join = create_nstate(JOIN, NULL, NULL);
	    split = create_nstate(n->val ? SPLITNG : SPLIT, join, l.head);
	    l.tail->nexta = join;
	    return chunk(split, join);
	case '+':
	    l = nft(n->l, mode);
	    split = create_nstate(n->val ? SPLITNG : SPLIT, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(l.head, split);
	case ':':
	    if (n->l->type == 'e') // implicit eps left operand
	    	return nft(n->r, 2);
	    if (n->r->type == 'e')
		return nft(n->l, 1);
	    // implicit eps right operand
	    l = nft(n->l, 1);
	    r = nft(n->r, 2);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '-':
	    if (n->l->type == 'c' && n->r->type == 'c') {
		join = create_nstate(JOIN, NULL, NULL);
		psplit = NULL;
		for(int c=n->r->val; c >= n->l->val; c--){
		    l = nft(create_nodev('c', c), mode);
		    split = create_nstate(SPLITNG, l.head, psplit);
		    l.tail->nexta = join;
		    psplit = split;
		}
		return chunk(psplit, join);
	    } else if (n->l->type == ':' && n->r->type == ':') {
		join = create_nstate(JOIN, NULL, NULL);
		psplit = NULL;

		llv = n->l->l->val;
		lrv = n->l->r->val;
		rlv = n->r->l->val;
		for(int c=rlv-llv; c >= 0; c--) {
		    l = nft(create_node(':',
		    	    create_nodev('c', llv+c),
		    	    create_nodev('c', lrv+c)
		    	    ), mode);
		    split = create_nstate(SPLITNG, l.head, psplit);
		    l.tail->nexta = join;
		    psplit = split;
		}
		return chunk(psplit, join);
	    }
	    else {
		fprintf(stderr, "error: unexpected range syntax\n");
		exit(EXIT_FAILURE);
	    }
	case 'I':
	    lb = n->r->type;		/* placeholder for the left range */
	    rb = n->r->val;		/* placeholder for the right range */
	    head = tail = create_nstate(JOIN, NULL, NULL);

	    for(int i=0; i <lb; i++) {
		l = nft(n->l, mode);
		tail->nexta = l.head;
		tail = l.tail;
	    }

	    if(rb == 0) {
		l = nft(create_node('*', n->l, NULL), mode);
		tail->nexta = l.head;
		tail = l.tail;
	    } else {
		final = create_nstate(JOIN, NULL, NULL);
		for(int i=lb; i < rb; i++) {
		    l = nft(n->l, mode);
		    // TODO: add non-greed modifier
		    tail->nexta = create_nstate(n->val ? SPLITNG : SPLIT, final, l.head);
		    tail = l.tail;
		}
		tail->nexta = final;
		tail = final;
	    }

	    return chunk(head, tail);
	default: 	 	//character
	    if (mode == 0) {
		cstate = create_nstate(CONS, NULL, NULL);
		pstate = create_nstate(PROD, NULL, NULL);
		cstate->val = n->val;
		pstate->val = n->val;
		cstate->nexta = pstate;
		return chunk(cstate, pstate);
	    }
	    else if (mode == 1) {
		state = create_nstate(CONS, NULL, NULL);
		state->val=n->val;
	    } else {
		state = create_nstate(PROD, NULL, NULL);
		state->val=n->val;
	    }
	    return chunk(state, state);
    }
}

/*
struct nstate* create_nft(struct node *root) {
    struct nstate *final = create_nstate(FINAL, NULL, NULL);
    struct nchunk ch = nft(root, 0);
    ch.tail->nexta = final;
    return ch.head;
}
*/

struct nstate* create_nft(struct node *root) {
    struct nstate *final = create_nstate(FINAL, NULL, NULL);
    struct nstate *init = create_nstate(JOIN, NULL, NULL);
    struct nchunk ch = nft(root, 0);
    ch.tail->nexta = final;
    init->nexta = ch.head;
    return init;
}

struct sitem {
    struct nstate *s;
    size_t i;
    size_t o;
};

struct sstack {
    struct sitem *items;
    size_t n_items;
    size_t capacity;
};

struct sstack * screate(size_t capacity) {
    struct sstack *stack;

    stack = (struct sstack*)malloc(sizeof(struct sstack));
    stack->items = malloc(capacity * sizeof(struct sitem));
    if (stack == NULL || stack->items == NULL) {
	fprintf(stderr, "error: stack memory allocation failed\n");
	exit(EXIT_FAILURE);
    }
    stack->n_items = 0;
    stack->capacity = capacity;
    return stack;
}

void sresize(struct sstack *stack, size_t new_capacity) {
    stack->items = realloc(stack->items, new_capacity * sizeof(struct sitem));
    if (stack->items == NULL) {
	fprintf(stderr, "error: stack memory re-allocation failed\n");
	exit(EXIT_FAILURE);
    }
    stack->capacity = new_capacity;
}

void spush(struct sstack *stack, struct nstate *s, size_t i, size_t o) {
    struct sitem *it;
    if (stack->n_items == stack->capacity) {
        if (stack->capacity * 2 > STACK_MAX_CAPACITY) {
	    fprintf(stderr, "error: stack max capacity reached\n");
	    exit(EXIT_FAILURE);
	}
	sresize(stack, stack->capacity * 2);
    }
    it = &stack->items[stack->n_items];
    it->s = s;
    it->i = i;
    it->o = o;

    stack->n_items++;
}

size_t spop(struct sstack *stack, struct nstate **s, size_t *i, size_t *o) {
    struct sitem *it;
    if (stack->n_items == 0) {
	fprintf(stderr, "error: stack underflow\n");
        exit(EXIT_FAILURE);
    }
    stack->n_items--;
    it = &stack->items[stack->n_items];
    *s = it->s;
    *i = it->i;
    *o = it->o;

    return stack->n_items;
}


// Function to dynamically resize the output array
char* resize_output(char *output, size_t *capacity) {
    *capacity *= 2; // Double the capacity
    output = realloc(output, *capacity * sizeof(char));
    if (!output) {
        fprintf(stderr, "error: memory reallocation failed for output\n");
        exit(EXIT_FAILURE);
    }
    return output;
}

// Main NFT traversal function (depth-first)
ssize_t infer_backtrack(struct nstate *start, char *input, struct sstack *stack, enum infer_mode mode) {
    size_t i = 0, o = 0;
    struct nstate *s = start;

    while (stack->n_items || s) {
        if (!s) {
	    spop(stack, &s, &i, &o);
            if (!s) {
                continue;
            }
        }
        // Resize output array if necessary
        if (o >= output_capacity - 1) {
            output = resize_output(output, &output_capacity);
        }

        switch (s->type) {
            case CONS:
                if (input[i] != '\0' && s->val == input[i]) {
                    i++;
                    s = s->nexta;
                } else {
                    s = NULL;
                }
                break;
            case PROD:
                output[o++] = s->val;
                s = s->nexta;
                break;
            case SPLIT:
                spush(stack, s->nexta, i, o);
                s = s->nextb;
                break;
            case SPLITNG:
                spush(stack, s->nextb, i, o);
                s = s->nexta;
                break;
            case JOIN:
                s = s->nexta;
                break;
            case FINAL:
            	if (mode == MATCH) {
		    if (input[i] == '\0') {
			output[o] = '\0'; // Null-terminate the output string
			fputs(output, stdout);
			fputs("\n", stdout);
		    }
		    s = NULL;
		} else {
		    output[o] = '\0'; // Null-terminate the output string
		    fputs(output, stdout);
		    return i;
		}
                break;
            default:
                fprintf(stderr, "error: unknown state type\n");
                exit(1);
        }
    }
    return -1;
}

int stack_lookup(struct nstate **b, struct nstate **e, struct nstate *v) {
    while(b != e)
	if (v == *(--e)) return 1;
    return 0;
}



void plot_nft(struct nstate *start) {
    struct nstate *stack[1024];
    struct nstate *visited[1024];

    struct nstate **sp = stack;
    struct nstate **vp = visited;
    struct nstate *s = start;
    char l,m;

    printf("digraph G {\n\tsplines=true; rankdir=LR;\n");

    push(sp, s);
    while (sp != stack) {
        s = pop(sp);
        push(vp, s);

        if (s->type == FINAL)
            printf("\t\"%p\" [peripheries=2, label=\"\"];\n", (void*)s);
        else {
            switch(s->type) {
		case PROD: 	l=s->val; m='+'; break;
		case CONS: 	l=s->val; m='-'; break;
		case SPLITNG: 	l='S'; m='n'; break;
		case SPLIT: 	l='S'; m=' '; break;
		case JOIN: 	l='J'; m=' '; break;
		default:	l=' '; m=' ';break;
	    }
            printf("\t\"%p\" [label=\"%c%c\"];\n", (void*)s, l, m);
        }

        if (s->nexta) {
            printf("\t\"%p\" -> \"%p\";\n", (void*)s, (void*)s->nexta);
            if (!stack_lookup(visited, vp, s->nexta))
                push(sp, s->nexta);
        }

        if (s->nextb) {
            printf("\t\"%p\" -> \"%p\" [label=\"%c\"];\n", (void*)s, (void*)s->nextb, '*');
            if (!stack_lookup(visited, vp, s->nextb))
                push(sp, s->nextb);
        }
    }
    printf("}\n");
}

struct str_item {
    unsigned char c;
    struct str_item *next;
};

struct str {
    struct str_item *head;
    struct str_item *tail;
};

struct str * str_create() {
    struct str *st;
    st = malloc(sizeof(struct str));
    st->head = st->tail = NULL;
    return st;
}

struct str * str_append(struct str *s, unsigned char c) {
    struct str_item *si;
    si = malloc(sizeof(struct str_item));
    si->c = c;
    si->next = NULL;

    if (s->tail) {
    	s->tail->next = si;
    	s->tail = si;
    } else {
    	s->tail = s->head = si;
    }
    return s;
}

char str_popleft(struct str *s) {
    unsigned char c;
    struct str_item *tmp;

    if (s->head == NULL) {
	fprintf(stderr, "error: string underflow\n");
    	exit(EXIT_FAILURE);
    }

    // question: what if s->head == s->tail ??
    c = s->head->c;

    if (s->head == s->tail) {
    	free(s->head);
    	s->head = s->tail = NULL;
    } else {
	tmp = s->head;
	s->head = s->head->next;
	free(tmp);
    }

    return c;
}

struct str * str_append_str(struct str *dst, struct str *src) {
    for(struct str_item *si = src->head; si != NULL; si = si->next) {
    	str_append(dst, si->c);
    }
    return dst;
}

struct str * str_copy(struct str* src) {
    struct str *dst = str_create();
    assert(src);

    for(struct str_item *si = src->head; si != NULL; si = si->next) {
    	str_append(dst, si->c);
    }
    return dst;
}

void str_free(struct str* s) {
    struct str_item *si, *next;
    if (!s) return;

    for(si = s->head; si != NULL;) {
    	next = si->next;
    	free(si);
    	si = next;
    }
    free(s);
}

void str_print(struct str *s) {
    assert(s);
    for(struct str_item *si=s->head; si != NULL; si=si->next)
    	fputc(si->c, stdout);
}

void str_to_char(struct str *s, unsigned char *ch) {
    assert(s);
    int i = 0;
    for(struct str_item *si=s->head; si != NULL; si=si->next)
    	ch[i++] = si->c;
    ch[i] = '\0';
}



struct slist {
    struct slitem *head;
    struct slitem *tail;
    size_t n;
};

struct slitem {
    struct nstate *state;
    struct str *suffix;
    struct slitem *next;
};

struct slist * slist_create() {
    struct slist *sl;
    sl = malloc(sizeof(struct slist));
    sl->head = sl->tail = NULL;
    sl->n = 0;

    return sl;
}

struct slist * slist_append(struct slist *sl, struct nstate *state, struct str *suffix) {
    struct slitem *li;
    li = malloc(sizeof(struct slitem));
    li->state = state;
    li->suffix = suffix;
    li->next = NULL;

    if (sl->tail) {
    	sl->tail->next = li;
    	sl->tail = li;
    } else {
    	sl->tail = sl->head = li;
    }
    sl->n += 1;
    return sl;
}


void slist_free(struct slist* sl) {
    struct slitem *li, *next;
    if (!sl) return;

    for(li = sl->head; li != NULL;) {
    	next = li->next;
    	str_free(li->suffix);
    	free(li);
    	li = next;
    }
    free(sl);
}


void nft_step_(struct nstate *s, struct str *o, unsigned char c, struct slist *sl) {
    if (!s) return;


    switch(s->type) {
	case SPLIT:
	    nft_step_(s->nextb, o, c, sl);
	    nft_step_(s->nexta, str_copy(o), c, sl);
	    break;
	case SPLITNG:
	    nft_step_(s->nexta, o, c, sl);
	    nft_step_(s->nextb, str_copy(o), c, sl);
	    break;
	case JOIN:
	    nft_step_(s->nexta, o, c, sl);
	    break;
	case PROD:
	    nft_step_(s->nexta, str_append(o, s->val), c, sl);
	    break;
	case CONS:	// found CONS state marked with 'c'
	    if (c == s->val && s->visited == 0) {
	    	s->visited = 1;
		slist_append(sl, s, o);
	    }
	    break;
	case FINAL:
	    if(c == '\0' && s->visited == 0) {	/* final states closure */
		slist_append(sl, s, o);
	    	s->visited = 1;
	    }
	    return;
    }
    return;
}


struct slist * nft_step(struct slist *states, unsigned char c) {
    struct slist *sl = slist_create();
    struct slitem *li;

    for(li=states->head; li; li=li->next)
	nft_step_(li->state->nexta, str_copy(li->suffix), c, sl);

    /* reset the visited flag; yes it is linear
     * but the list have to be short */
    for(li=sl->head; li; li=li->next)
    	li->state->visited = 0;

    return sl;
}


struct dstate {
    struct slist *states;
    struct str *final_out;
    int8_t final;
    struct str *out[256];
    struct dstate *next[256];
};

struct dstate * dstate_create(struct slist *states) {
    struct dstate *ds;
    ds = malloc(sizeof(struct dstate));
    ds->states = states;
    ds->final = -1;
    memset(ds->next, 0, sizeof ds->next);
    memset(ds->out, 0, sizeof ds->out);
    return ds;
}

int dstack_lookup(struct dstate **b, struct dstate **e, struct dstate *v) {
    while(b != e)
	if (v == *(--e)) return 1;
    return 0;
}


void plot_dft(struct dstate *dstart) {
    struct dstate *stack[1024];
    struct dstate *visited[1024];

    struct dstate **sp = stack;
    struct dstate **vp = visited;
    struct dstate *s = dstart, *s_next = NULL;
    unsigned char out[32], label[32];

    printf("digraph G {\n\tsplines=true; rankdir=LR;\n");

    push(sp, s);
    while (sp != stack) {
        s = pop(sp);
        push(vp, s);

        if (s->final == 1) {
            str_to_char(s->final_out, out);
	    printf("\t\"%p\" [peripheries=2, label=\"%s\"];\n", (void*)s, out);
	}
        else
            printf("\t\"%p\" [label=\"\"];\n", (void*)s);

	for(int c=0; c < 256; c++) {
	    if ((s_next = s->next[c]) != NULL) {
	    	str_to_char(s->out[c], label);
		printf("\t\"%p\" -> \"%p\" [label=\"%c:%s\"];\n", (void*)s, (void*)s_next, c, label);
		if (!dstack_lookup(visited, vp, s_next))
		    push(sp, s_next);
	    }
        }
    }
    printf("}\n");
}



struct str * truncate_lcp(struct slist *sl, struct str *prefix) {
    struct slitem *li;
    unsigned char ch;

    for(;;) {
    	if(!sl->head->suffix->head) 			/* end of lcp */
    	    return prefix;

    	ch = sl->head->suffix->head->c;			/* omg */
	for(li=sl->head; li; li=li->next) {
	    if (!li->suffix || !li->suffix->head || li->suffix->head->c != ch)
		return prefix;
	}

	/* we found the common char,
	 * - add the char to the prefix,
	 * - truncate the suffixes */
	str_append(prefix, ch);
	for(li=sl->head; li; li=li->next)
	    str_popleft(li->suffix);
    }
    return prefix;
}

/* lexicographic comparison */
int str_cmp(struct str *a, struct str *b) {
    struct str_item *ai, *bi;
    for(ai=a->head, bi=b->head; ai && bi; ai=ai->next, bi=bi->next)
    	if (ai->c < bi->c)
    	    return -1;
	else if (ai->c > bi->c)
    	    return 1;
    if (ai)
	return 1;
    if (bi)
	return -1;

    return 0;
}

int list_cmp(struct slist *a, struct slist *b) {
    struct slitem *ai, *bi;
    int sign;

    if(a->n < b->n)
    	return -1;
    if(a->n > b->n)
    	return 1;

    for(ai=a->head, bi=b->head; ai && bi; ai=ai->next, bi=bi->next)
	if(ai->state < bi->state)
	    return -1;
	else if(ai->state > bi->state)
	    return 1;
	else {
	    sign = str_cmp(ai->suffix, bi->suffix);
	    if (sign != 0)
		return sign;
	}

    return 0;
}


// Define the structure for the tree node
struct btnode {
    struct dstate *ds;
    struct btnode *l;
    struct btnode *r;
};

// Function to create a new node with given data
struct btnode* bt_create(struct dstate *ds) {
    struct btnode* node;
    node = malloc(sizeof(struct btnode));
    if (node == NULL) {
        printf("error: binary tree node allocation failed.\n");
	exit(EXIT_FAILURE);
    }
    node->ds = ds;
    node->l = NULL;
    node->r = NULL;
    return node;
}

// Lookup function to search for a value in the binary tree
struct dstate* bt_lookup(struct btnode *n, struct slist *sl) {
    int sign = 0;

    while (n != NULL) {
    	sign = list_cmp(sl, n->ds->states);
        if (sign < 0)		/* less */
            n = n->l;
        else if (sign > 0)	/* more */
            n = n->r;
        else
            return n->ds; 	/* found */
    }
    return NULL;
}

// Function to insert nodes to form a binary search tree
struct btnode* bt_insert(struct btnode* n, struct dstate *ds) {
    int sign;
    if (n == NULL)
        return bt_create(ds);
    sign = list_cmp(ds->states, n->ds->states);
    if (sign < 0)
        n->l = bt_insert(n->l, ds);
    else if (sign > 0)
	n->r = bt_insert(n->r, ds);
    return n;
}

void bt_free(struct btnode* root) {
    if (root == NULL) return;
    bt_free(root->l);
    bt_free(root->r);
    free(root);
}


int infer_dft(struct dstate *dstart, unsigned char *inp, struct btnode *dcache, enum infer_mode mode) {
    struct dstate *ds_next, *ds=dstart;
    struct str *prefix, *out = str_create();
    struct slist *sl;

    unsigned char *c;
    int i = 0;

    for(c=inp; *c != '\0'; c++, i++) {

	if (mode == SCAN && ds->final == 1) {
	    str_print(out);
	    str_print(ds->final_out);
	    str_free(out);
	    return i;
	}

	// todo: double check this logic
	if (ds->next[*c] != NULL) {
	    str_append_str(out, ds->out[*c]);
	    ds = ds->next[*c];
	}
	else if (ds->out[*c] != NULL) {				/* already explored but found nothing */
	    break;
	}
	else {							/* not explored, explore */
	    sl = nft_step(ds->states, *c);

	    /* expand each state and accumulate CONS states labeled with c */
	    if (!sl->head) {					/* got empty list; mark as explored and exit */
	    	ds->out[*c] = (struct str*)1;			/* fixme: can we create a better indicator? */
	    	slist_free(sl);
	    	break;
	    }

	    prefix = str_create();
	    truncate_lcp(sl, prefix);

	    if ((ds_next = bt_lookup(dcache, sl)) != NULL) {
	    	ds->next[*c] = ds_next;
	    	ds->out[*c] = prefix;
	    	slist_free(sl);					/* no need for the list */
	    } else {
	    	ds_next = dstate_create(sl);
	    	ds->next[*c] = ds_next;
	    	ds->out[*c] = prefix;
	    	bt_insert(dcache, ds_next);
	    }

	    str_append_str(out, ds->out[*c]);
	    ds = ds_next;
	    //str_free(prefix);

	    // refactor this: do not make closure several times
	    if (ds->final < 0) {					/* unexplored */
		sl = nft_step(ds->states, '\0');

		if (sl->head) {						/* got final states; take the first one */
		    ds->final = 1;
		    ds->final_out = str_copy(sl->head->suffix);
		    // we do not need the sl list anymore!
		} else {
		    ds->final = 0;
		}
	    }
	}
    }

    /*
    if (mode == SCAN && ds->final == 1) {
	//printf("i:%d\n", i);
	str_print(out);
	str_print(ds->final_out);
	return i;
    }*/


    //if (mode == MATCH && ds->final == 1 && *c == '\0') {
    if (ds->final == 1 && *c == '\0') {
	str_print(out);
	str_print(ds->final_out);
    }

    str_free(out);

    return -1;
}


int main(int argc, char **argv)
{
    FILE *fp;
    char *expr;
    ssize_t read, ioffset;
    size_t input_len;
    //unsigned char *line = NULL, *input_fn, *ch;
    char *line = NULL, *input_fn, *ch;
    struct node *root;
    struct nstate *start;
    //struct sstack *stack = screate(32);
    struct dstate *dstart;
    struct btnode *dcache;
    enum infer_mode mode = SCAN;


    int opt, debug=0;

    while ((opt = getopt(argc, argv, "dma")) != -1) {
	switch (opt) {
	    case 'd':
		debug = 1;
		break;
	    case 'm':
		mode = MATCH;
		break;
	    case 'a':
		fprintf(stderr, "Not supported yet\n");
		exit(EXIT_FAILURE);
	    default: /* '?' */
		fprintf(stderr, "Usage: %s [-dma] expr [file]\n",
		       argv[0]);
		exit(EXIT_FAILURE);
	}
    }

    if (optind >= argc) {
       fprintf(stderr, "error: missing trre expression\n");
       exit(EXIT_FAILURE);
    }

    expr = argv[optind];
    root = parse(expr);

    start = create_nft(root);

    if (debug) {
	//plot_ast(root);
	plot_nft(start);
    }

    //printf("%c\n", start->type);

    // todo: can we do better?
    output = malloc(output_capacity*sizeof(char));

    struct slist *sl_init = slist_create();
    slist_append(sl_init, start, str_create());

    dstart = dstate_create(sl_init);
    dcache = bt_create(dstart);

    if (optind == argc - 2) {		// filename provided
	input_fn = argv[optind + 1];

	fp = fopen(input_fn, "r");
	if (fp == NULL) {
	    fprintf(stderr, "error: can not open file %s\n", input_fn);
	    exit(EXIT_FAILURE);
	}
    } else
    	fp = stdin;

    if (mode == SCAN) {
	while ((read = getline(&line, &input_len, fp)) != -1) {
	    line[read-1] = '\0';
	    ch = line;

	    while (*ch != '\0') {
		ioffset = infer_dft(dstart, (unsigned char*)ch, dcache, mode);
		if (ioffset > 0)
		    ch += ioffset;
		else {
		    fputc(*ch++, stdout);
		}
	    }
	    fputc('\n', stdout);
	}
    } else {	/* MATCH mode and generator */
	while ((read = getline(&line, &input_len, fp)) != -1) {
	    line[read-1] = '\0';
	    ioffset = infer_dft(dstart, (unsigned char*)line, dcache, mode);
	    fputc('\n', stdout);
	}
    }
    if (debug) {
    	plot_dft(dstart);
    }

    fclose(fp);
    if (line)
        free(line);
    return 0;
}
