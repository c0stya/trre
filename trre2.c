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
    	case 'g':		return 4;
	case '?': case '*':
	case '+': case 'I': 	return 5;
	case '\\':		return 6;
    }
    return -1;
}

struct node {
    char type;
    char val;
    struct node * l;
    struct node * r;
};

#define push(stack, item) 	*stack++ = item
#define pop(stack) 		*--stack
#define top(stack) 		*(stack-1)

static char operators[1024];
static struct node *operands[1024];

static char *opr = operators;
static struct node **opd = operands;


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
struct node * create_nodev(char type, char val) {
    struct node *node = create_node(type, NULL, NULL);
    node->val = val;
    return node;
}

enum infer_mode {
    MODE_SCAN,
    MODE_MATCH,
};


void reduce() {
    char op;
    struct node *l, *r, *n;

    op = pop(opr);
    switch(op) {
	case '|': case '.': case ':': case '-':
	    r = pop(opd);
	    l = pop(opd);
	    push(opd, create_node(op, l, r));
	    break;
	case '*': case '+': case '?': case 'g':
	    l = pop(opd);
	    push(opd, create_node(op, l, NULL));
	    break;
	case 'I':
	    // TODO: implement it
	    r = pop(opd);
	    l = pop(opd);
	    //c = pop(opd);
	    n = create_node(op, l, NULL);
	    //n->val = l;
	    push(opd, n);
	    break;
	case '(':
	    fprintf(stderr, "error: unmached parenthesis\n");
	    exit(EXIT_FAILURE);
    }
}


void reduce_op(char op) {
    while(opr != operators && prec(top(opr)) > prec(op))
        reduce();
    push(opr, op);
}

char* parse_curly_brackets(char *expr) {
    int state = 0;
    int count_iter = 0;
    char c;

    while ((c = *expr) != '\0') {
        if (c >= '0' && c <= '9') {
            count_iter = count_iter*10 + c;
	} else if (c == ',') {
            push(opd, create_nodev('L', count_iter));
            count_iter = 0;
            state += 1;
	} else if (c == '}') {
            if (state == 0)
		push(opd, create_nodev('L', count_iter));
	    push(opd, create_nodev('R', count_iter));
            push(opr, 'I');
            return expr;
        } else {
	    fprintf(stderr, "error: unexpected symbol in curly brackets: %c", c);
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
    char c;
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
		    push(opd, create_nodev('a', 0));
		    state = 1;
		    break;
		case ':':					// epsilon as an implicit left operand
		    push(opd, create_nodev('e', c));
		    state = 1;
		    continue;					// stay in the same position in expr
		case '|': case '*': case '+': case '?':
		case ')': case '{': case '}':
		    fprintf(stderr, "error: unexpected symbol %c\n", c);
		    exit(EXIT_FAILURE);
		default:
		    if (opr != operators && top(opr) == ':') { 	// epsilon as an implicit right operand
			push(opd, create_nodev('e', c));
			state = 1;
			continue;				// stay in the same position in expr
		    }
		    //printf("%c\n", c);
		    push(opd, create_nodev('c', c));
		    state = 1;
            }
	} else {               					// expect postfix or binary operator
	    switch (c) {
	    case '*': case '+':
                reduce_op(c);
                break;
	    case '?':
                switch(*(expr-1)) {
		    case '*': case '+': case '?': case '}':
			reduce_op('g');
			break;
		    default:
			reduce_op(c);
		}
            	break;
            case '|':
                reduce_op(c);
                state = 0;
                break;
            case ':':
                if (*(expr+1) == '\0') {              // implicit epsilon as a right operand
                    push(opd, create_nodev('e', c));
		    reduce_op(c);
		    state = 0;
		}
		break;
            case '{':
                expr = parse_curly_brackets(expr+1);
                state = 1;
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


enum nstate_type {
    PROD,
    CONS,
    SPLIT,
    SPLITG,
    JOIN,
    FINAL,
    ITER
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


struct nchunk nft(struct node *node, char mode, char greed) {
    struct nstate *split, *psplit, *join;
    struct nstate *cstate, *pstate, *state;
    struct nchunk l, r;
    int llv, lrv, rlv;

    if (node == NULL)
    	return chunk(NULL, NULL);

    switch(node->type) {
	case '.':
	    l = nft(node->l, mode, 0);
	    r = nft(node->r, mode, 0);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '|':
	    l = nft(node->l, mode, 0);
	    r = nft(node->r, mode, 0);
	    split = create_nstate(SPLITG, l.head, r.head);
	    join = create_nstate(JOIN, NULL, NULL);
	    l.tail->nexta = join;
	    r.tail->nexta = join;
	    return chunk(split, join);
	case '*':
	    l = nft(node->l, mode, 0);
	    split = create_nstate(SPLITG ? SPLIT : greed, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(split, split);
	case '?':
	    l = nft(node->l, mode, 0);
	    join = create_nstate(JOIN, NULL, NULL);
	    split = create_nstate( SPLITG ? SPLIT : greed, join, l.head);
	    l.tail->nexta = join;
	    return chunk(split, join);
	case '+':
	    l = nft(node->l, mode, 0);
	    split = create_nstate(SPLITG ? SPLIT : greed, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(l.head, split);
	case 'g':
	   return nft(node->l, mode, 1);
	case ':':
	    if (node->l->type == 'e') // implicit eps left operand
	    	return nft(node->r, 2, 0);
	    if (node->r->type == 'e')
		return nft(node->l, 1, 0);

	    // implicit eps right operand
	    l = nft(node->l, 1, 0);
	    r = nft(node->r, 2, 0);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '-':
	    if (node->l->type == 'c' && node->r->type == 'c') {
		join = create_nstate(JOIN, NULL, NULL);
		psplit = NULL;
		for(char c=node->r->val; c >= node->l->val; c--){
		    l = nft(create_nodev('c', c), mode, 0);
		    split = create_nstate(SPLITG, l.head, psplit);
		    l.tail->nexta = join;
		    psplit = split;
		}
		return chunk(psplit, join);
	    } else if (node->l->type == ':' && node->r->type == ':') {
		join = create_nstate(JOIN, NULL, NULL);
		psplit = NULL;

		llv = node->l->l->val;
		lrv = node->l->r->val;
		rlv = node->r->l->val;
		for(char c=rlv-llv; c >= 0; c--) {
		    l = nft(create_node(':',
		    	    create_nodev('c', llv+c),
		    	    create_nodev('c', lrv+c)
		    	    ), mode, 0);
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
	/*
	case 'I':

	    lb, rb = node.val
	    head = tail = NFTState('JOIN')
	    for i in range(0, lb):
		l, r = nft(node.left, mode)
		tail.nexta = l
		tail = r
	    if rb == 0:
		l, r = nft(Node('*', 0, left=node.left), mode, greed)
		tail.nexta = l
		tail = r
	    else:
		final = NFTState('JOIN')
		for i in range(lb, rb):
		    l, r = nft(node.left, mode)
		    s = NFTState('SPLITG' if greed else 'SPLIT', nexta=final, nextb=l)
		    tail.nexta = s
		    tail = r
		tail.nexta = final
		tail = final

	    return head, tail
	*/
	case 'a':
	    return nft(create_node('-',
	    	    create_nodev('c', 0), create_nodev('c', (char)255)),
	    	    mode, 0);

	default: 	 	//character
	    if (mode == 0) {
		cstate = create_nstate(CONS, NULL, NULL);
		pstate = create_nstate(PROD, NULL, NULL);
		cstate->val = node->val;
		pstate->val = node->val;
		cstate->nexta = pstate;
		return chunk(cstate, pstate);
	    }
	    else if (mode == 1) {
		state = create_nstate(CONS, NULL, NULL);
		state->val=node->val;
	    } else {
		state = create_nstate(PROD, NULL, NULL);
		state->val=node->val;
	    }
	    return chunk(state, state);
    }
}


int main(int argc, char **argv)
{
    FILE *fp;
    char *expr;
    ssize_t read;
    char *line = NULL, *input_fn;
    struct node *root;
    enum infer_mode mode = MODE_SCAN;


    int opt, debug=0;

    while ((opt = getopt(argc, argv, "dg")) != -1) {
	switch (opt) {
	    case 'd':
		debug = 1;
		break;
	    case 'g':
		mode = MODE_MATCH;
		break;
	    default: /* '?' */
		fprintf(stderr, "Usage: %s [-d] [-g] expr [file]\n",
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

    printf("%c", root->type);

    /*
    if (optind == argc - 2) {		// filename provided
	input_fn = argv[optind + 1];

	fp = fopen(input_fn, "r");
	if (fp == NULL) {
	    fprintf(stderr, "error: can not open file %s\n", input_fn);
	    exit(EXIT_FAILURE);
	}
    } else
    	fp = stdin;

    while ((read = getline(&line, &input_len, fp)) != -1) {
    	stack->n_items = 0; 					// reset the stack; do not shrink the capacity
        line[read-1] = '\0';
	infer_backtrack(state, line, stack, infer_mode);
    }

    fclose(fp);
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
    */

    return 0;
}


