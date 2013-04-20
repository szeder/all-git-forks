# git blame --recursive

## Use Case

The user is interested in some contiguous set of lines in some particular file.

She wants to know the complete history for those lines: i.e. the shas for each
commit that modified the line.

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


## Output

Output should include the output of `git blame` for each iteration.  Each
iteration will also have `commit <sha>` as its first line.  If colour is enabled
this will be in the same colour as it is in git log.  Iterations will be
separated by a blank line, a la `git log`.



## Next Steps
0. define gdb macros
  - show the scoreboard
  - show blame entry (sha, lines)
  - define n to autolist
1. Consider an example where some subset of the blessed lines are reordered.
2. Become certain about the hunk-local hypothesis.  Is this actually reasonable?
3. Sketch the implementation.
  1. understand git blame
    - we want to set up a build with debug symbols
    - we want to run tests via gdb
    - ultimately we want to run a git-blame test in gdb
  2. sketch the implementation
4. Express examples as tests.
5. Sane gdbrc.


##  Tests
1. Test line reordering.  Hypothesis:  git blame's similarity computation 
   already handles this for us.
2. Test commit branching in blessed set


## Future Work
To completely support the use case, we want another feature, namely for blame to
support an output format that shows sha, author and short form commit message
rather than sha, author and timestamp.

