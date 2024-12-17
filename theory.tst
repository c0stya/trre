= Transductive regular expression

== Intro

The idea behind *trre* is very similar to regular expressions (*re*). But where *re* defines a regular language the *trre* defines a binary relation between two regular languages. To check if a string corresponds to a given regular expression normally we construct an abstract machine we call finite state automaton or finite state acceptor (FSA). There is an established formal correspondence between the class of *re* and the class of FSA. For each expression in *re* we can construct an automaton. And for each automaton we can compose an expression in *re*.

For *trre* the class of FSA is not enough. Instead we use finite state transducer (FST) as underlying matching engine. It is similar to the acceptor but for each transition we have two labels. One defines an input character and the other defines output. The current article establishes correspondence between *trre* and FST similar to *re* and FSA.

To do this we need to define the trre language formaly and then prove that for every *trre* expression there is a corresponding FST and vice versa. But first, let's define the *re* language and then extend it to the *trre*.

== RE language specification

*Definition*. Let $cal(A)$ be a finite set augmented with symbols $emptyset$ and $epsilon$. We call it an alphabet. Then *RE* language will be set of words over the alphabet $cal(A)$ and symbols $+, dot.op, *$ defined inductively:

1. symbols $epsilon, emptyset, "and" a in cal(A)$ are regular expressions
2. for any regular expressions $r$ and $s$ the words $r + s$, $r dot.op s$ and $r\*$ are regular expressions.

*Definition*. For a given regular expression $r$ the language of $r$ denoted as $L(r)$ is a set (may be infinite) of finite words defined inductively:

1. for trre expressions $epsilon, emptyset, a in cal(A)$
    - $L(epsilon) = {epsilon}$
    - $L(emptyset) = emptyset$
    - $L(a) = {a}$
2. for regular expressions $r, s$
    - $L(r + s) = L(r) union L(s)$
    - $L(r dot.op s) = L(r) dot.op L(s) = {a dot.op b | a in L(r), b in L(s)} $
    - $L(r \*) = L(r)\* = sum_(n >= 0) r^n = epsilon + r + r dot.op r + r dot.op r dot.op r + ... $

The operation $r\*$ we call Kleene star or Kleene closure.

*Examples*.
    - $(a+b)c$
    - $a*b*$

== TRRE language specification

*Definition*. Let $R$ be a set of regular expressions over alphabet $cal(A)$ and $S$ be a  set of regular expressions over alphabet $cal(B)$. We refer to $cal(A)$ as input alphabet and $cal(B)$ as output alphabet. Transductive regular expressions `trre` will be a pair of regular expression combined with a delimiter symbol ':':
1. for regular expressions $r in R, s in S$, the string $r:s$ is a transductive regular expression
2. for transductive regular expressions $t, u$ expressions $t dot.op u$, $t + u$, $t\*$ are transductive regular expressions

*Definition*. For a given transductive regular expression $t$ the language of $t$ denoted as $L(t)$ is a set of *pairs* of words defined inductively:

1. for a transductive regular expression $r:s$ where r, s are the regular expressions the language $L(r:s) = L(r) times L(s) = { (a,b) | a in L(r), b in L(s) }$, i.e. the direct product of languages $L(r)$ and $L(s)$.
2. for transductive regular expression $t, u$:
    - $L(t dot.op u) = L(t) dot.op L(u) = { (a dot.op b, x dot.op y ) | (a,x) in L(t), (b,y) in L(u) } $
    - $L(t + u) = L(t) union  L(u) $
    - $L(t\*) = L(t)\* =  sum_(n >= 0) t^n$.

== Some properties of TRRE language

1. $a:b = a:epsilon dot.op epsilon:b = epsilon : b dot.op a : epsilon$

Proof:
    $ L(a:b) & = {(u,v) | u in L(a), v in L(b)}  & "by definition of" a:b "language" \
    & = {(u dot.op epsilon), (epsilon dot.op v) | u in L(a), v in L(b)}  \
    & = {(u dot.op epsilon), (epsilon dot.op v) | (u, epsilon) in L(a:epsilon), v in L(epsilon:b)} \
    & = L(a:epsilon) dot.op L(epsilon:b) & "by definition of concatenation" \
    & = L(a:epsilon dot.op epsilon:b) $

2. $(a + b):c = a dot.op c + b dot.op c $

Proof:
    $ L((a + b):c) & = L(a + b) times L(c) & "by definition of" a:b "language" \
    & = (L(a) union L(b)) times L(c) & "by definition of" a + b "language" \
    & = L(a) times L(c) union L(b) times L(c) & "cartesian product of sets is distributive over union" \
    & = L(a:c) union L(b:c) \
    & = L(a:c + b:c) $

3. $a:(b + c) = a dot.op b + a dot.op c $

Proof: same as in 2

4. $a\*:epsilon = (a : epsilon)\* $

Proof:
    $ a\*:epsilon & = ( sum_(n>=0) a^n ) : epsilon  	& "by definition of" \* \
    & = sum_(n>=0) (a^n: epsilon ) & "by Property 2" \
    & = (a:epsilon)* & "by definition of" \* $

5. (a : epsilon)\* = $a\*:epsilon $

Proof: same as in 4

== trre normal form

Let's introduce a simplified version of TRRE language. We will call it normal form of TRRE.

*Definition*. Let $cal(A), cal(B)$ be the alphabets. Then normal form is any string defined recursively:

1. any string of the form $a:b$ is the normal form, where $a in cal(A) union {epsilon}, b in cal(B) union {epsilon}$
2. any string of the form $A:B, A+B, A\* $ are normal forms, where $A,B$ are normal forms

It is clear that any normal is a transductive expression. Let's show that the inverse is true as well.

*Lemma 1.* Every trre expression can be converted to a normal form.

*Proof:* First, let's show that any trre expressions of the form $a:epsilon$ and $epsilon:b$ where a,b are valid regular expressions can be converted to a normal form. Let's prove it for $A:epsilon$ using structural induction on the *RE* construction.

_Base_: $a:epsilon$, $epsilon:epsilon$ are in the normal form.

_Step_: Let's assume that all the subformulas of the formula $a:epsilon$ can be converted to normal form. Then let's prove that $(a + b) : epsilon, a dot.op b : epsilon, a*:epsilon$ can be converted to normal form:
    1. $(a + b):epsilon = a:epsilon + b:epsilon$ by Property 2.
    2. $a dot.op b : epsilon = a:epsilon dot.op b:epsilon$ by Property 1.
    3. $(a \*:epsilon) = (a:epsilon)\*$ by Property 4.

For the case $epsilon:b$ the proof is similar.
So, knowing that any formula $A:B$ can be represented as $A:epsilon dot.op epsilon:B$ we can represent any TRRE of the form "A:B" in the normal form. The last step is to prove that any TRRE formula can be represented as a normal form. Using current fact as a base of induction we can prove it by structural induction on the recursive definition of TRRE.

== trre and automata equivalence

The final step is to show that for any TRRE there is corresponding FST which defines same language. And for any FST there is TRRE which defines same language.

*Theorem*.  $ forall t in "TRRE" exists "FST" a "such" cal(L)(t) = cal(L)(a) $  and
	$ forall "FST" a exists "TRRE" t "such" cal(L)(a) = cal(L)(t) $

*Proof $arrow.r$*

The proof follows classical Thompson's construction algorithm. The only difference is that we represent the transducer as an automaton over the alphabet $cal(A) union {epsilon} times cal(B) union {epsilon}$.

*Proof $arrow.l$*

The proof is a classical Kleene's algorithm for converting automaton to regular expression. The difference here is that we represent transductive regular expression as regular expression by first expressing TRRE in normal form (by Lemma 1) and then treating each transductive pair of the form $a:b$ as a symbol in the new alphabet. The rest of the proof is the same.

== Inference

Like in many modern re engines we construct a deterministic automata on the fly. The difference here is that we construct deterministic FST whenever it is possible.
