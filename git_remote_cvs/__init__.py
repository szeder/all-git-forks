#!/usr/bin/env python

"""Support library package for git-remote-cvs.

git-remote-cvs is a Git remote helper command that interfaces with a
CVS repository to provide automatic import of CVS history into a Git
repository.

This package provides the support library needed by git-remote-cvs.
The following modules are included:

- cvs - Interaction with CVS repositories

- cvs_symbol_cache - Local CVS symbol cache

- changeset - Collect individual CVS revisions into commits

- git - Interaction with Git repositories

- commit_states - Map Git commits to CVS states

- cvs_revision_map - Map CVS revisions to various metainformation

- util - General utility functionality use by the other modules in
         this package, and also used directly by git-remote-cvs.

"""
