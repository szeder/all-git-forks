#!/usr/bin/env python

"""Distutils build/install script for the git_remote_cvs package."""

from distutils.core import setup

setup(
    name = 'git_remote_cvs',
    version = '0.1.0',
    description = 'Git remote helper program for CVS repositories',
    license = 'GPLv2',
    author = 'The Git Community',
    author_email = 'git@vger.kernel.org',
    url = 'http://www.git-scm.com/',
    package_dir = {'git_remote_cvs': ''},
    packages = ['git_remote_cvs'],
)
