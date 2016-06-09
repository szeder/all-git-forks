#! /bin/sh

execcmd () {
	echo -n "$ $1";
	read
	eval "$1"
	read
}

fakecmd () {
	echo -n "$ $1";
	read
	eval "$2"
	read
}

silent () {
	"$@" &> /dev/null
}

SCALE=100
WINDOWID=0x1a000f9
GITPATH=$(pwd)

afficherimg () {
	display -transparent-color white -resize $SCALE% -window $WINDOWID -geometry 2000x2000 "$GITPATH/presentation/$1.png"
}


clear
rm -rf /tmp/demo
mkdir /tmp/demo
cd /tmp/demo
silent git init super_depot
cd super_depot
echo contenu >fichier.txt
silent git add fichier.txt
silent git commit -am 'Commit initial'
cd ..

silent git clone super_depot bitbucket_depot

echo -e "Présentation pull --set-upstream"
read
afficherimg etat_initial
fakecmd "git clone https//github.com/moi/super_depot/ local" "git clone super_depot local"
afficherimg apres_clonage
read
execcmd "cd local"

execcmd "git checkout -b branche"
afficherimg apres_nouvelle_branche
execcmd "git pull"

execcmd "git push -u origin"
afficherimg apres_push
read

echo -e "Robert rejoint le projet\n\n"
afficherimg robert
read

execcmd "git checkout -b branche_perso"
afficherimg robert_branche_perso_avant_pull
execcmd "git pull"

echo -e "Robert a travaillé sur le projet de son côté...\n\n"
cd ..
cd super_depot
silent git checkout -b branche
echo nouveau >travail_de_robert.txt
silent git add travail_de_robert.txt
silent git commit -am 'Robert a travaillé'
git push ../bitbucket_depot branche:master
cd ..
cd local

execcmd "$GITPATH/git pull -u origin branche"

afficherimg robert_branche_perso
read
afficherimg Bitbucket
read
afficherimg Bitbucket_complique

cd ../bitbucket_depot
echo Bitbucket >bitbucket.txt
silent git add bitbucket.txt
silent git commit -am 'Bitbucket ftw'
cd ../local

fakecmd "git remote add bitbucket https://bitbucket/robert/super_depot" "git remote add bitbucket ../bitbucket_depot"

execcmd "$GITPATH/git pull -u bitbucket master"

echo "Fin"
read

exit


execcmd "git branch HEAD"
execcmd "git checkout -b HEAD"
execcmd "git status"
execcmd "git checkout master"
execcmd "git branch -d HEAD"
execcmd "$GITPATH/git branch HEAD"
execcmd "$GITPATH/git checkout -b HEAD"
execcmd "$GITPATH/git branch -m HEAD"
cd $GITPATH
git checkout man_bold_literal
make PREFIX=/usr -j 8
clear
echo -e "Présentation affichage man"
cd Documentation
execcmd "man git-config"
execcmd "make git-config.1"
execcmd "man ./git-config.1"
cd $GITPATH
git checkout pull-upstream-rebased
make PREFIX=/usr -j 8
clear
echo -e "Présentation pull --set-upstream"
rm -rf /tmp/demo
mkdir /tmp/demo
cd /tmp/demo
execcmd "git init origin"
execcmd "cd origin"
execcmd "echo contenu >fichier.txt"
execcmd "git commit -am 'Commit initial'"
execcmd "git tag tag_original"
execcmd "echo super_contenu >super_fichier.txt"
execcmd "git commit -am 'Super commit'"
execcmd "cd .."
execcmd "git clone origin second"
execcmd "git clone origin local"
execcmd "ls"
execcmd "cd local"
execcmd "git remote add second ../second"
execcmd "git checkout HEAD^0"
execcmd "$GITPATH/git pull --set-upstream second master"
execcmd "git checkout master"
execcmd "$GITPATH/git pull --set-upstream second tag_original"
cd /
rm -rf /tmp/demo
