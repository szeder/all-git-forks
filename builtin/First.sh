#/bin/bash

# intiialize the three directories we are going to use.
mkdir ./upstream/
mkdir ./origin/ 
git init ./origin
git init ./upstream
git clone ./origin master
touch ./upstream/test.txt
cd ./upstream
git add .
git commit -m "Initializing contents of repo"
cd ..

#Tell user to go into directory (Check their command),
# if valid do it.  Otherwise retry.

#Tell them to add the remotes.  Same as above.
#git remote add origin ../origin
#git remote add upstream ../upstream


# tell them to fetch "git fetch upstream"
# tell them to merge "git merge upstream/master"
# tell them to push "git push origin"
#git add
#git status

