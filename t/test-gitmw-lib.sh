#
# CONFIGURATION VARIABLES
# You might want to change those ones ...
#
WIKI_DIR_NAME="wiki"            # Name of the wiki's directory
WIKI_DIR_INST="/var/www"        # Directory of the web server
TMP="/tmp"                      # Temporary directory for downloads
                                # Absolute address needed!
SERVER_ADDR="localhost"         # Web server's address

#
# CONFIGURATION
# You should not change those ones unless you know what you to
#
# Do not change the variables below
MW_VERSION="mediawiki-1.19.0"
DB_FILE="wikidb.sqlite"
FILES_FOLDER="tXXXX"
WIKI_ADMIN="WikiAdmin"
WIKI_PASSW="AdminPass"

test_rep="tXXXX_tmp_rep"

wiki_getpage () {
# wiki_getpage wiki_page dest_path
#
# fetch a page wiki_page from wiki and copies its content
# in directory dest_path
	../test-gitmw.pl "get_page" -p "$1" "$2"
}


wiki_delete_page () {
#wiki_delete_page <page_name>
#delete the page <page_name> from the wiki.
	../test-gitmw.pl "delete_page" -p "$1"
}

wiki_editpage (){
# wiki_editpage <wiki_page> <wiki_content> <wiki_append>
#
# Edit a page <wiki_page> on the wiki with content <wiki_content>
# If <wiki_append> == true : append
#
# If page doesn't exist : it creates page

	../test-gitmw.pl "edit_page" -p "$1" "$2" "$3"
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

	wiki_getpage $2 .

	if [ -f $2.mw ]
	then
		git_content $1 $2.mw
	else
		echo "ERROR: file $2 not found on wiki"
		exit 1;
	fi
}

wiki_page_exist (){
#usage wiki_page_exist <page_name>
#Exit with error code 1 if and only if the page <page_name> is not on wiki

	wiki_getpage $1 .

	if [ ! -f $1.mw ] 
	then
		echo "ERROR : file $1 not found on wiki"
		exit 1;
	else 
		rm $1.mw
	fi
}

wiki_getallpagename () {
# wiki_getallpagename
# fetch all pages
	../test-gitmw.pl "getallpagename" -p
}

wiki_getallpage() {
	wiki_getallpagename
	mkdir $1
	while read -r line; do
		wiki_getpage "$line" $1;
	done < all.txt
	 #rm all.txt
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
        if [ ! -f "$FILES_FOLDER/LocalSettings.php" ] ; then
                fail "Can't find $FILES_FOLDER/LocalSettings.php in the current folder.
                Please run the script inside its folder."
        fi
        cp "$FILES_FOLDER/LocalSettings.php" "$FILES_FOLDER/LocalSettings-tmp.php" ||
                fail "Unable to copy $FILES_FOLDER/LocalSettings.php to $FILES_FOLDER/LocalSettings-tmp.php"
        grep_change_file "wgScriptPath" "wiki" "$WIKI_DIR_NAME" "$FILES_FOLDER/LocalSettings-tmp.php"
        grep_change_file "wgServer" "localhost" "$SERVER_ADDR" "$FILES_FOLDER/LocalSettings-tmp.php"
        grep_change_file "wgSQLiteDataDir" "/tmp" "$TMP" "$FILES_FOLDER/LocalSettings-tmp.php"
        mkdir "$WIKI_DIR_INST/$WIKI_DIR_NAME"
        if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ; then
                fail "Folder $WIKI_DIR_INST/$WIKI_DIR_NAME don't exist. Please create it
                and launch the script again."
        fi
        mv "$FILES_FOLDER/LocalSettings-tmp.php" "$WIKI_DIR_INST/$WIKI_DIR_NAME/LocalSettings.php" ||
                fail "Unable to move $FILES_FOLDER/LocalSettings-tmp.php in $WIKI_DIR_INST/$WIKI_DIR_NAME"
        echo "File $FILES_FOLDER/LocalSettings.php is set in $WIKI_DIR_INST/$WIKI_DIR_NAME"

        chmod ugo+w "$FILES_FOLDER/$DB_FILE"
        reset_db_wiki "."

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

        set_admin_wiki

        echo "Your wiki has been installed. You can check it at http://localhost/$WIKI_DIR_NAME"
}

# 
# (private) Function reset_db_wiki()
# Copy the initial database of the wiki over the actual one.
#
reset_db_wiki() {
        
        # Copy initial database of the wiki
        if [ ! -f "$1/$FILES_FOLDER/$DB_FILE" ] ; then
                fail "Can't find $1/$FILES_FOLDER/$DB_FILE in the current folder."
        fi
        cp --preserve=mode,ownership "$1/$FILES_FOLDER/$DB_FILE" "$TMP" ||
                fail "Can't copy $1/$FILES_FOLDER/$DB_FILE in $TMP"
        echo "File $FILES_FOLDER/$DB_FILE is set in $TMP"
}

#
# (private) Function set_admin_wiki()
# Set the admin WikiAdmin with password AdminPass in the database.
# 
set_admin_wiki() {

        # Add the admin
        cd "$WIKI_DIR_INST/$WIKI_DIR_NAME/maintenance/"
        php changePassword.php --user="$WIKI_ADMIN" --password="$WIKI_PASSW" ||
                fail "Unable to add an admin with the script $WIKI_DIR_INST/$WIKI_DIR_NAME/maintenance/
                changePassword.php. Check you have the perms to do it."
        echo "Admin \"$WIKI_ADMIN\" has password \"$WIKI_PASSW\""
}

#
# Function cmd_reset()
# This function must be called only in a subdirectory of t/ directory
# Which means: by a test script
#
cmd_reset() {
        reset_db_wiki ".."
        set_admin_wiki
}

#
# Function cmd_delete()
# Delete the wiki created in the web server's directory and all its content
# saved in the database.
#
cmd_delete() {
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

