## Why it works

Under the hood we construct the special automaton that is equivalent to a given transductive expression. The idea behind `trre` is very similar to regular expressions. But there are some different key elements:

* `trre` defines a binary relation on a pair of regular languages
* Instead of `finite state acceptor` (`FSA`) we use `finite state transducer` (`FST`) as underlying inference engine
* Like in many modern `re` engines we construct a deterministic automata on the fly. The difference here is that we construct deterministic `FST` whenever it is possible.

To justify the laguage of trunsductive regular expression we need:

1. Define the `trre` language formaly
2. Set up the correspondance between underlying automata
3. Find efficient inference algorithm to process an input string

### `re` syntax and language specification

First, let's define the `re` language and then extend it to the `trre` language.

**Definition**. Let $\mathbb{A}$ be an finite set augmented with symbols $\emptyset$ and $\varepsilon$. We call it an alphabet. Then `re` language will be set of words over the alphabet $\mathbb{A}^*$ and symbols '$+$', $\cdot$ and $*$ defined inductively:

1. for any symbol $a$ in the alphabet $\mathbb{A}$, $a$ is a regular expression
2. $\varepsilon$ and $\emptyset$ are regular expressions
3. for any regular expressions $r$ and $s$ the words $r+s$, $r \cdot s$ and $r*$ are regular expressions

Note that the we defined an expression so far, i.e. chain of symbols. We need to define the semantics.

**Definition**. For a given regular expression $r$ the language of $r$ denoted as $L(r)$ is a set of finite words defined inductively:

1. for a one-symbol regular expression  $a$ in the alphabet $\mathbb{A}$ its language defined as $L(a) = {a}$
2. for a regular expression $\varepsilon$ its language is $L(\varepsilon) = {\varepsilon}$
3. for a regular expression $\emptyset$ its language is $L(\emptyset) = \emptyset$
4. for regular expressions $r, s$
    $$L(r + s) = L(r) \cup L(s)$$,
    $$L(r \cdot s) = L(r) \cdot L(s)$$,
    $$L(r*) = L(r)* = \sum_{n \geq 0} r^n$$.

### `trre` syntax and language specification

**Definition**. Let $r,s$ be a set of regular expression.  and $S$ be \mathbb{A}$ be an finite set augmented with symbols $\emptyset$ and $\varepsilon$. We call it an alphabet. Then `re` language will be set of words over the alphabet $\mathbb{A}^*$ and symbols '$+$', $\cdot$ and $*$ defined inductively:

1. for regular expressions $r, s$, the string $r:s$ is a transductive regular expression
2. for transductive regular expressions $t, u$
    $t \cdot u$, $t + u$, $t*$ are transductive regular expressions

**Definition**. For a given transductive regular expression $t$ the language of $t$ denoted as $L(t)$ is a set of *pairs* of words defined inductively:

1. for a transductive regular expression $r:s$ where r, s are the regular expressions the language $L(r:s) = L(r) \times L(s) = { (a,b) | a \in L(r), b \in L(s) }, i.e. the direct product of languages L(r) and L(s).
2. for transudctive regular expression $t, u$:
    $L(t \cdot u) = L(t) \cdot L(u) = { (a \cdot b, x \cdot y ) \in L(t), (b,y) \in L(u) }
    $L(t \cup u) = L(t) \cup L(u)
    $L(t*) = L(t)* =  \sum_{n \geq 0} t^n$.


### Automata equivalence

Lemma.  

### Inference

