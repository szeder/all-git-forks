# Copyright (C) 2012
#     Charles Roussel <charles.roussel@ensimag.imag.fr>
#     Simon Cathebras <simon.cathebras@ensimag.imag.fr>
#     Julien Khayat <julien.khayat@ensimag.imag.fr>
#     Guillaume Sasdy <guillaume.sasdy@ensimag.imag.fr>
#     Simon Perrat <simon.perrat@ensimag.imag.fr>
# License: GPL v2 or later

#
# CONFIGURATION VARIABLES
# You might want to change these ones
#
WIKI_DIR_NAME="wiki"            # Name of the wiki's directory
WIKI_DIR_INST="/var/www"        # Directory of the web server
TMP="/tmp"                      # Temporary directory for downloads
                                # Absolute path required!
SERVER_ADDR="localhost"         # Web server's address

# CONFIGURATION
# You should not change these ones unless you know what you do
#
MW_VERSION="mediawiki-1.19.0"
DB_FILE="wikidb.sqlite"
FILES_FOLDER="install-wiki"
DB_INSTALL_SCRIPT="db_install.php"
WIKI_ADMIN="WikiAdmin"
WIKI_PASSW="AdminPass"

export CURR_DIR=$(pwd)
export TEST_DIRECTORY=$CURR_DIR/../../../t

wiki_getpage () {
	$CURR_DIR/test-gitmw.pl get_page "$@"
}

wiki_delete_page () {
	$CURR_DIR/test-gitmw.pl delete_page "$@"
}

wiki_editpage () {
	$CURR_DIR/test-gitmw.pl edit_page "$@"
}

die () {
	die_with_status 1 "$@"
}

die_with_status () {
	status=$1
	shift
	echo >&2 "$*"
	exit "$status"
}

# test_diff_directories <dir_git> <dir_wiki>
#
# Compare the contents of directories <dir_git> and <dir_wiki> with diff
# and errors if they do not match. The program will
# not look into .git in the process.
# Warning: the first argument MUST be the directory containing the git data
test_diff_directories () {
	rm -rf "$1_tmp"
	mkdir -p "$1_tmp"
	cp "$1"/*.mw "$1_tmp"
	diff -r -b "$1_tmp" "$2"
}

# $1=<dir>
# $2=<N>
#
# Check that <dir> contains exactly <N> files
test_contains_N_files () {
	test `ls "$1" | wc -l` -eq "$2";
}


# wiki_check_content <file_name> <page_name> 
#
# Compares the contents of the file <file_name> and the wiki page
# <page_name> and exits with error 1 if they do not match.
wiki_check_content () {
	mkdir -p wiki_tmp
	wiki_getpage "$2" wiki_tmp
	diff -b "$1" wiki_tmp/"$2".mw
	if test $? -ne 0
	then
		rm -rf wiki_tmp
		error "ERROR: file $2 not found on wiki"
	fi
	rm -rf wiki_tmp
}

# wiki_page_exist <page_name>
#
# Check the existence of the page <page_name> on the wiki and exits
# with error if it is absent from it.
wiki_page_exist () {
	wiki_getpage "$1" .

	if test -f "$1".mw ; then
		rm "$1".mw
	else
		error "test failed: file $1 not found on wiki"
	fi
}

# wiki_getallpagename
# 
# Fetch the name of each page on the wiki.
wiki_getallpagename () {
	$CURR_DIR/test-gitmw.pl getallpagename
}

# wiki_getallpagecategory <category>
# 
# Fetch the name of each page belonging to <category> on the wiki.
wiki_getallpagecategory () {
	$CURR_DIR/test-gitmw.pl getallpagename "$@"
}

# wiki_getallpage <dest_dir> [<category>]
#
# Fetch all the pages from the wiki and place them in the directory
# <dest_dir>.
# If <category> is define, then wiki_getallpage fetch the pages included
# in <category>.
wiki_getallpage () {
	if test -z "$2";
	then
		wiki_getallpagename
	else
		wiki_getallpagecategory "$2"
	fi
	mkdir -p "$1"
	while read -r line; do
		wiki_getpage "$line" $1;
	done < all.txt
}

# Create the SQLite database of the MediaWiki. If the database file already
# exists, it will be deleted.
# This script should be runned from the directory where $FILES_FOLDER is
# located.
create_db () {
        rm -f "$TMP/$DB_FILE"

        echo "Generating the SQLite database file. It can take some time ..."
        # Run the php script to generate the SQLite database file
        # with cURL calls.
        php "$FILES_FOLDER/$DB_INSTALL_SCRIPT" $(basename "$DB_FILE" .sqlite) \
                "$WIKI_ADMIN" "$WIKI_PASSW" "$TMP"

        if [ ! -f "$TMP/$DB_FILE" ] ; then
                error "Can't create database file in TODO. Try to run ./install-wiki.sh delete first."
        fi
        chmod 666 "$TMP/$DB_FILE"

        # Copy the generated database file into the directory the
        # user indicated.
        cp --preserve=mode,ownership "$TMP/$DB_FILE" "$FILES_FOLDER" ||
                error "Unable to copy $TMP/$DB_FILE to $FILES_FOLDER"
}

# Install a wiki in your web server directory.
wiki_install () {

        # In this part, we change directory to $TMP in order to download,
        # unpack and copy the files of MediaWiki
        (
        mkdir -p "$WIKI_DIR_INST/$WIKI_DIR_NAME"
        if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ; then
                error "Folder $WIKI_DIR_INST/$WIKI_DIR_NAME doesn't exist.
                Please create it and launch the script again."
        fi

        # Fetch MediaWiki's archive if not already present in the TMP directory
        cd "$TMP"
        if [ ! -f "$MW_VERSION.tar.gz" ] ; then
                echo "Downloading $MW_VERSION sources ..."
                wget "http://download.wikimedia.org/mediawiki/1.19/mediawiki-1.19.0.tar.gz" ||
                        error "Unable to download "\
                        "http://download.wikimedia.org/mediawiki/1.19/"\
                        "mediawiki-1.19.0.tar.gz. "\
                        "Please fix your connection and launch the script again."
        fi
        tar xfz "$MW_VERSION.tar.gz"
        echo "$MW_VERSION.tar.gz downloaded in `pwd`."\
                "You can delete it later if you want."

        # Copy the files of MediaWiki wiki in the web server's directory.
        cd "$MW_VERSION"
        cp -Rf * "$WIKI_DIR_INST/$WIKI_DIR_NAME/" ||
                error "Unable to copy WikiMedia's files from `pwd` to "\
                        "$WIKI_DIR_INST/$WIKI_DIR_NAME"
        )

        create_db

        # Copy the generic LocalSettings.php in the web server's directory
        # And modify parameters according to the ones set at the top
        # of this script.
        # Note that LocalSettings.php is never modified.
        if [ ! -f "$FILES_FOLDER/LocalSettings.php" ] ; then
                error "Can't find $FILES_FOLDER/LocalSettings.php " \
                        "in the current folder. "\
                "Please run the script inside its folder."
        fi
        cp "$FILES_FOLDER/LocalSettings.php" \
                "$FILES_FOLDER/LocalSettings-tmp.php" ||
                error "Unable to copy $FILES_FOLDER/LocalSettings.php " \
                "to $FILES_FOLDER/LocalSettings-tmp.php"

        # Parse and set the LocalSettings file of the user according to the
        # CONFIGURATION VARIABLES section at the beginning of this script
        file_swap="$FILES_FOLDER/LocalSettings-swap.php"
        sed "s,@WG_SCRIPT_PATH@,/$WIKI_DIR_NAME," \
                "$FILES_FOLDER/LocalSettings-tmp.php" > "$file_swap"
        mv "$file_swap" "$FILES_FOLDER/LocalSettings-tmp.php"
        sed "s,@WG_SERVER@,http://$SERVER_ADDR," \
                "$FILES_FOLDER/LocalSettings-tmp.php" > "$file_swap"
        mv "$file_swap" "$FILES_FOLDER/LocalSettings-tmp.php"
        sed "s,@WG_SQLITE_DATADIR@,$TMP," \
                "$FILES_FOLDER/LocalSettings-tmp.php" > "$file_swap"
        mv "$file_swap" "$FILES_FOLDER/LocalSettings-tmp.php"

        mv "$FILES_FOLDER/LocalSettings-tmp.php" \
                "$WIKI_DIR_INST/$WIKI_DIR_NAME/LocalSettings.php" ||
                error "Unable to move $FILES_FOLDER/LocalSettings-tmp.php" \
                "in $WIKI_DIR_INST/$WIKI_DIR_NAME"
        echo "File $FILES_FOLDER/LocalSettings.php is set in" \
                " $WIKI_DIR_INST/$WIKI_DIR_NAME"

        chmod ugo+w "$FILES_FOLDER/$DB_FILE"

        echo "Your wiki has been installed. You can check it at
                http://$SERVER_ADDR/$WIKI_DIR_NAME"
}

# Reset the database of the wiki and the password of the admin
#
# Warning: This function must be called only in a subdirectory of t/ directory
wiki_reset () {
        # Copy initial database of the wiki
        if [ ! -f "../$FILES_FOLDER/$DB_FILE" ] ; then
                error "Can't find ../$FILES_FOLDER/$DB_FILE in the current folder."
        fi
        cp "../$FILES_FOLDER/$DB_FILE" "$TMP" ||
                error "Can't copy ../$FILES_FOLDER/$DB_FILE in $TMP"
        echo "File $FILES_FOLDER/$DB_FILE is set in $TMP"
}

# Delete the wiki created in the web server's directory and all its content
# saved in the database.
wiki_delete () {
	# Delete the wiki's directory.
	rm -rf "$WIKI_DIR_INST/$WIKI_DIR_NAME" ||
		error "Wiki's directory $WIKI_DIR_INST/" \
		"$WIKI_DIR_NAME could not be deleted"

	# Delete the wiki's SQLite database
	rm -f "$TMP/$DB_FILE" || error "Database $TMP/$DB_FILE could not be deleted."
}

