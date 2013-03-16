# git blame --recursive

## Use Case

The user knows some set of lines she is interested in.  She can specify those
lines by line number or regex (similar to what `git blame` already supports).

She wants to know the complete history for those lines: i.e. the shas and short
messages for each commit that modified the line.

##    Approaches

###   By line number

We recursively call git-blame with line numbers.

####  Updating line numbers

To do this we need to get updated line numbers for blamed shas, and for the
parents of blamed shas.  We believe getting the line numbers for blamed shas is
already done by `git blame`.

##### Updating line numbers for parents of blamed shas

We intend to use edit-distance to maintain a language-agnostic approach.

We still have the problem of identifying multiline changes.  Consider a commit
with a diff:

```
- foo( a, b, c )
+ foo(
+  a,
+  b,
+  c
+ )
```

Or its sister commit

```
- foo(
-  a,
-  b,
-  c
- )
+ foo( a, b, c )
```

Or even her more annoying sister commit:

```
- foo(
-  a, b
- )
+ foo( a,
+  b,
+  c )
```

We want to keep tracing line history through these commits. 

For a target introduced line, which is necessarily an addition, we need to
consider the introduced lines in the same hunk and match them against preceding
removed lines in the same hunk, with "\n" in the edit distance alphabet.
