# TRRE: transductive regular expressions

#### TLDR: It is an extension of the regular expressions for text editing and a `grep`-like command line tool.
#### It is a PROTOTYPE. Do not use in production.

## Intro

![automata](docs/automata.png)

Regular expressions is a great tool for searching patterns in text. But I always found it unnatural for text editing. The *group* logic works as a post-processor and can be complicated. Here I propose an extension to the regular expression language for pattern matching and text modification. I call it **transductive regular expressions** or simply **`trre`**.

It  introduces the `:` symbol to define transformations. The simplest form is `a:b`, which replaces a with b. I call this a `transductive pair` or `transduction`.

To demonstrate the concept, I have created a command line tool **`trre`**. It feels similar to the `grep -E` command.

## Examples

To run it first build the program:

```bash
make
```

### Basics

To change `cat` to `dog` we use the following expression:

```bash
echo 'cat' | ./trre 'cat:dog'
```
```
dog
```

A more cryptic token-by-token version:

```bash
echo 'cat' | ./trre '(c:d)(a:o)(t:g)'
```
```
dog
```

It can be used like `sed` to replace all matches in a string:

```bash
echo 'Mary had a little lamb.' | ./trre 'lamb:cat'
```
```
Mary had a little cat.
```

**Deletion:**

To delete a string we can use the pattern `string_to_delete:`

```bash
echo 'xor' | ./trre '(x:)or'
```
```
or
```

The expression `(x:)` could be interpreted as of translation of `x` to an empty string.

This expression can be used to delete all the occurances in the default `scan` mode. E.g. expression below removes all occurances of 'a' from a string/

```bash
echo 'Mary had a little lamb.' | ./trre 'a:'
```
```
Mry hd  little lmb.
```

Technically, here we replace character `a` with empty symbol.

To remove several characters from this string we can use square brackets construction similar to normal regex:

```bash
echo 'Mary had a little lamb.' | ./trre '[aie]:'
```
```
Mry hd  lttl lmb.
```

**Insertion:**

To insert a string we can use the pattern `:string_to_insert`

```bash
echo 'or' | ./trre '(:x)or'
```
```
xor
```

We could think of the expression `(:x)` as of translation of an empty string into `x`.

We can insert a word in a context:

```bash
echo 'Mary had a lamb.' | ./trre 'had a (:little )lamb'
```
```
Mary had a little lamb.
```

### Regex over transductions

As for normal regular expression we could use **alternations** with `|` symbol:

```bash
echo 'cat dog' | ./trre '(c:b)at|(d:h)og'
```
```
bat hog
```

Or use the **star** over `trre` to repeat the transformation:

```bash
echo 'catcatcat' | ./trre '(cat:dog)*'
```
```
dogdogdog
```

In the default `scan` mode, **star** can be omitted:

```bash
echo 'catcatcat' | ./trre 'cat:dog'
```
```
dogdogdog
```

You can also use the star in the left part to "consume" a pattern infinitely:

```bash
echo 'catcatcat' | ./trre '(cat)*:dog'
```
```
dog
```

#### Danger zone

Avoid using `*` or `+` in the right part, as it can cause infinite loops:

```bash
echo '' | trre ':a*'      # <- do NOT do this
```
```
...
```

Instead, use finite iterations:

```bash
echo '' | ./trre ':(repeat-10-times){10}'

repeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-timesrepeat-10-times
```

### Range transformations

Transform ranges of characters:

```bash
echo "regular expressions" | trre  "[a:A-z:Z]"
```
```
REGULAR EXPRESSIONS
```

As more complex example, lets create a toy cipher. Below is the Caesar cipher(1) implementation:

```bash
echo 'caesar cipher' | trre '[a:b-y:zz:a]'
```
```
dbftbs djqifs
```

And decrypt it back:

```bash
echo 'dbftbs djqifs' | trre '[a:zb:a-y:x]'
```
```
caesar cipher
```

### Generators

**`trre`** can generate multiple output strings for a single input. By default, it uses the first possible match. You can also generate all possible outputs.

**Binary sequences:**
```bash
echo '' | trre -ma ':(0|1){3}'
```
```

000
001
010
011
100
101
110
111
```

**Subsets:**
```bash
echo '' | trre -ma ':(0|1){,3}?'
```
```

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

Informally, we define a **`trre`** as a pair `pattern-to-match`:`pattern-to-generate`. The `pattern-to-match` can be a string or regexp. The `pattern-to-generate` normally is a string. But it can be a `regex` as well. Moreover, we can do normal regular expression over these pairs.

We can specify a smiplified grammar of **`trre`** as following:

```
TRRE    <- TRRE* TRRE|TRRE TRRE.TRRE
TRRE    <- REGEX REGEX:REGEX
```

Where `REGEX` is a usual regular expression, and `.` stands for concatenation.

For now I consider the operator `:` as non-associative and the expression `TRRE:TRRE` as unsyntactical. There is a natural meaning for this expression as a composition of relations defined by **`trre`**s. But it can make things too complex. Maybe I change this later.


## Why it works

Under the hood, **`trre`** constructs a special automaton called a **Finite State Transducer (FST)**, which is similar to a **Finite State Automaton (FSA)** used in normal regular expressions but handles input-output pairs instead of plain strings.

Key differences:

* **`trre`** defines a binary relation between two regular languages.

* It uses **FST**s instead of **FSA**s for inference.

* It supports on-the-fly determinization for performance (experimental and under construction!).

To justify the laguage of trunsductive regular expression we need to prove the correspondence between **`trre`** expressions and the corresponding **FST**s. There is my sketch of a the proof: [theory.pdf](doc/theory.pdf).

## Precedence

Below is the table of precedence (priority) of the `trre` operators from high to low:

|-----------------------------------|----------------------|
| Escaped characters                | \\                   |
| Bracket expression                | []                   |
| Grouping                          | ()                   |
| Iteration                         | * + ? {m,n}          |
| Concatenation                     |                      |
| **Transduction**                  | :                    |
| Alternation                       | \|                   |


## Modes and greediness

**`trre`** supports two modes:

* **Scan Mode (default)**: Applies transformations sequentially.

* **Match Mode**: Checks the entire string against the expression (use `-m` flag).

Use `-a` to generate all possible outputs.

The `?` modifier makes `*`, `+`, and `{,}` operators non-greedy:

```bash
echo '<cat><dog>' | ./trre '<(.:)*>'
```
```
<>
```

```bash
echo '<cat><dog>' | ./trre '<(.:)*?>'
```
```
<><>
```

## Determinization

<img src="docs/determinization.png" width="80%"/>

The important part of the modern regex engines is determinization. This routine converts the non-deterministic automata to the deterministic one. Once converted it has linear time inference on the input string length. It is handy but the convertion is exponential in the worst case.

For **`trre`** the similar approach is possible. The bad news is that not all the non-deterministic transducers (**NFT**) can be converted to a deterministic (**DFT**). In case of two "bad" cycles with same input labels the algorithm is trapped in the infinite loop of a state creation. There is a way to detect such loops but it is expensive (see more in [Allauzen, Mohri, Efficient Algorithms for testing the twins property](https://cs.nyu.edu/~mohri/pub/twins.pdf)).

## Performance

The default non-deterministic version is a bit slower then `sed`:

```bash
wget https://www.gutenberg.org/cache/epub/57333/pg57333.txt -O chekhov.txt

time cat chekhov.txt | ./trre '(vodka):(VODKA)' > /dev/null
```
```
real	0m0.046s
user	0m0.043s
sys	0m0.007s
```

```bash
time cat chekhov.txt | sed  's/vodka/VODKA/' > /dev/null
```
```
real	0m0.024s
user	0m0.020s
sys	0m0.010s
```

For complex tasks, **`trre_dft`** (deterministic version) can outperform sed:

```bash
time cat chekhov.txt | sed -e 's/\(.*\)/\U\1/' > /dev/null
```
```
real	0m0.508s
user	0m0.504s
sys	0m0.015s
```

```bash
time cat chekhov.txt | ./trre_dft '[a:A-z:Z]' > /dev/null
```
```
real	0m0.131s
user	0m0.127s
sys	0m0.009s
```

## Installation

No pre-built binaries are available yet. Clone the repository and compile:

```bash
git clone git@github.com:c0stya/trre.git trre
cd trre
make && sh test.sh
```

Then move the binary to a directory in your `$PATH`.

## TODO

* Stable *DFT* version
* Full unicode support
* Complete the ERE feature set:
    - negation `^` within `[]`
    - character classes
    - '$^' anchoring symbols
* Efficient range processing

## References

* The approach is strongly inspired by Russ Cox article: [Regular Expression Matching Can Be Simple And Fast](https://swtch.com/~rsc/regexp/regexp1.html)
* The idea of transducer determinization comes from this paper: [Cyril Allauzen, Mehryar Mohri, "Finitely Subsequential Transducers"](https://research.google/pubs/finitely-subsequential-transducers-2/)
* Parsing approach comes from [Double-E algorithm](https://github.com/erikeidt/erikeidt.github.io/blob/master/The-Double-E-Method.md) by Erik Eidt which is close to the classical [Shunting Yard algorithm](https://en.wikipedia.org/wiki/Shunting_yard_algorithm).
