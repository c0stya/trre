# TRRE: transductive regular expressions

#### TLDR: It is an extension of the regular expressions for text editing and a `grep`-like command line tool.
#### It is a prototype. Do not use in production.

## Intro

![automata](docs/automata.png)

Regular expressions is a great tool for searching patterns in text. But I always found it unnatural for text editing. The *group* logic works as a post-processor and can be complicated. Here I propose an extension to the regular expression language for pattern matching and text modification. I call it transductive regular expressions or simply `trre`.

It is similar to the normal regex language with addtional of symbol `:`. The simplest form of `trre` is `a:b` where `a`, `b` are characters. It will change symbol `a` to symbol `b`. I call this pair a `transductive pair` or simply `transduction`.

To demonstrate the concept there is a command line tool `trre`. It feels similar to the `grep -E` command.

## Examples

### Basics

To change `cat` to `dog` we can write the following transductive regular expression:

```bash
$ echo 'cat' | trre 'c:da:ot:g'
dog
```

We can write the same in a more readable way:

```bash
$ echo 'cat' | trre '(cat):(dog)'
dog
```

Next, we can use it like `sed` to scan through a string and replace all the matches:

```bash
$ echo 'Mary had a little lamb.' | trre '(lamb):(cat)'
Mary had a little cat.
```

**Deletion:**

```bash
$ echo 'xor' | '(x:)or' 'xor'
cat
```

The expression `(x:)` could be interpreted as of translation of `x` to an empty string.

**Insertion:**

```bash
$ echo 'or' | ./trre '(:x)or'
xor
```

We could think of the expression `(:x)` as of translation of an empty string into `x`.

### Regex over transductions

As for regular expression we could use **alternations** using `|` symbol:

```bash
$ echo 'cat dog' | trre 'c:bat|d:hog'
bat hog
```

or use the **star** over `trre`.

```bash
$ echo 'catcatcat' | trre '(cat):(dog)'
dogdogdog
```

We could use the **star** in the left part of the transductive pair:

```bash
$ echo 'catcatcat' trre '(cat)*:(dog)'
dog
```

### Range transformations


```bash
$ echo "regular expressions" | trre  "[a:A-z:Z]"
REGULAR EXPRESSIONS
```

We could use it to create a toy cipher, e.g. Caesar cipher where we encrypt text by a cyclic shift of the alphabet:

```bash
$ echo 'caesar cipher' | trre '[a:b-y:zz:a]'
dbftbs djqifs
```

and decrypt back:

```bash
$ echo 'dbftbs djqifs' | trre '[a:zb:a-y:x]'
caesar cipher
```

### Generators

Transductive expression can be `non-functional`. It means for any input string we could have multiple output strings. Using the default `scan` mode we search for the first possible string to match a transductive expression.

We can even use pure generator without any imput (or more formally, an empty string as an input).

```bash
$ echo '' | trre -g '(0|1){3}'
000
001
010
011
100
101
110
111
```

We can generate all the subsets as well:

```bash
$ echo '' | ./trre -g ':(0|1){,3}?'

0
00
000
001
01
010
011
1
10
100
101
11
110
111
```

## Language specification

Informally, we define a `trre` as a pair `pattern-to-match`:`pattern-to-generate`. The `pattern-to-match` can be a string or regexp. The `pattern-to-generate` normally is a string. But it can be a `regex` as well. It is where things may be comlicated. We come back to this later. Moreover, we can do normal regular expression over these pairs.

Formally, we can specify a smiplified grammar of `trre` as following:

```
TRRE    <- TRRE* TRRE|TRRE TRRE.TRRE
TRRE    <- REGEX REGEX:REGEX
```

Where `REGEX` is a usual regular expression and `.` stands for concatenation.

For now I consider the operator `:` as non-associative and the expression `TRRE:TRRE` as unsyntactical. There is a natural meaning for this expression as a composition of relations defined by `trre`s. But it can make things too complex. Maybe I change this later.


## Why it works

Under the hood we construct the special automaton that is equivalent to a given transductive expression. The idea behind `trre` is very similar to regular expressions. But there are some key differencies:

* `trre` defines a binary relation on a pair of regular languages
* Instead of `finite state acceptor` (`FSA`) we use `finite state transducer` (`FST`) as underlying inference engine
* Like in many modern `re` engines we construct a deterministic automata on the fly. The difference here is that we construct (non)deterministic `FST` whenever it is possible.

The significant obstacle with non-deterministic FST is that it can not be always determinized. In the worst case the number of states during determinization process grow infinitely. In this case we fall back to the non-deterministic FST (NFT) simulation.

To justify the laguage of trunsductive regular expression we need:

1. Define the `trre` language formaly
2. Set up the correspondance between underlying automata
3. Find efficient inference algorithm to process an input string

The sketch of a proof you can find in this document: [theory.pdf](theory.pdf).

## Design choices and open questions

There are tons of decisions to make:

1. **Associativity of ':' symbol**. I have chosen ':' symbol to make it look more like usual regular expressions. In some cases it is confusing. E.g. expression 'a:b:c' has no intuitive meaning in the current formulation. There can be an alternative syntax using two separate symbols, e.g. '>' and '<' to express the `consuming` and `producing` symbols. Though it would look less `regex` and more `diff`.

2. ':' symbol **precendance**. It is not clear what a priority the symbol should have.

3. Implicit epsilon. I decided to not introduce symbol for an empty string explicitly. Instead, if parser finds empty left or right part of the transductive pair it fills it with empty string. E.g. ':a', 'a|c:' '(cat)*:'.


## Determinization and performance

![determinization](docs/determinization.png)

The important part of the modern regex engines is determinization. This routine converts the non-deterministic automata to the deterministic one. Once converted it has linear time inference on the input string length. It is handy but the convertion is exponential in the worst case. That's why regex engines use on-the-fly determinization where remember the explored deterministic states and put them into a cache.

For `trre` the similar approach is possible. The bad news is that not all the non-deterministic transducers (NFT) can be converted to a deterministic (DFT). In case of two "bad" cycles with same input labels the algorithm is trapped in the infinite loop of a state creation. There is a way to detect such loops but it is expensive (see more in [Allauzen, Mohri, Efficient Algorithms for testing the twins property](https://cs.nyu.edu/~mohri/pub/twins.pdf)).

Alternatively, we can just restrict the number of states in the DFT. In case of overflow we could reset the cache or fall back to the non-deterministic automaton.

The question is should we bother with the rather complex algorithm for transducer determinization. The answer is yes. The deterministic algorithm works 5-20 times faster on large texts. The `trre` implements on-the-fly NFT determinization. But be careful. It is a prototype with possible bugs.

## Installation

There is no package or binary distribution yet. Compile it and run the tests.

```bash
make && sh test.sh
```

Then put it to a binary forlder according to your distro.

## TODO

* Complete the ERE feature set:
    - negation `^` within square brackets
    - character classes
* '$^' symbols for the SCAN mode
* Full unicode support
* Efficient range processing

## References

* The approach is strongly inspired by Russ Cox article: [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
* The idea of transducer determinization comes from this paper: [Cyril Allauzen, Mehryar Mohri, "Finitely Subsequential Transducers"](https://research.google/pubs/finitely-subsequential-transducers-2/)
* Parsing approach comes from [Double-E algorithm](https://github.com/erikeidt/erikeidt.github.io/blob/master/The-Double-E-Method.md) by Erik Eidt which is close to the classical [Shunting Yard algorithm](https://en.wikipedia.org/wiki/Shunting_yard_algorithm).
