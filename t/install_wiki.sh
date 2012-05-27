#!/bin/sh

# This script installs or deletes a MediaWiki on your computer.
# It requires a web server with PHP and a database running and mediawiki installed.
# As it changes some permissions file, it surely needs root privileges.
# Please set the CONFIGURATION VARIABLES section below first.

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
        if [ ! -f "$DB_FILE" ] ; then
                fail "Can't find $DB_FILE in the current folder.
                Please run the script inside its folder."
        fi
        cp "$DB_FILE" "$TMP" || fail "Can't copy $DB_FILE in $TMP"
        chmod ugo+rw "$TMP/$DB_FILE" || fail "Can't add write perms on $TMP/$DB_FILE"
        echo "File $DB_FILE is set in $TMP"
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

# Argument: install, reset, delete
if [ "$1" = "install" ] ; then
        cmd_install
        exit 0
elif [ "$1" = "reset" ] ; then
        cmd_reset
        exit 0
elif [ "$1" = "delete" ] ; then
        cmd_clear
        exit 0
else
        cmd_help
fi
