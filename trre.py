# DOUBLE-E algorithm adoptation
# original description: https://github.com/erikeidt/erikeidt.github.io/blob/master/The-Double-E-Method.md

# parser todo:
# []
# eps

prec = {'|': 1, '-': 2, '.': 2, ':': 3, '?': 4, '+': 4, '*': 4, 'I': 4, '\\': 5, '(': -1, '[': -1}

class Node(object):
    def __init__(self, type_, val=0, left=None, right=None):
        self.type_ = type_
        self.val = val
        self.left = left
        self.right = right


def reduce(opr, opd):
    op = opr.pop()
    if op in '|.:-':
        r = opd.pop()
        l = opd.pop()
        opd.append(Node(op, 0, l, r))
    elif op in '*?+':
        l = opd.pop()
        opd.append(Node(op, 0, l))
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


def parse_square_brackets(expr, i, opr, opd):
    state = 0

    while (i < len(expr)):
        t = expr[i]
        if state == 0:              # expect operand
            if t in ':-[]':
                raise SyntaxError("square brackets: unexpected symbol: {}".format(t))
            else:
                opd.append(Node('c', t))       # push operand
                state = 1
        else:                       # expect operator
            if t in ':-':
                reduce_op(t, opr, opd)
                state = 0
            elif t == "]":
                while opr and opr[-1] != '[':
                    reduce(opr, opd)
                opr.pop()           # remove [ from the stack
                return i
            else:                   # implicit alternation
                reduce_op('|', opr, opd)
                i -= 1
                state = 0
        i+=1

    raise SyntaxError("square brackets: unmached square brackets")

def parse(expr):
    opr = []
    opd = []
    state = 0
    count_iter = 0

    i = 0
    while (i < len(expr)):
        t = expr[i]

        if state == 0:                          # expect operand
            if t in '(':
                opr.append(t)
            elif t in '[':
                opr.append(t)
                i = parse_square_brackets(expr, i+1, opr, opd)
                state = 1
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
        i += 1

    # print (opr, opd)  #debug
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
    elif node.type_ == '-':
        if node.left.type_ == 'c' and node.right.type_ == 'c':
            lstate, _ = nft(node.left, mode)
            rstate, _ = nft(node.right, mode)
            join = NFTState('JOIN')
            for i in range(l
                split = NFTState('SPLIT', nexta=None, nextb=head)
            split.nexta = join

        elif node.left.type_ == ':' and node.right.type_ == ':':
            pass
        else:
            raise SyntaxError("Unexpected range syntax")
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

        if state.type_ == 'CONS':
            if i < len(input) and state.val == input[i]:
                i += 1
                state = state.nexta
            else:
                state = None
        elif state.type_ == 'PROD':
            output[o] = state.val
            o+=1
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
                return (output[:o])
            state = None


"""
init_dft_state = DftState((None, ''))

dft_state <- init_dft_state

for c in string:
    if c in next(dft_state):
        if we have next dft state by label c, move to it
    else:
        take next step in NFT and get the set of NFT states
        if set of states already exists in DFT
            - if so, lookup for this next state
            - else
                - create a new one
                - compute the longest common prefix (lcp)
                - put the created state in the dft cache
        link current dft state to the next dft state and use lcp as output label for transition , e.g.
        dft_state[c] = (new_dft_state, lcp)
        move to the next DFT state, e.g. dft_state = dft_state[c]

return output

"""

def step_nft(states, c):
    S = list(states)
    output = []
    visited = set()

    while S:
        s, o = S.pop(0)
        if s is None:
            continue

        if s.type_ == "SPLIT":
            S.append((s.nexta, o))
            S.append((s.nextb, o[:]))
        elif s.type_ == "JOIN":
            S.append((s.nexta, o))
        elif s.type_ == "PROD":
            S.append((s.nexta, o+s.val))
        elif c != "" and s.type_ == "CONS":
            if c == s.val and s not in visited:
                output.append((s, o))
                visited.add(s)
        elif c == "" and s.type_ == "FINAL":        # closure
            output.append((s, o))


    return output


def longest_common_prefix(strs):
    if not strs:
        return ""

    prefix = strs[0]  # Start with the first string as the initial prefix

    for string in strs[1:]:
        # Reduce the prefix until it matches the start of the current string
        while not string.startswith(prefix) and prefix:
            prefix = prefix[:-1]  # Remove the last character from prefix

    return prefix


class DftState(object):
    def __init__(self, states):
        self.states = states
        self.next = {}
        self.final = None
        self.final_out = ""


def infer_dft(input, dft):
    # dft - dictionary of dft states:
    # key:      set of reachable nft states
    # value:    set of outputs for each nft state,
    #           dictionary [0..255] -> dft state

    dft_state = dft[()]
    out = ""

    for c in input:
        if c in dft_state.next:
            # print ("hit", c)
            dft_state, oc = dft_state.next[c]
            if dft_state is None:
                return None
        else:
            # print ("miss", c)
            states = step_nft(dft_state.states, c)
            if not states:
                dft_state.next[c] = (None, '')         # can't make a step
                return None

            id_ = tuple(sorted(set(id(s) for s, _ in states)))
            oc = longest_common_prefix([o for _,o in states])

            if id_ and id_ in dft:
                next_dft_state = dft[id_]
            else:
                next_dft_state = DftState(tuple((s.nexta, o[len(oc):]) for s, o in states))
                dft[id_] = next_dft_state


            dft_state.next[c] = (next_dft_state, oc)
            dft_state = next_dft_state

        out += oc

    if dft_state.final is None:
        # print("miss final")
        states = step_nft(dft_state.states, "")   # closure
        if states:
            dft_state.final = True
            dft_state.final_out = states[0][1]

    if dft_state.final:
        return out + dft_state.final_out
    else:
        return None


#expr = "(a:x|a:y)*c"
expr = "[a-c]*"
root = parse(expr)
start = create_nft(root)
out = infer_dfs(start, 'acaaa')
print (out)

#traverse(root)

'''
dft = {}
dft_start = DftState(states=[(start,'')])
dft[()] = dft_start

out = infer_dft('bc', dft)
print (out)
out = infer_dft('abbbbbbc', dft)
print (out)
print (dft)
'''
