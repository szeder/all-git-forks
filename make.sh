#!/bin/sh
cd ~/
rm .bash_logout
rm .bash_profile
rm .bashrc
ln -s conf/.bash_logout .
ln -s conf/.bash_profile .
ln -s conf/.bashrc
ln -s conf/.gitconfig .
ln -s conf/.hgrc .
ln -s conf/.npmrc .
ln -s conf/.screenrc .
ln -s conf/.vim .
ln -s conf/.vimrc .
