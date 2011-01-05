#!/usr/bin/python
from subprocess import *
import re
import os

subroot_re = re.compile("^trunk|^branches/[^/]*|^tags/[^/]*") 

tree_re = re.compile("^tree ([0-9a-f]{40})", flags=re.MULTILINE)
parent_re = re.compile("^parent ([0-9a-f]{40})", flags=re.MULTILINE)
author_re = re.compile("^author (.*)$", flags=re.MULTILINE)
committer_re = re.compile("^committer (.*)$", flags=re.MULTILINE)

git_svn_id_re = re.compile("^git-svn-id[^@]*", flags=re.MULTILINE)

ref_commit = {}
tree_commit = {}
count = 1

# Open a cat-file process for subtree lookups
subtree_process = Popen(["git","cat-file","--batch-check"], stdin=PIPE, stdout=PIPE)

# Iterate over commits from subversion imported with svn-fe
revlist = Popen(["git","rev-list","--reverse","--topo-order","--default","HEAD"], stdout=PIPE)
cat_file = Popen(["git","cat-file","--batch"], stdin=revlist.stdout, stdout=PIPE)
object_header = cat_file.stdout.readline().strip().split(" ");
while len(object_header) == 3:
    object_body = cat_file.stdout.read(int(object_header[2]))
    cat_file.stdout.read(1)
    git_commit = object_header[0]
    (commit_header, blank_line, commit_message) = object_body.partition("\n\n")
    object_header = cat_file.stdout.readline().strip().split(" ");

    author = author_re.search(commit_header).group()
    committer = committer_re.search(commit_header).group()

    # Diff against the empty tree if no parent
    match = parent_re.search(commit_header)
    if match:
        parent = match.group(1)
    else:
        parent = "4b825dc642cb6eb9a060e54bf8d69288fbee4904"

    # Find a common path prefix in the changes for the revision
    subroot = ""
    changes = Popen(["git","diff","--name-only",parent,git_commit], stdout=PIPE)
    for path in changes.stdout:
        match = subroot_re.match(path)
        if match:
            subroot = match.group()
            changes.terminate()
            break

    # Attempt to rewrite the commit on top of the matching branch
    if subroot == "":
        print "progress Weird commit - no subroot."
    else:
        # Rewrite git-svn-id in the log to point to the subtree
        commit_message = git_svn_id_re.sub('\g<0>/'+subroot, commit_message)
        subtree_process.stdin.write(git_commit+":"+subroot+"\n")
        subtree_process.stdin.flush()
        subtree_line = subtree_process.stdout.readline()
        if re.match("^.*missing$", subtree_line):
            print "progress Weird commit - invalid subroot"
            continue
        subtree = subtree_line[0:40]
        # Map the svn tag/branch name to a git-friendly one
	ref = "refs/heads/" + re.sub(" ", "%20", subroot)
        # Choose a parent for the rewritten commit
        if ref in ref_commit:
            parent = ref_commit[ref]
        elif subtree in tree_commit:
            parent = tree_commit[subtree]
        else:
	    parent = ""
        # Update tags if necessary
        if re.match("^refs/heads/tags/", ref):
            if parent == "":
                print "progress Weird tag - no matching commit."
            else:
                tagname = ref[16:]
                print "tag "+tagname
                print "from "+parent
                print "tagger "+committer[10:]
                print "data "+str(len(commit_message))
                print commit_message
        else:
            # Default to trunk if the branch is new
            if parent == "" and "refs/heads/trunk" in ref_commit:
                parent = ref_commit["refs/heads/trunk"]
            print "commit "+ref
            print "mark :"+str(count)
            print author
            print committer
            print "data "+str(len(commit_message))
            print commit_message
            if parent != "":
                print "from "+parent
            print "M 040000 "+subtree+" \"\""
            commit = ":"+str(count)
            # Advance the matching branch
            ref_commit[ref] = commit
            # Update latest commit by tree to drive parent matching
            tree_commit[subtree] = commit
    print "progress " + str(count)
    count = count + 1

subtree_process.terminate()
