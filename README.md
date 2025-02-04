# TRRE: transductive regular expressions

#### TLDR: an extension of the regular expressions for text editing and a `grep`-like command line tool
#### It is a prototype. Do not use it in production.

## Intro

Regular expressions is a great tool for searching patterns in text. But I always found it unnatural for text editing. The *group* logic works as a post-processor and can be complicated.

. like in (sed editor)[https://www.gnu.org/software/sed/manual/sed.html]. Here I propose an extension to the regular expression language for pattern matching and text modification. I call it transductive regular expressions or simply `trre`.

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

The construction of `(x:)` could be interpreted as of translation of `x` to an empty string.

**Insertion:**

```bash
$ echo 'or' | ./trre '(:x)or'
xor
```

We could think of the construction `(x:)` as of changing empty string into `x`.

### Regex over transductions

As for regular expression we could use **alternations** using `|` symbol:

```bash
$ echo 'cat dog' | trre 'c:bat|d:hog'
bat hog
```

or use the **star** over `trre`.

```bash
$ trre '((cat):(dog))*' 'catcatcat'
dogdogdog

We could use the **star** in the parser part:

```bash
$ trre '(cat)*:(dog)' 'catcatcat'
dog
```

## Range transformations

```
echo "regular expressions" | trre  "[a:A-z:Z]"
REGULAR EXPRESSIONS
```

We could use it to create a toy cipher, e.g. Caesar cipher where we encrypt text by a cyclic shift of the alphabet:
```
echo "caesar cipher" | trre "[a:b-y:zz:a]"
dbftbs djqifs
```

and decrypt back:

```
echo "dbftbs djqifs" | trre "[a:zb:a-y:x]"
caesar cipher
```

## Generators

Transductive expression can be `non-functional`. It means for any input string we could have multiple output strings. Using the default `scan` mode we search for the first possible string to match a transductive expression.

We can even use pure generator without any imput (or more formally, an empty string as an input).

```
echo "" | trre -g "(0|1){3}"
000
001
010
011
100
101
110
111
```

Or the 

echo "" | ./trre -g ":(0|1){,3}?"



## Language specification

Informally, we define a `trre` as a pair 'pattern_to_search':'pattern_to_replace'. The 'pattern_to_search' can be a constant string or regexp. The 'pattern_to_replace' normally is a constant string. But it can be a regex as well. But it is where things may be comlicated. We come back to this later. Moreover, we can do normal regular expression over these pairs.

Formally, we can specify a smiplified grammar of trre as

```
trre       <- trre* trre|trre trre.trre
trre       <- regex:regex
```

Where `regex` is a usual regular expression.

## Why it works

![automata](docs/automata.png)

Under the hood we construct the special automaton that is equivalent to a given transductive expression. The idea behind `trre` is very similar to regular expressions. But there are some different key elements:

* `trre` defines a binary relation on a pair of regular languages
* Instead of `finite state acceptor` (`FSA`) we use `finite state transducer` (`FST`) as underlying inference engine
* Like in many modern `re` engines we construct a deterministic automata on the fly. The difference here is that we construct (non)deterministic `FST` whenever it is possible.

To justify the laguage of trunsductive regular expression we need:

1. Define the `trre` language formaly
2. Set up the correspondance between underlying automata
3. Find efficient inference algorithm to process an input string

The sketch of a proof you can find in this document: [theory.pdf](theory.pdf). 


## Design choices and open questions

There are tons of decisions to make:

1. **Associativity of ':' symbol**. I have chosen ':' symbol to make it look more like usual regular expressions. In some cases it is confusing. E.g. expression 'a:b:c' has no intuitive meaning in the current formulation. There can be an alternative formulation using two separate symbols, e.g. '>' and '<' to express the `consuming` and `producing` symbols. Though it would look less regex and more `diff`.

2. ':' symbol precendence. Should it be

3. Syntax: implicit epsilon.


## TODO

* Complete the ERE feature set:
    - negation `^` within square brackets
    - character classes
* '$^' symbols for the SCAN mode
* Full unicode support
* Efficient range processing
* Deterministic automaton for the SCAN mode
