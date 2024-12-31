# DOUBLE-E algorithm adoptation
# original description: https://github.com/erikeidt/erikeidt.github.io/blob/master/The-Double-E-Method.md

# parser todo:
# []
# eps

prec = {'|': 1, '-': 2, '.': 2, ':': 3, '?': 4, '+': 4, '*': 4, 'I': 4, '\\': 5, '(': -1}

class Node(object):
    def __init__(self, type_, val=0, left=None, right=None):
        self.type_ = type_
        self.val = val
        self.left = left
        self.right = right


def reduce(opr, opd):
    op = opr.pop()
    if op in '|.:':
        r = opd.pop()
        l = opd.pop()
        # opd.append('(' + op + l + r + ')')
        opd.append(Node(op, 0, l, r))
    elif op in '*?+':
        l = opd.pop()
        opd.append(Node(op, 0, l))
        #opd.append('(' + op +  l + ')')
    if op == 'I':
        rb = opd.pop()
        lb = opd.pop()
        l = opd.pop()
        opd.append(Node(op, (lb.val, rb.val), l))
    elif op == '(':
        raise SyntaxError('unmached parenthesis')


def reduce_op(op, opr, opd):
    while(opr and prec[opr[-1]] > prec[op]):
        reduce(opr, opd)
    opr.append(op)


def parse(expr):
    opr = []
    opd = []
    state = 0
    count_iter = 0

    i = 0
    while (i < len(expr)):
        t = expr[i]

        if state == 0:                          # expect operand
            if t in '([':
                opr.append(t)
            elif t == '\\':
                i += 1
                opd.append(Node('c', expr[i]))
                state = 1
            elif t == '.':
                opd.append(Node('c_any', expr[i]))
                state = 1
            elif t not in '|*:+?()':
                opd.append(Node('c', t))
                state = 1
            else:
                raise SyntaxError("near position {}, token {}".format(i, t))
        elif state ==  1:                                   # expect postfix or binary operator
            if t in '*+?':                    # and prec[t] > prec[opr[-1]]:
                reduce_op(t, opr, opd)
            elif t in '|:':
                reduce_op(t, opr, opd)
                state = 0
            elif t == '{':
                state = 2
            elif t == ')':
                while opr and opr[-1] != '(':
                    reduce(opr, opd)
                if not opr or opr[-1] != '(':
                    raise SyntaxError("unmached parenthesis")
                opr.pop()                       # remove ( from the stack
            else:                               # implicit cat
                reduce_op('.', opr, opd)
                i -= 1
                state = 0
        elif state ==  2:                       # expect curly brackets and its content
            if t >= '0' and t <= '9':
                count_iter = count_iter*10 + int(t)
            elif t == ',':
                opd.append(Node('IL', count_iter))
                count_iter = 0
            elif t == '}':
                opd.append(Node('IR', count_iter))
                opr.append('I')
                count_iter = 0
                state = 1
            else:
                raise SyntaxError("unexpected symbol:", t)
        '''
        elif state ==  3:                       # expect curly brackets and its content
            if t == ':':
            elif t == '-':
            elif t == "]":
                state = 2
            else:
        '''


        i += 1

    print (opr, opd)

    while (opr):
        reduce(opr, opd)

    assert len(opd) == 1

    return opd[0]


def traverse(node):
    if node is None:
        return ''
    return (node.val if node.type_ == 'c' else node.type_) + \
                traverse(node.left) + traverse(node.right)


class NFTState(object):
    def __init__(self, type_, val=0, mode=0, nexta=None, nextb=None):
        self.type_ = type_
        self.val = val
        self.mode = mode
        self.nexta = nexta
        self.nextb = nextb
        self.is_final = False


def nft(node, mode=0, greed=0):
    if node is None:
        return

    if node.type_ == '.':
        lhead, ltail = nft(node.left, mode)
        rhead, rtail = nft(node.right, mode)

        ltail.nexta = rhead
        return lhead, rtail
    if node.type_ == '|':
        lhead, ltail = nft(node.left, mode)
        rhead, rtail = nft(node.right, mode)

        split = NFTState('SPLIT', nexta=lhead, nextb=rhead)
        join = NFTState('JOIN')
        ltail.nexta = join
        rtail.nexta = join
        return split, join
    elif node.type_ == '*':
        head, tail = nft(node.left, mode)
        split = NFTState('SPLIT', nexta=None, nextb=head)
        tail.nexta = split
        return split, split
    elif node.type_ == ':':
        lhead, ltail = nft(node.left, mode=1)
        rhead, rtail = nft(node.right, mode=2)
        ltail.nexta = rhead
        return lhead, rtail
    elif node.type_ == 'I':
        # TODO: complete the section
        '''
        lb, rb = node.val

        #ptail = NFTState('')
        #[s]->[] -> [s] -> []

        phead, ptail = nft(node.left)

        for i in range(rb):
            head, tail = nft(node.left)
            ptail.nexta = head
            ptail = tail
        return phead, ptail
        '''

    elif node.type_ == 'c_any':
        state = NFTState('CONS', val=node.val, mode=mode)
        return state, state
    else: # character
        if mode == 0: # consprod
            cstate = NFTState('CONS', val=node.val)
            pstate = NFTState('PROD', val=node.val)
            cstate.nexta = pstate
            return cstate, pstate
        elif mode == 1:
            state = NFTState('CONS', val=node.val)
        elif mode == 2:
            state = NFTState('PROD', val=node.val)
        return state, state


def create_nft(root):
    final = NFTState('FINAL')
    head, tail = nft(root)
    tail.nexta = final
    return head


def infer_dfs(start, input):
    i, o = 0, 0
    output = ['' for _ in range(1000)]
    stack = []
    state = start

    while (stack or state):
        if state is None:
            state, i, o = stack.pop()

        if state.type_ == 'CHAR':
            if state.mode == 1 and i < len(input) and state.val == input[i]:
                i += 1
            elif state.mode == 0 and i < len(input) and state.val == input[i]:
                output[o] = state.val
                i+=1
                o+=1
            elif state.mode == 2 and i < len(input) and state.val == input[i]:
                output[o] = state.val
                o+=1
            else:
                state = None
                continue
            state = state.nexta
        elif state.type_ == 'CHAR_ANY':
            # todo: incomplete
            if state.mode == 0 and i < len(input):
                output[o] = input[i] #state.val
                i+=1
                o+=1
                state = state.nexta
            else:
                state = None
        elif state.type_ == 'SPLIT':
            stack.append((state.nexta,  i, o))
            state = state.nextb
        elif state.type_ == 'JOIN':
            state = state.nexta
        elif state.type_ == 'FINAL':
            if len(input) == i:
                print(output[:o])
            state = None


'''
Idea: go through all the states simultaneously.
The additional to the NFA inference we have to keep track
of the produced states and output strings pairs.

The naive approach with tracking states and all
possible outputs would require the path copy
for each split.


C <- (init_state, '')
i = 0

while True:

    while C is not empty:    # closure
        s, o = C.pop()

        if s is None:
            continue

        if s == SPLIT:
            C.push(s.nexta, o)
            C.push(s.nextb, copy(o))
        elif s == JOIN:
            C.push(s.nexta, o)
        elif s == PROD:
            C.push(s.nexta, o+s.char)
        elif s == CONS:
            S.push(s, o)

    if i >= len(input):
        return

    visited = set()
    while S:
        s, 0 = pop.S
        if s.char == input[i] and s not in visited:
            C.push(s.next, o)
            visited.add(s)

    i+=1
'''

def infer_bfs(start, input):
    C = [] # closure stack
    S = [] # step stack
    i = 0

    C.append((start, ''))

    while True:

        while C:
            s, o = C.pop()

            if s is None:
                continue

            if s.type_ == "SPLIT":
                C.append((s.nexta, o))
                C.append((s.nextb, o[:]))
            elif s.type_ == "JOIN":
                C.append((s.nexta, o))
            elif s.type_ == "PROD":
                C.append((s.nexta, o+s.val))
            elif s.type_ == "CONS":
                S.append((s, o))
            elif s.type_ == "FINAL":
                if i == len(input):
                    print (o)
                    return
                C.append((s.nexta, o))

        if i >= len(input):
            return

        visited = set()
        while S:
            s, o = S.pop(0)
            if s.val == input[i] and (s not in visited):
                C.append((s.nexta, o))
                visited.add(s)


class dft_state(object)
    def __init__(
        self.next = {}
        self.out = ''


def infer_dft(input, dft):
    init = DFT()
    C = closure()
    i = 0

    dfa = {}
    dft_state = [(id(start), ''),]

    for t in input:
        if in dft_state.next:
            dft_state = dft_state.next[c]
        else:
            states = dft_step(c)


    for i in range():
        if state[0]


expr = "(a:x|a:y)*c"
root = parse(expr)
start = create_nft(root)

traverse(root)
#infer_dfs(start, 'abbbcd')
infer_bfs(start, 'aac')

'''
-a-x-b-y
-a-v-b-w
'''
