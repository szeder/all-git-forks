





wiki_page_content (){
	rm -rf ./tmp_test
	mkdir ./tmp_test
	#A modif !
	wiki_getpage.pl $2 $3 -d ./tmp_test

	if find ./tmp_test -name $2 -type f | grep -q $2; then
		git_content.sh $1 ./tmp_test/$2
		rm -rf ./tmp_test
	else
		rm -rf ./tmp_test
		echo "ERROR : file $2 not found on wiki $3"
		exit 1;
	fi
}

wiki_page_exist (){
	rm -rf ./tmp_test
	mkdir tmp_test
	#A modif !
	wiki_getpage.pl $1 $2 -d ./tmp_test

	if find ./tmp_test/ -name $1 -type f | grep -q $1; then
		rm -rf tmp_test
	else
		rm -rf ./tmp_test
		echo "ERROR : file $1 not found on wiki $2"
		exit 1;
	fi
}

git_content (){

	result=$(diff $1 $2)

	if echo $result | grep -q ">" ; then
		echo "test failed : file $1 and $2 do not match"
		exit 1;
	fi
}

git_exist (){

	result=$(find $1 -type f -name $2)

	if ! echo $result | grep -q $2; then
		echo "test failed : file $1/$2 does not exist"
		exit 1;
	fi

}
