#
# CONFIGURATION VARIABLES
#
WIKI_DIR_NAME="wiki"            # Name of the wiki's directory
WIKI_DIR_INST="/var/www"        # Directory of the web server
TMP="/tmp"                      # Temporary directory for downloads
                                # Absolute address needed!
SERVER_ADDR="localhost"         # Web server's address

# Do not change the variables below
MW_VERSION="mediawiki-1.19.0"
DB_FILE="wikidb.sqlite"

test_rep="tXXXX_tmp_rep"

wiki_getpage () {
# wiki_getpage wiki_page dest_path
#
# fetch a page wiki_page from wiki and copies its content
# in directory dest_path
	../test-gitmw.pl "get_page" "$1" "$2"
}


wiki_delete_page () {
#wiki_delete_page <page_name>
#delete the page <page_name> from the wiki.
	../test-gitmw.pl "delete_page" "$1"
}

wiki_editpage (){
# wiki_editpage <wiki_page> <wiki_content> <wiki_append>
#
# Edit a page <wiki_page> on the wiki with content <wiki_content>
# If <wiki_append> == true : append
#
# If page doesn't exist : it creates page

	../test-gitmw.pl "edit_page" "$1" "$2" "$3"
}

git_content (){
#usage: git_content dir_1 dir_2
#precondition: directories dir_1 and dir_2 must exist
#behavior: if content of directories dir_1 and dir_2 do not match, the program exit with an error.
#Please be advised: the difference of case and blank character are not considered in the difference between files.

	result=$(diff -r -B -w --exclude=".git" $1 $2)

	if echo $result | grep -q ">" ; then
		echo "test failed: file $1 and $2 do not match"
		exit 1;
	fi
}

git_exist (){
#usage : git_exist rep_name file_name
#behavior : if file_name is not present in rep_name 
#or in his subdirectory, the program exit with an error
	result=$(find $1 -type f -name $2)

	if ! echo $result | grep -q $2; then
		echo "test failed: file $1/$2 does not exist"
		exit 1;
	fi

}

wiki_page_content (){
#usage wiki_page_content <file> <page_name> 
#
#Exit with error code 1 if and only if the content of
#<page_name> and <file> do not match.

	test -d $test_rep || mkdir $test_rep
	wiki_getpage $2 $test_rep

	if find $test_rep -name $2.mw -type f | grep -q $2; then
		git_content $1 $test_rep/$2.mw
		rm -rf $test_rep
	else
		rm -rf $test_rep
		echo "ERROR: file $2 not found on wiki"
		exit 1;
	fi
}

wiki_page_exist (){
#usage wiki_page_exist <page_name>
#Exit with error code 1 if and only if the page <page_name> is not on wiki


	test -d $test_rep || mkdir $test_rep
	wiki_getpage $1 $test_rep

	if find $test_rep -name $1.mw -type f | grep -q $1; then
		rm -rf $test_rep
	else
		rm -rf $test_rep
		echo "ERROR: file $1 not found on wiki"
		exit 1;
	fi
}

wiki_getallpagename () {
# wiki_getallpagename

# fetch all pages
	../test-gitmw.pl "getallpagename"
}

wiki_getallpage() {
	wiki_getallpagename
	mkdir $1
	while read -r line; do
		wiki_getpage "$line" $1;
	done < all.txt
	rm all.txt
}

fail()
{
        echo $1
        exit 1
}

#
# Function grep_change_file()
# Grep the first occurence of the parameter $1 value $2 and replace
# that value by $3 in the MediaWiki's LocalSettings.php file named $4.
#
grep_change_file() {
        param="$1"
        value="$2"
        replace="$3"
        file="$4"
        file_tmp="${file}.tmp"
       
        nline=`grep -n "^\\\$$param" "$file" | cut -d ':' -f 1`
        sed "${nline}s,${value},${replace}," "$file" > "$file_tmp"
        cp "$file_tmp" "$file"
        rm "$file_tmp"
}

#
# Function cmd_install()
# Install a wiki in your web server directory.
#
cmd_install()
{
        # Copy the generic LocalSettings.php in the web server's directory
        # And modify parameters according to the ones set at the top
        # of this script.
        # Note that LocalSettings.php is never modified.
        if [ ! -f "LocalSettings.php" ] ; then
                fail "Can't find LocalSettings.php in the current folder.
                Please run the script inside its folder."
        fi
        cp "LocalSettings.php" "LocalSettings-tmp.php" ||
                fail "Unable to copy LocalSettings.php to LocalSettings-tmp.php"
        grep_change_file "wgScriptPath" "wiki" "$WIKI_DIR_NAME" "LocalSettings-tmp.php"
        grep_change_file "wgServer" "localhost" "$SERVER_ADDR" "LocalSettings-tmp.php"
        grep_change_file "wgSQLiteDataDir" "/tmp" "$TMP" "LocalSettings-tmp.php"
        mkdir "$WIKI_DIR_INST/$WIKI_DIR_NAME"
        if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ; then
                fail "Folder $WIKI_DIR_INST/$WIKI_DIR_NAME don't exist. Please create it
                and launch the script again."
        fi
        mv "LocalSettings-tmp.php" "$WIKI_DIR_INST/$WIKI_DIR_NAME/LocalSettings.php" ||
                fail "Unable to move LocalSettings-tmp.php in $WIKI_DIR_INST/$WIKI_DIR_NAME"
        echo "File LocalSettings.php is set in $WIKI_DIR_INST/$WIKI_DIR_NAME"

        # Copy the database file in the TMP directory.
        cmd_reset

        # Fetch MediaWiki's archive if not already present in the TMP directory
        cd $TMP
        if [ ! -f "$MW_VERSION.tar.gz" ] ; then
                echo "Downloading $MW_VERSION sources ..."
                wget http://download.wikimedia.org/mediawiki/1.19/mediawiki-1.19.0.tar.gz ||
                        fail "Unable to download http://download.wikimedia.org/mediawiki/1.19/mediawiki-1.19.0.tar.gz.
                        Please fix your connection and launch the script again."
        fi
        tar xfz "$MW_VERSION.tar.gz"
        echo "$MW_VERSION.tar.gz downloaded in `pwd`. You can delete it later if you want."

        # Copy the files of MediaWiki wiki in the web server's directory.
        cd "$MW_VERSION"
        cp -Rf * "$WIKI_DIR_INST/$WIKI_DIR_NAME/" ||
                fail "Unable to copy WikiMedia's files from `pwd` to $WIKI_DIR_INST/$WIKI_DIR_NAME"

        echo "Your wiki has been installed. You can check it at http://localhost/$WIKI_DIR_NAME"
}

#
# Function cmd_reset()
# Copy the initial database of the wiki over the actual one.
#
cmd_reset() {
        if [ ! -f "../tXXXX/$DB_FILE" ] ; then
                fail "Can't find $DB_FILE in the current folder.
                Please run the script inside its folder."
        fi
	cp "../tXXXX/$DB_FILE" "$TMP" || fail "Can't copy $DB_FILE in $TMP"
        chmod ugo+rw "$TMP/$DB_FILE" || fail "Can't add write perms on $TMP/$DB_FILE"
        echo "File ../tXXXX/$DB_FILE is set in $TMP"

}

#
# Function cmd_clear()
# Delete the wiki created in the web server's directory and all its content
# saved in the database.
#
cmd_clear() {
        # Delete the wiki's directory.
        rm -rf "$WIKI_DIR_INST/$WIKI_DIR_NAME" ||
                fail "Wiki's directory $WIKI_DIR_INST/$WIKI_DIR_NAME could not be deleted"

        # Delete the wiki's SQLite database
        rm -f "$TMP/$DB_FILE" || fail "Database $TMP/$DB_FILE could not be deleted."
}

cmd_help() {
        echo "Usage: "
        echo "  ./install_wiki <install|reset|delete|help>"
        echo "          install: Install a wiki on your computer."
        echo "          reset: Clear all pages and content of the wiki"
        echo "          delete: Delete the wiki and all its pages and content"
}

wiki_getallpage tXXXX_tmp

