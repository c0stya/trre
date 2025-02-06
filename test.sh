#!/bin/bash

cmd_scan="./trre"
cmd_match="./trre -g"

M() {
    local inp="$1"
    local tre="$2"
    local exp="$3"

    res=$(echo ${inp} | $cmd_match ${tre})

    if ! diff <(echo -e "$exp") <(echo -e "$res") > /dev/null; then
	echo -e "FAIL M:" "$inp" "->" "$tre"
	diff <(echo -e "$exp") <(echo -e "$res")
    fi
}

S() {
    local inp="$1"
    local tre="$2"
    local exp="$3"

    res=$(echo ${inp} | $cmd_scan ${tre})

    if ! diff <(echo -e "$exp") <(echo -e "$res") > /dev/null; then
	echo -e "FAIL S:" "$inp" "->" "$tre"
	diff <(echo -e "$exp") <(echo -e "$res")
    fi
}

	# input		# trre			# expected
# basics
M 	"a"		"a:x" 			"x"
M	"ab"		"a:xb:y" 		"xy"
M 	"cat"		"(cat):(dog)"		"dog"
M	"cat"		"c:da:ot:g"		"dog"
M	"mat"		"c:da:ot:g"		""

# alternation
M 	"a"		"a|b|c"			"a"
M 	"b"		"a|b|c"			"b"
M 	"c"		"a|b|c"			"c"

# star
S	"a"		"a*"			"a"
S	"aaa"		"a*"			"aaa"
M	"b"		"a*"			""
M	"bbb"		"a*"			""
M	"abab"		"(ab)*"			"abab"
M	"ababa"		"(ab)*"			""

# basic ranges
M	"a"		"[a-c]"			"a"
M	"b"		"[a-c]"			"b"
M	"c"		"[a-c]"			"c"
M	"d"		"[a-c]"			""

# transform ranges
M	"a"		"[a:x]"			"x"
M	"a"		"[a:xb:y]"		"x"
M	"b"		"[a:xb:y]"		"y"
M	"c"		"[a:xb:y]"		""

# range-transform ranges
M	"a"		"[a:x-c:z]"		"x"
M	"b"		"[a:x-c:z]"		"y"
M	"c"		"[a:x-c:z]"		"z"
M	"d"		"[a:x-c:z]"		""

# any char
M	"a"		"."			"a"
M	"b"		"."			"b"
M	"abc"		"..."			"abc"

# iteration, single
M	"aa"		"a{2}"			"aa"
M	"a"		"a{2}"			""
M	"aaa"		"a{2}"			""

# iteration, left
M	"" 		"a{,2}"			""
M	"a" 		"a{,2}"			"a"
M	"aa" 		"a{,2}"			"aa"
M	"aaa" 		"a{,2}"			""

# iteration, center
M	"" 		"a{1,2}"		""
M	"a" 		"a{1,2}"		"a"
M	"aa" 		"a{1,2}"		"aa"
M	"aaa" 		"a{1,2}"		""

# iteration, right
M	""		"a{2,}"			""
M	"a"		"a{2,}"			""
M	"aa"		"a{2,}"			"aa"
M	"aaa"		"a{2,}"			"aaa"

# generators
S	""		":a{,3}"		"aaa"
M	""		":a{,3}"		"aaa\naa\na"

S	""		":a{,3}?"		""
M	""		":a{,3}?"		"\na\naa\naaa"

# greed modifiers
S	"aaa"		"(.:x)*.*"		"xxx"
S	"aaa"		"(.:x)*?.*"		"aaa"

M	"aaa"		"(.:x)*.*"		"xxx\nxxa\nxaa\naaa"
M	"aaa"		"(.:x)*?.*"		"aaa\nxaa\nxxa\nxxx"

# escapes
S	"."		"\."			"."
S	"+"		"\+"			"+"
S	"?"		"\?"			"?"
S	":"		"\:"			":"
S	".a"		"\.a"			".a"
M	".c"		"[.]c"			".c"

# epsilon
# generators

## iteration, range
#printf "aaa\n"	| $CMD "((a:x){,}.*)"
