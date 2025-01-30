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


struct node * create_node(char type, struct node *l, struct node *r) {
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
		    //push(opd, create_nodev('a', 0));
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
    char val;
    char mode;
    struct nstate *nexta;
    struct nstate *nextb;
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
	    split = create_nstate(SPLIT, l.head, r.head);
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
		    split = create_nstate(SPLIT, l.head, psplit);
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
		    split = create_nstate(SPLIT, l.head, psplit);
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

struct nstate* create_nft(struct node *root) {
    struct nstate *final = create_nstate(FINAL, NULL, NULL);
    struct nchunk ch = nft(root, 0);
    ch.tail->nexta = final;
    return ch.head;
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
		default:	break;
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
    char c;
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

struct str * str_append(struct str *s, char c) {
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

struct str * str_append_str(struct str *dst, struct str *src) {
    for(struct str_item *si = src->head; si != NULL; si = si->next) {
    	str_append(dst, si->c);
    }
    return dst;
}

struct str * str_copy(struct str* src) {
    struct str *dst = str_create();
    for(struct str_item *si = src->head; si != NULL; si = si->next) {
    	str_append(dst, si->c);
    }
    return dst;
}

struct slist {
    struct nstate *state;
    struct str *suffix;
    struct slist *next;
};

struct slist * slist_create(struct nstate *state, struct str *suffix) {
    struct slist *sl;
    sl = malloc(sizeof(struct slist));
    sl->state = state;
    sl->suffix = suffix; //str_create();
    sl->next = NULL;
    return sl;
}

struct slist *slist_append(struct slist *sl, struct nstate *state, struct str *suffix) {
    struct slist *sl_next = slist_create(state, suffix);
    sl->next = sl_next;

    return sl_next;
}


struct slist * nft_step(struct nstate *s, struct str *o, unsigned char c, struct slist *sl) {
    /* Given the nstate and the corresponding suffix traverse
     * the nft automaton to reach the CONS states. Keep track
     * of all the characters produced by PROD states.
     *
     * Returns a list of nstates each with corresponding prefix
     *
     */

    // todo: change to the number of states
    struct nstate *state_stack[1024];
    struct str *str_stack[1024];
    struct nstate **sp = state_stack;
    struct str **cp = str_stack;

    push(sp, s);
    push(cp, o);


    while (sp != state_stack) { 	// while not empty
        s = pop(sp);
        o = pop(cp);

        if (!s) continue;

	switch(s->type) {
	    case SPLIT:
		push(cp, o);
		push(sp, s->nextb);
		push(cp, str_copy(o));
		push(sp, s->nexta);
		break;
	    case SPLITNG:
		push(cp, o);
		push(sp, s->nexta);
		push(cp, str_copy(o));
		push(sp, s->nextb);
		break;
	    case JOIN:
		push(cp, o);
		push(sp, s->nexta);
		break;
	    case PROD:
		push(cp, str_append(o, s->val));
		push(sp, s->nexta);
		break;
	    case CONS:	// found CONS state marked with 'c'
		if (c == s->val) {
		    sl = slist_append(sl, s, o);
		}
		break;
	    case FINAL:
		sl = slist_append(sl, s, o);
	    	break;
	}
    }

    return sl;
}


struct dstate {
    struct slist *states;
    struct str *out[256];
    struct dstate *next[256];
};

struct dstate * dstate_create(struct slist *states) {
    struct dstate *ds;
    ds = malloc(sizeof(struct dstate));
    ds->states = states;
    memset(ds->next, 0, sizeof ds->next);
    memset(ds->out, 0, sizeof ds->out);
    return ds;
}

int infer_dft(struct dstate *ds, unsigned char *inp) {
    struct dstate *ds_next;
    struct str *out;
    struct slist *sl, *sl_head;

    for(unsigned char *c=inp; *c != '\0'; c++) {
	if (ds->next[*c] != NULL) {
	    ds_next = ds->next[*c];
	    // put output to a buffer
	    str_append_str(out, ds->out[*c]);
	}
	else if (ds->out[*c] == NULL) {				/* not explored */
	    sl_head = slist_create(NULL, NULL);			/* dummy node */
	    sl = sl_head;

	    /* expand each state and accumulate CONS states labeled with c */
	    for(struct slist *li=ds->states; li; li=li->next) {
		/* return a pointer to the latest item */
		sl = nft_step(li->state, str_copy(li->suffix), *c, sl);
	    }
	    if (sl_head == sl) {				/* got empty list; mark as explored and exit */
	    	ds->out[*c] = (struct str*)1;			/* fixme: can we create a better indicator? */
	    	return 0;
	    }

	    //prefix = lcp (sl_next);

	    /* lookup in the dstate cache */
	    /* if found
	     * 	- assign transition state
	     * 	- assign transition string
	     * 	- move to this state (ds = ds_next)
	     * else
	     *  - create a dstate with a corresponding set of states and suffixes (slist)
	     *  - put the dstate to the dcache using slist as a key
	     * 	- move to this state (ds = ds_next)
	     */

	    if (0) { //(ds_next = btree_lookup(dcache, sl_head->next)) != NULL) {
	    	ds->next[*c] = ds_next;
	    	ds->out[*c] = NULL; //prefix;
	    } else {
	    	ds_next = dstate_create(sl_head->next);
	    	ds->next[*c] = ds_next;
	    	ds->out[*c] = NULL; //prefix;
	    }
	    ds = ds_next;

	}
    }
    return 0;
}


int main(int argc, char **argv)
{
    FILE *fp;
    char *expr;
    ssize_t read, ioffset;
    size_t input_len;
    char *line = NULL, *input_fn, *ch;
    struct node *root;
    struct nstate *start;
    struct sstack *stack = screate(32);
    enum infer_mode mode = SCAN;


    int opt, debug=0;

    while ((opt = getopt(argc, argv, "dm")) != -1) {
	switch (opt) {
	    case 'd':
		debug = 1;
		break;
	    case 'm':
		mode = MATCH;
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

    expr = argv[optind];
    root = parse(expr);

    if (debug) {
	plot_ast(root);
	//plot_nft(start);
    }

    start = create_nft(root);
    //printf("%c\n", start->type);

    // todo: can we do better?
    output = malloc(output_capacity*sizeof(char));

    struct slist *slist_init = slist_create(start, str_create());
    struct dstate *dstart = dstate_create(slist_init);

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
	    stack->n_items = 0; 	// reset the stack; do not shrink the capacity
	    line[read-1] = '\0';
	    ch = line;

	    while (*ch != '\0') {
		ioffset = infer_dft(dstart, (unsigned char*)ch);
		if (ioffset > 0)
		    ch += ioffset;
		else
		    fputc(*ch++, stdout);
	    }
	    fputc('\n', stdout);

	    /*
	    infer_backtrack(start, line, stack, mode);
	    */
	}
    } else {	/* MATCH mode and generator */
	while ((read = getline(&line, &input_len, fp)) != -1) {
	    stack->n_items = 0; 	// reset the stack; do not shrink the capacity
	    line[read-1] = '\0';
	    ioffset = infer_dft(dstart, (unsigned char*)line);
	    //fputc('\n', stdout);
	}
    }

    fclose(fp);
    if (line)
        free(line);
    return 0;
}

