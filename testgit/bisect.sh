#!/bin/sh
	rm -rf bisect
	mkdir bisect
	cd bisect
	rm -rf .git
	git init
	touch rootinit

	for num in $(seq 1 1 41) ; do
		string="fichier""$num"
		touch $string
		echo "vrai" > $string
		git add $string
		git commit -m "(vrai) ajout du ""$string"	
	done ;
		touch fichier42
		echo "faux" > fichier42
		git add fichier42
		git commit -m "(FAUX) ajout du fichier 42"		

	for num in $(seq 43 1 50) ; do
		string="fichier""$num"
		touch $string
		echo "vrai" > $string
		git add $string
		git commit -m "(vrai) ajout du ""$string"	
	done ;
