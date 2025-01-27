#!/bin/bash

cmd_scan="./trre"
cmd_match="./trre -m"

assert() {
    local msg=$1
    local act=$2
    local exp=$3

    if [ "$exp" == "$act" ]; then
	echo -e "PASS: $msg \t '$exp' == '$act'"
    else
	echo -e "FAIL: $msg \t '$exp' != '$act'"
    fi
}

T() {
    local input=$1
    local tregex=$2
    local expected=$3
    assert "$input -> $tregex" $(echo $input | $cmd_match $tregex) $expected
}

	# input		# trre			# expected
# basics
T 	"a"		"a:x" 			"x"
T	"ab"		"a:xb:y" 		"xy"
T 	"cat"		"(cat):(dog)"		"dog"
T	"cat"		"c:da:ot:g"		"dog"
T	"mat"		"c:da:ot:g"		""

# alternation
T 	"a"		"a|b|c"			"a"
T 	"b"		"a|b|c"			"b"
T 	"c"		"a|b|c"			"c"

# star
T	"a"		"a*"			"x"
T	"aaa"		"a*"			"xaa"
T	"b"		"a*"			""
T	"bbb"		"a*"			""
T	"abab"		"(ab)*"			"abab"
T	"ababa"		"(ab)*"			""

# basic ranges
T	"a"		"[a-c]"			"a"
T	"b"		"[a-c]"			"b"
T	"c"		"[a-c]"			"c"
T	"d"		"[a-c]"			""

# transform ranges
T	"a"		"[a:x]"			"x"
T	"a"		"[a:xb:y]"		"x"
T	"b"		"[a:xb:y]"		"y"
T	"c"		"[a:xb:y]"		""

# range-transform ranges
T	"a"		"[a:x-c:z]"		"x"
T	"b"		"[a:x-c:z]"		"y"
T	"c"		"[a:x-c:z]"		"z"
T	"d"		"[a:x-c:z]"		""

# any char
T	"a"		"."			"a"
T	"b"		"."			"b"
T	"abc"		"..."			"abc"

# iteration, single
T	"aa"		"a{2}"			"aa"
T	"a"		"a{2}"			""
T	"aaa"		"a{2}"			""

# iteration, left
T	"" 		"a{,2}"			""
T	"a" 		"a{,2}"			"a"
T	"aa" 		"a{,2}"			"aa"
T	"aaa" 		"a{,2}"			""

# iteration, center
T	"" 		"a{1,2}"		""
T	"a" 		"a{1,2}"		"a"
T	"aa" 		"a{1,2}"		"aa"
T	"aaa" 		"a{1,2}"		""

# iteration, right
T	""		"a{2,}"			""
T	"a"		"a{2,}"			""
T	"aa"		"a{2,}"			"aa"
T	"aaa"		"a{2,}"			"aaa"

# generators
T	""		"a{2,}"			""

# epsilon
# generators
# escapes

## iteration, range
#printf "aaa\n"	| $CMD "((a:x){,}.*)"
#printf "aaa\n"	| $CMD "((a:x){1,}.*)"
