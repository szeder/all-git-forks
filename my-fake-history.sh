#!/bin/sh

# Given the output of git-rev-list, this reconstructs the DAG of the history

i=0
tac | while read rev parents; do
	let i=$i+1
	echo $i > a1
	git add a1
	tree=$(git write-tree)
	parents="$(for parent in $parents
		do
			echo -n "-p $(git rev-parse sp-$parent) "
		done)"
	commit=$(echo "$rev $i" | git commit-tree $tree $parents)
	git tag sp-$rev $commit
done

