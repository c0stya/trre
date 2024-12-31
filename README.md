# TRRE: transductive regular expressions

## Intro

Regular expressions is a great tool for searching patterns in text. But I always found it unnatural for text editing. You have to use groups or meta-regex techniques. Here I propose a simple extension to the regular expression language for the purpose of patter matching and text modification. I call it transductive regular expressions or simply `trre`.

It is similar to the classical regexp with addtional symbol ":". So the simplest form of trre is "A:B" where A, B are strings. E.g. 'a:b' will translate symbol 'a' into symbol 'b'. I call this pair a `transductive pair` or `transduction`.

Let's see more examples.

## Examples

To change `cat` to `dog` we can write the following transductive regular expression:

```bash
$ trre 'c:da:ot:g' 'cat'
dog
```

We can write the same in more intuitive way:

```bash
$ trre '(cat):(dog)' 'cat'
dog
```

Next, we can use it like sed to scan through string and replace the match:

```bash
$ trre '(lamb):(cat)' 'Mary had a little lamb.'
Mary had a little cat
```

Let's delete something:

```bash
$ '(bob:)cat' 'bobcat'
cat
```

I used the construction of `(bob:)` to translate `bob` to the empty string.

Let's insert something:

```bash
$ '(:bob)cat' 'cat'
bobcat
```

I used the construction of `(:bob)` to translate `bob` to the empty string.

```bash
$ 'cat:bobcat' 'cat'
bobcat
```

Ok. Now let's add regular expression over the transductions:

```bash
$ trre '(cat:dog)+' 'catcatcat'
dogdogdog

```bash
$ trre '(cat)+:(dog)' 'catcatcat'
dog
```

It would take some time to get comfortable with `trre`. But the logic behind is simple and straightforward.

Alteration over `trre`:

```bash
$ trre 'c:bat|d:hog' cat dog
bat hog 
```

# Example

To find something using TRRE we can scan and delete everything except interting part:

```bash
$ trre '<tag ..>|.:-'
```

or just using default scan

```bash
$ trre -s '<tag ..>'

## Language specification
```

Informally, we define a `trre` as a pair 'pattern_to_search':'pattern_to_replace'. The 'pattern_to_search' can be a constant string or regexp. The 'pattern_to_replace' normally is a constant string. But it can be a regex as well. But it is where things may be comlicated. We come back to this later. Moreover, we can do normal regular expression over these pairs.

Formally, we can specify a smiplified grammar of trre "*|:."

```
trre       <- trre* trre|trre trre.trre
trre       <- regex:regex
regex      <- a a* a.b a|b a*
```

Where `a`,`b` any symbols in the alphabet.

## Under the hood

There is well-known result. For any regular expression there exists an automaton that defines exactly the same language as regular expression. So regex engine use this fact and construct a finite state acceptor and evaluate it against an input string.

Here I follow similar approach. For each transductive regular expression I construct finite state transducer and traverse it against an input string. Well, is it legal? The following small lemma justifies it.

Lemma. For each transductive regular expression R there exists a finite state state transducer T such Language(R) = Language(T). It is true in other direction as well.

Proof. TBD.

## Formalities

Formally, the proposed language is a direct product of a pair of the Kleene algebra with 1 denoted as〈ε,ε〉and 0 denoted as〈∅,∅〉. Which itself is a Kleene algebra.

Some equalities that directly follow from Kleene algebra properties:
```
(a:x)(b:y) = (ab):(xy) = (ab:)(:xy)

(a|b):x = a:x|b:x
a:(x|y) = a:x|a:y

a:x = a:ε ε:x = ε:x a:ε
a*:ε =(ε|a|aa|aa|...):ε = ε:ε|a:ε|aa:ε|aaa:ε|... = ε:ε|a:ε|aa:εε|aaa:εεε|. = (a:ε)*
a*:b = a*:ε ε:b = (a:ε)*(.:b)

(a|b)* = a*|b* (NO!)
```
