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

