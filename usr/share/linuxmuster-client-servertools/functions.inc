#!/bin/bash

# Funktionsbibliothek für die serverseitigen 
# Clientscripte zur Integration eines Community-Cloops
# GPL V3
# Frank Schiebel <frank@linuxmuster.net>
# Jesko Anschuetz <jesko|linuxmuster.net>
# Tobias Küchel <tobias|linuxmuster.net>

ts=$(date +%Y%m%d-%H%M%S)
CACHE=/var/cache/linuxmuster/client-servertools
TEMPLATEDIR=/usr/lib/linuxmuster-client-servertools
SHAREDIR=/usr/share/linuxmuster-client-servertools/examples/

#
# Farbige Ausgabe
# @Argumente
# $1 Farbe
# $2 Text
# @Anwendung
# colorecho "red" "Failed!"
# colorecho "green" "OK"
#
function colorecho {

case "$1" in
    green)
        open="\e[01;32m";
        close="\e[00m";
        ;;
    red)
        open="\e[00;31m";
        close="\e[00m";
        ;;
    *)
        open=""
        close=""
esac

echo -e "${open}$2${close}"

}

#
# Beendet das Skript mit einem Fehler
#
function exit_error {
    if [ "x$1" != "x" ]; then 
        colorecho red "$1"
    fi
    exit 1
}




#
# Einige Checks, die vor dem Start des eigentlichen 
# Skripts ausgeführt werden
#
function check_startup {
    # Are we root?
    if [[ ! "$USER" == "root" ]]; then
        exit_error "Dieses Skript muss als root laufen. Abbruch."
    fi

    # Gibt es das linbo-Verzeichnis?
    if [[ ! -d ${CONF_LINBODIR} ]]; then
        exit_error "Das angegebene Linboverzeichnis existiert nicht. Abbruch"
    fi
}

# 
# Aktualisiert die Liste der aktuell verfügbaren Online-Image
# 
function get_available_images {
    #if [ -d /var/cache/linuxmuster/client-servertools/ ]; then 
    #    rm -rf /var/cache/linuxmuster/client-servertools/*
    #fi
    echo  -n "Hole Liste der verfügbaren cloops..."
    wget --mirror -A.meta -np -nH --cut-dirs=2 -P $CACHE $CONF_CLOOP_SERVER > /dev/null 2>&1 && colorecho "green" "OK"
}

#
# Testet, ob ein Image mit diesem Namen (lokal) existiert
# 
# $1 Imagename
#
function is_image_available {

    desc_files=$(find $CACHE/ -name '*.meta')

    for desc in $desc_files; do
        name=$(grep ^#Name: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        if [ "$1" == "$name" ]; then 
            return 0
        fi
    done
    return 1
}
    
# 
# Listet die aktuell verfügbaren Online-Images auf
# 
function list_available_images {
    echo "+++ Auflistung +++"

    desc_files=$(find $CACHE/ -name '*.meta')
    echo 
    echo -e "Imagename\tInfo"
    echo "-----------------------------------------------"
    for desc in $desc_files; do
        name=$(grep ^#Name: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        info=$(grep ^#Info: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        echo -e "$name\t$info"
    done
    echo "-----------------------------------------------"
    echo 
}

#
# Erzeugt ein Array mit den Zieldateien
# @Argumente
# $1 hardwareklasse
# $2 Imagename
# $3 patchklasse
# @Anwendung
# get_target_fileset myxenial xenial916
#
function get_target_fileset {
    TARGET_FILESET["startconf"]=${CONF_LINBODIR}/start.conf.$1 
    TARGET_FILESET["cloop"]=${CONF_LINBODIR}/$2.cloop 
    for key in desc info; do 
        TARGET_FILESET["$key"]=${CONF_LINBODIR}/$2.cloop.$key 
    done
    TARGET_FILESET["client-dir"]=${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$3/
}

#
# Erzeugt ein Array mit den Quelldateien
# @Argumente
# $1 Remote cloop Name
# @Anwendung
# get_source_fileset xenial914
#
function get_source_fileset {
    SOURCE_FILESET["startconf"]=start.conf.$1 
    SOURCE_FILESET["cloop"]=$1.cloop 
    for key in desc info; do 
        SOURCE_FILESET["$key"]=$1.cloop.$key 
    done
    SOURCE_FILESET["client-dir"]=${CONF_POSTSYNCDIR}/$1/
}


#
# Ueberprüft, ob die geplanten Zieldateien schon existieren
# @Argumente
# $1 hardwareklasse
# @Anwendung
# check_conflicting_files myxenial
#
function check_target_fileset {
    stop="0";
    for key in startconf cloop desc info client-dir; do 
        if [ -e ${TARGET_FILESET["$key"]} ]; then 
            echo "Die Datei ${TARGET_FILESET["$key"]}  existiert bereits. (Wird mit -f überschrieben)"
            stop="1"
        fi
    done
    if [ $FORCE -ne 1 ]; then 
	if [ "x$stop" == "x1" ]; then 
            colorecho "red" "Werde keine Dateien überschreiben, lösen Sie den Konflikt bitte zuerst auf"
            colorecho "red" "Oder verwenden Sie -f (--force), um alle Dateien zu überschreiben."
            exit 1
	fi
    fi
    return 0
}

#
# Hole alle Daten Download-Server
# @Argumente
# $1 lokale Hardwareklasse
# $2 Remote Hardwareklasse
#
function get_remote_cloop {
    echo "+++ Download +++"

    hwclass=${2}
    cd ${CACHE}/${hwclass}/

    echo -n "Hole ${hwclass}.clpkg.tar.bz2 von ${CONF_CLOOP_SERVER}: "
    if wget -c ${CONF_CLOOP_SERVER}/$hwclass/${hwclass}.clpkg.tar.bz2; then
	colorecho "green" "Success."
    else
        colorecho "red" "Failed"
    fi

    echo -n "Überprüfe Integrität: "
    if md5sum -c ${hwclass}.clpkg.meta --status ; then
	colorecho "green" "Success."
    else
        colorecho "red" "Failed"
    fi

    echo -n "Entpacke: "
    if tar -xjf ${hwclass}.clpkg.tar.bz2; then
	colorecho "green" "Success."
    else
        colorecho "red" "Failed"
    fi

}

#
# Sets password hash to postsync file
# $1 Name der Patchklasse
#
function set_password_to_postsync {

    if [ ! -d ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$1/common/ ]; then 
        echo "ERROR: Das Verzeichnis ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$1/common/ existiert nicht."
        echo "ERROR: Die Hardwareklasse $1 gibt es nicht?!"
	exit_error "Failed. Aborting."
    fi
    if [ "x$LAPASS" == "x" ]; then 
        # postsync konfiguration anpassen
        # linuxadmin-Passworthash aus der Konfiguration bestimmen und für das postsync Skript bereitstellen
	echo "INFO: Setze Passwort auf Konfigurationswert"
        PWHASH=$(echo "$CONF_LINUXADMIN_PW" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
        echo "linuxadmin|$PWHASH" > ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$1/common/passwords
    else 
	echo "INFO: Setze Passwort auf Kommandozeilenwert"
        PWHASH=$(echo "$LAPASS" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
        echo "linuxadmin|$PWHASH" > ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$1/common/passwords
    fi
}


#
# @Argumente
# $1 local patchclass
# $2 local hwclass
# $3 local cloop_name
# $4 remotename
#
function configure_cloop {

    echo "+++ Konfiguration +++"
    PATCHCLASS=$1
    HWCLASS=$2
    CLOOP_NAME=$3
    remotename=$4
    get_target_fileset $HWCLASS $CLOOP_NAME $PATCHCLASS
    get_source_fileset $remotename
    echo -n "Überprüfe bereits vorhandene Dateien: "
    if check_target_fileset ; then
	colorecho "green" "Success."
    else
	colorecho "red" "Failed."
    fi
    echo "HW-Klasse: $HWCLASS"
    echo "INFO: Patchklasse ist $PATCHCLASS"

    ## backup für die start.conf wird automatisch gemacht
    STARTCONF=${TARGET_FILESET["startconf"]};
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi
    
    ## backup für das Patchklassenverzeichnis
    if [ -d ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ]; then 
        echo "INFO: Sichere das vorhandene Patchklassenverzeichnis"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS  nach"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup"
        mv ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup
    fi

    cd ${CACHE}/${remotename}/
    for key in startconf cloop desc info client-dir; do 
        echo -n "Kopiere ${SOURCE_FILESET[$key]} nach ${TARGET_FILESET[$key]}: "
        if rsync -a --delete ${SOURCE_FILESET[$key]} ${TARGET_FILESET[$key]} ; then 
            colorecho "green" "Success."
        else 
            colorecho "red" "Failed"
        fi
    done
    
    # Gibt es das Cloop?
    if [ ! -e $CONF_LINBODIR/${CLOOP_NAME}.cloop ]; then 
        echo "Cloop Datei nicht gefunden: $CONF_LINBODIR/${CLOOP_NAME}.cloop"
	exit_error "Failed. Aborting."
    fi
    echo "INFO: Cloop-Datei ist $CONF_LINBODIR/${CLOOP_NAME}.cloop"

    # Netzwerksettings in den postsync-pfad
    echo "INFO: Creating and sourcing /etc/linuxmuster-client/server.network.settings in the clients filesystem"
    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/
    echo >  ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    for i in servername serverip basedn sambadomain domainname ; do
	grep -e $i $SERVER_SETTINGS | sed "s/\ //g" >> ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    done
    . ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings

    # start.conf sichern
    if [ ! -e $STARTCONF ]; then 
        echo "WARNING: Keine start.conf für $HWCLASS gefunden"
	exit_error "Failed. Aborting."
    fi
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi

    # start.conf anpassen
    echo "INFO: Passe $STARTCONF an"
    sed -i "s/\(Server\s*\=\s*\) \(.*\)/\1 $serverip/" $STARTCONF
    sed -i "s/\(Group\s*\=\s*\) \(.*\)/\1 $HWCLASS            #Hardwareklasse/" $STARTCONF
    sed -i "s/\(BaseImage\s*\=\s*\) \(.*\)/\1 ${CLOOP_NAME}.cloop/" $STARTCONF

    # Imageverteilung via rsync oder ist bittorrent enabled?
    BITTORRENT_ON=$(grep START_BTTRACK /etc/default/bittorrent  | awk -F= '{print $2}')
    if [ "x$BITTORRENT_ON" == "x0" ]; then 
        sed -i "s/\(DownloadType\s*\=\s*\) \(.*\)/\1 rsync/" $STARTCONF
    fi

    # generisches postsync aus vorlage holen (+ autobackup)
    echo "INFO: Erstelle postsync aus Vorlage mit korrekter Patchklasse"
    POSTSYNC=$CONF_LINBODIR/${CLOOP_NAME}.cloop.postsync
    if [ -f $POSTSYNC ]; then
	if ! diff $POSTSYNC $CONF_GENERIC_POSTSYNC/generic.postsync >/dev/null; then
	    echo "WARNING: Sichere $POSTSYNC nach ${POSTSYNC}.$ts.autobackup"
            mv $POSTSYNC ${POSTSYNC}.$ts.autobackup
	fi
    fi
    cp $CONF_GENERIC_POSTSYNC/generic.postsync $POSTSYNC
    sed -i "s/\(PATCHCLASS\s*\=\s*\)\(.*\)/\1\"$PATCHCLASS\"/" $POSTSYNC

    # postsync.d-files aus Vorlage holen (+ autobackup)
    echo "INFO: Kopiere generische postsync.d-Dateien und passe sie an"
    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/postsync.d/
    for SRC in $CONF_GENERIC_POSTSYNC/generic.postsync.d/*[^~]; do
     	if echo $(basename $SRC) | grep -E '\.dpkg-old$|\.dpkg-new$|\.dpkg-dist$' ; then
	    :
	else
	    TARGET=${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/postsync.d/$(basename $SRC)
	    if [ -f $TARGET ]; then
		if ! diff $TARGET $SRC > /dev/null; then
		    echo "WARNING: Sichere $TARGET nach $TARGET.$ts.autobackup~"
		    mv $TARGET $TARGET.$ts.autobackup~
		fi
	    fi
	    cp -ar $SRC $TARGET
	fi
    done

    # postsync-data-files aus Vorlage holen (+ autobackup) und ersetzen wichtiger Parameter
    function copy_template_to_postsyncdir() {
	mkdir -p $(dirname $TARGET)
	if [ -f $TARGET ]; then
	    if ! diff $TARGET $SRC > /dev/null; then
		echo " WARNUNG: Sichere $TARGET nach $TARGET.$ts.autobackup~"
		mv $TARGET $TARGET.$ts.autobackup~
	    fi
	fi
	cp -ar $SRC $TARGET
	sed -i "s/@@SERVERIP@@/$serverip/g" $TARGET
	sed -i "s/@@SERVERNAME@@/$servername/g" $TARGET
	sed -i "s/@@DOMAINNAME@@/$domainname/g" $TARGET
	sed -i "s/@@BASEDN@@/$basedn/g" $TARGET
	sed -i "s/@@SAMBADOMAIN@@/$sambadomain/g" $TARGET

	sed -i "s/@@PATCHCLASS@@/$PATCHCLASS/g" $TARGET
    }	

    for SRC in $TEMPLATEDIR/postsync.data.d/*[^~]; do
	if echo $(basename $SRC) | grep -E '\.dpkg-old$|\.dpkg-new$|\.dpkg-dist$' ; then
	    :
	else
	    TARGET=${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/$(head -n 1 $SRC | cut -d " " -f 2)
	    copy_template_to_postsyncdir
	    echo " Copying $SRC from template"
	fi
    done

    # Passworthash in den postsync-Baum schreiben
    echo "INFO: Setze Passwort für linuxadmin & co."
    set_password_to_postsync $PATCHCLASS

    # public-key des Server-roots in die authorized keys der client roots
    echo "INFO: Kopiere root-public-keys in den postsync-Baum nach /root/.ssh/authorized_keys"
    mkdir -p  ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh
    cat /root/.ssh/id_*.pub > ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh/authorized_keys

    ## cp example file
    echo "INFO: Eigenes postsync.d-file nach ${CONF_POSTSYNCDIR}/PATCHCLASS/common/postsync.d kopieren, z.B. folgendermaßen:"
    echo cp $SHAREDIR/10-local-example-fix-permissions ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/postsync.d
    
    colorecho "green" "Success"
}

#
# @Argumente
# $1  Patchklasse
# $2  interactive=1|0
#
function cleanup {
    PATCHCLASS=$1
    INTERACTIVE=$2

    echo "INFO: Cleaning up backup~ files in ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/ "
    files=$(find ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/ -name \*~)
    if [ -z "$files" ]; then
	echo " - Nothing to cleanup"
    else
	if [ "x$INTERACTIVE" = "x1" ]; then
	    echo $files
	    echo " - Do you want to delete those files? (y/N)"
	    read REPLY
	    if [ "x$REPLY" = "xy" ]; then
		find ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/ -name \*~ -exec rm \{\} \;
	    fi
	else
	    echo " - deleting files"
	    find ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/ -name \*~ -exec rm \{\} \;
	fi
    fi
      
}
