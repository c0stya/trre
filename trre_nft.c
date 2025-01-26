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

#define STACK_MAX_CAPACITY	1000000

static char operators[1024];
static struct node *operands[1024];

static char *opr = operators;
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
    struct node *l, *r;

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
	    r = pop(opd);
	    l = pop(opd);
	    l->l = pop(opd);
	    // use right leaf as a placeholder
	    // for the right range value
	    l->r = r;
	    l->type = 'I';
	    push(opd, l);
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
            count_iter = count_iter*10 + c - '0';
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
                if (*(expr+1) == '\0') {		// implicit epsilon as a right operand
                    push(opd, create_nodev('e', c));
                }
		reduce_op(c);
		state = 0;
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


struct nchunk nft(struct node *n, char mode, char greed) {
    struct nstate *split, *psplit, *join;
    struct nstate *cstate, *pstate, *state, *head, *tail, *final;
    struct nchunk l, r;
    int llv, lrv, rlv;
    int lb, rb;

    if (n == NULL)
    	return chunk(NULL, NULL);

    switch(n->type) {
	case '.':
	    l = nft(n->l, mode, 0);
	    r = nft(n->r, mode, 0);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '|':
	    l = nft(n->l, mode, 0);
	    r = nft(n->r, mode, 0);
	    split = create_nstate(SPLIT, l.head, r.head);
	    join = create_nstate(JOIN, NULL, NULL);
	    l.tail->nexta = join;
	    r.tail->nexta = join;
	    return chunk(split, join);
	case '*':
	    l = nft(n->l, mode, 0);
	    split = create_nstate(greed ? SPLITNG : SPLIT, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(split, split);
	case '?':
	    l = nft(n->l, mode, 0);
	    join = create_nstate(JOIN, NULL, NULL);
	    split = create_nstate(greed ? SPLITNG : SPLIT, join, l.head);
	    l.tail->nexta = join;
	    return chunk(split, join);
	case '+':
	    l = nft(n->l, mode, 0);
	    split = create_nstate(greed ? SPLITNG : SPLIT, NULL, l.head);
	    l.tail->nexta = split;
	    return chunk(l.head, split);
	case 'g':
	    return nft(n->l, mode, 1);
	case ':':
	    if (n->l->type == 'e') // implicit eps left operand
	    	return nft(n->r, 2, 0);
	    if (n->r->type == 'e')
		return nft(n->l, 1, 0);
	    // implicit eps right operand
	    l = nft(n->l, 1, 0);
	    r = nft(n->r, 2, 0);
	    l.tail->nexta = r.head;
	    return chunk(l.head, r.tail);
	case '-':
	    if (n->l->type == 'c' && n->r->type == 'c') {
		join = create_nstate(JOIN, NULL, NULL);
		psplit = NULL;
		for(char c=n->r->val; c >= n->l->val; c--){
		    l = nft(create_nodev('c', c), mode, 0);
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
	case 'I':
	    lb = n->val;
	    rb = n->r->val;
	    head = tail = create_nstate(JOIN, NULL, NULL);

	    for(int i=0; i <lb; i++) {
		l = nft(n->l, mode, 0);
		tail->nexta = l.head;
		tail = l.tail;
	    }

	    if(rb == 0) {
		l = nft(create_node('*', n->l, NULL), mode, 0);
		tail->nexta = l.head;
		tail = l.tail;
	    } else {
		final = create_nstate(JOIN, NULL, NULL);
		for(int i=lb; i < rb; i++) {
		    l = nft(n->l, mode, 0);
		    tail->nexta = create_nstate(greed ? SPLITNG : SPLIT, final, l.head);
		    tail = l.tail;
		}
		tail->nexta = final;
		tail = final;
	    }

	    return chunk(head, tail);

	case 'a':
	    return nft(create_node('-',
	    	    create_nodev('c', 0), create_nodev('c', (char)255)),
	    	    mode, 0);

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
    struct nchunk ch = nft(root, 0, 0);
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
    output = (char*)realloc(output, *capacity * sizeof(char));
    if (!output) {
        fprintf(stderr, "error: memory reallocation failed for output\n");
        exit(EXIT_FAILURE);
    }
    return output;
}

// Main DFS traversal function
void infer_backtrack(struct nstate *start, char *input, struct sstack *stack, enum infer_mode mode) {
    size_t i = 0, o = 0;
    struct nstate *s = start;

    while (stack->n_items || s) {
        if (!s) {
            if (!spop(stack, &s, &i, &o)) {
                break; // Stack is empty
            }
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
                if (input[i] == '\0') {
                    output[o] = '\0'; // Null-terminate the output string
                    fputs(output, stdout);
                    fputs("\n", stdout);
                }
                s = NULL;
                break;
            default:
                fprintf(stderr, "error: unknown state type\n");
                exit(1);
        }
    }
}

/*
// Define the structure for the tree node
struct btnode {
    int data;
    struct btnode* l;
    struct btnode* r;
};

// Function to create a new node with given data
struct btnode* create_btnode(int data) {
    struct btnode* node = malloc(sizeof(struct btnode));
    if (node == NULL) {
        printf("error: binary tree node allocation failed.\n");
	exit(EXIT_FAILURE);
    }
    node->data = data;
    node->left = NULL;
    node->right = NULL;
    return newNode;
}

// Lookup function to search for a value in the binary tree
struct btnode* lookup(struct btnode* node, int value) {
    while (node != NULL) {
        if (value < node->data) {
            node = node->left;
        } else if (value > node->data) {
            node = node->right;
        } else {
            return node; // Value found
        }
    }
    return NULL; // Value not found
}

// Function to insert nodes to form a binary search tree
Node* insert(Node* root, int value) {
    if (root == NULL) {
        return createNode(value);
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else if (value > root->data) {
        root->right = insert(root->right, value);
    }
    return root;
}
*/

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


int main(int argc, char **argv)
{
    FILE *fp;
    char *expr;
    ssize_t read;
    size_t input_len;
    char *line = NULL, *input_fn;
    struct node *root;
    struct nstate *start;
    struct sstack *stack = screate(32);
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
    start = create_nft(root);

    start = create_nft(root);

    //plot_nft(start);
    //printf("%c\n", start->type);
    //

    output = malloc(output_capacity*sizeof(char));

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
	infer_backtrack(start, line, stack, mode);
    }

    fclose(fp);
    if (line)
        free(line);
    exit(EXIT_SUCCESS);

    return 0;
}


