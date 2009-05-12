#!/bin/sh

git init

echo a > a
echo b > b
echo c > c
echo d > d
git add a b c d
git commit -m "a b c d"

echo bb >> b
echo cc >> c
echo ee > e
echo ff > f
git add b c e f

echo ccc >> c
echo ddd >> d
echo fff >> f
echo ggg >> g

# a is untouched
# b is modified and staged
# c is modified and partially staged
# d is modified and unstaged
# e is new and fully staged
# f is new and partially staged
# g is new and unstaged


