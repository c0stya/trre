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
$ echo 'or' | trre '(:x)or'
xor
```

We could think of the expression `(:x)` as of translation of an empty string into `x`.

### Regex over transductions

As for regular expression we could use **alternations** using `|` symbol:

```bash
$ echo 'cat dog' | trre 'c:bat|d:hog'
bat hog
```

or use the **star** over `trre` to infinitely change the corresponding pattern:

```bash
$ echo 'catcatcat' | trre '((cat):(dog))*'
dogdogdog
```

but in the default `scan` mode we could omit the **star**:

```bash
$ echo 'catcatcat' | trre '(cat):(dog)'
dogdogdog
```

We could use the **star** in the left part of the transductive pair to infinitely "consume" corresponding pattern:

```bash
$ echo 'catcatcat' | trre '(cat)*:(dog)'
dog
```

#### Danger zone

Do not use '*' or '+' in the right part because it forces the infinite generation loop:

```bash
$ echo '' | trre ':a*'      # <- do NOT do this
dog
```

Use the finite iteration instead:

```bash
$ echo '' | trre ':(repeat-10-times){10}'
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
$ echo '' | trre -g ':(0|1){,3}?'

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


## Modes and greediness

Current version supports two modes. The default is a *scan* mode where expression is applied to the string sequentially. The second **match** mode check the expression against the whole string. To switch the mode to **matching** use flag `-m`.

To generate all possible output string use the modifier `-a`. It is mostly useful in the **matching** mode.

There are special `?` modifier which applies to `+`,`*`,`{,}` operators. The behavior is the same as in normal regex.

For example, let's remove all the text between "<>" symbols:

```bash
$ echo '<cat><dog>' | trre '<(.:)*>'
<>

$ echo '<cat><dog>' | trre '<(.:)*?>'
<><>
```
The second expression behaves non-greedy.


## Determinization and performance

<img src="docs/determinization.png" width="70%"/>


The important part of the modern regex engines is determinization. This routine converts the non-deterministic automata to the deterministic one. Once converted it has linear time inference on the input string length. It is handy but the convertion is exponential in the worst case. That's why regex engines use on-the-fly determinization where remember the explored deterministic states and put them into a cache.

For `trre` the similar approach is possible. The bad news is that not all the non-deterministic transducers (NFT) can be converted to a deterministic (DFT). In case of two "bad" cycles with same input labels the algorithm is trapped in the infinite loop of a state creation. There is a way to detect such loops but it is expensive (see more in [Allauzen, Mohri, Efficient Algorithms for testing the twins property](https://cs.nyu.edu/~mohri/pub/twins.pdf)).

Alternatively, we can just restrict the number of states in the DFT. In case of overflow we could reset the cache or fall back to the non-deterministic automaton.

The question is should we bother with the rather complex algorithm for transducer determinization. The answer is yes. The deterministic algorithm works 3-10 times faster on large texts. The `trre` implements on-the-fly NFT determinization as a separate binary `trre_dft`. But be careful. It is a prototype with possible bugs.

The NFT (non-deterministic) version is a bit slower then `sed`:

```bash
$ wget https://www.gutenberg.org/cache/epub/57333/pg57333.txt -O chekhov.txt

$ time cat chekhov.txt | ./trre '(vodka):(VODKA)' > /dev/null

real	0m0.046s
user	0m0.043s
sys	0m0.007s

$ time cat chekhov.txt | sed  's/vodka/VODKA/' > /dev/null

real	0m0.024s
user	0m0.020s
sys	0m0.010s

```

The DFT version is faster then `sed` on more complex examples. E.g. let's uppercase the whole text:

```bash
$ time cat chekhov.txt | sed -e 's/\(.*\)/\U\1/' > /dev/null

real	0m0.508s
user	0m0.504s
sys	0m0.015s

$ time cat chekhov.txt | ./trre_dft '[a:A-z:Z]' > /dev/null

real	0m0.131s
user	0m0.127s
sys	0m0.009s
```

It might be not a fair comparison since `trre` do not support unicode. But I have not specially optimized the current version. So I expect there is a huge space for performance improvements.


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
