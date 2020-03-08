#!/bin/bash

# Funktionsbibliothek für die serverseitigen 
# Clientscripte zur Integration eines Community-Cloops
# GPL V3
# Frank Schiebel <frank@linuxmuster.net>
# Jesko Anschuetz <jesko|linuxmuster.net>
# Tobias Küchel <tobias|linuxmuster.net>

ts=$(date +%Y%m%d-%H%M%S)
CACHE=/var/cache/linuxmuster/client-servertools

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
    wget --mirror -A.meta -np -P $CACHE $CONF_CLOOP_SERVER > /dev/null 2>&1 && colorecho "green" "OK"
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
# @Anwendung
# get_target_fileset myxenial xenial916
#
function get_target_fileset {
    TARGET_FILESET["startconf"]=${CONF_LINBODIR}/start.conf.$1 
    TARGET_FILESET["cloop"]=${CONF_LINBODIR}/$2.cloop 
    for key in desc info; do 
        TARGET_FILESET["$key"]=${CONF_LINBODIR}/$2.cloop.$key 
    done
    TARGET_FILESET["client-dir"]=${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$1/
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
# $1 Hardwareklasse
# $2 Remote Hardwareklasse
#
function get_remote_cloop {

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

    get_target_fileset $1 $hwclass
    get_source_fileset $hwclass
    echo -n "Überprüfe bereits vorhandene Dateien: "
    if check_target_fileset ; then
	colorecho "green" "Success."
    else
	colorecho "red" "Failed."
    fi

    ## backup für die start.conf wird automatisch gemacht
    STARTCONF=${TARGET_FILESET["startconf"]};
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi
    
    ## backup für das Patchklassenverzeichnis
    PATCHCLASS=$hwclass
    if [ -d ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ]; then 
        echo "INFO: Sichere das vorhandene Patchklassenverzeichnis"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS  nach"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup"
        mv ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup
    fi

    for key in startconf cloop desc info client-dir; do 
        echo -n "Kopiere ${SOURCE_FILESET[$key]} nach ${TARGET_FILESET[$key]}: "
        if rsync -a --delete ${SOURCE_FILESET[$key]} ${TARGET_FILESET[$key]} ; then 
            colorecho "green" "Success."
        else 
            colorecho "red" "Failed"
        fi
    done
    
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
# $1  Hardwareklasse
# $2  Name der cloop-Datei, die eingerichtet werden soll
#
function configure_cloop {

    CLOOP_NAME=${2%.cloop}
    # Gibt es das Cloop?
    if [ ! -e $CONF_LINBODIR/${CLOOP_NAME}.cloop ]; then 
        echo "Cloop Datei nicht gefunden: $CONF_LINBODIR/${CLOOP_NAME}.cloop"
	exit_error "Failed. Aborting."
    fi
    echo "INFO: Cloop-Datei ist $CONF_LINBODIR/${CLOOP_NAME}.cloop"

    HOSTGROUP=${1}
    STARTCONF=$CONF_LINBODIR/start.conf.$HOSTGROUP
    if [ ! -e $STARTCONF ]; then 
        echo "WARNING: Keine start.conf für $HOSTGROUP gefunden"
	exit_error "Failed. Aborting."
    fi
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach $STARTCONF ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi

    # start.conf anpassen
    echo "INFO: Passe $STARTCONF an"
    sed -i "s/\(Server\s*\=\s*\) \(.*\)/\1 $serverip/" $STARTCONF
    sed -i "s/\(Group\s*\=\s*\) \(.*\)/\1 $HOSTGROUP            #Hardwareklasse/" $STARTCONF
    sed -i "s/\(BaseImage\s*\=\s*\) \(.*\)/\1 ${CLOOP_NAME}.cloop/" $STARTCONF

    # Imageverteilung via rsync oder ist bittorrent enabled?
    BITTORRENT_ON=$(grep START_BTTRACK /etc/default/bittorrent  | awk -F= '{print $2}')
    if [ "x$BITTORRENT_ON" == "x0" ]; then 
        sed -i "s/\(DownloadType\s*\=\s*\) \(.*\)/\1 rsync/" $STARTCONF
    fi

    echo "INFO: Erstelle postsync aus Vorlage"
    # postsync aus vorlage holen
    POSTSYNC=$CONF_LINBODIR/${CLOOP_NAME}.cloop.postsync
    cp $CONF_GENERIC_POSTSYNC/generic.postsync $POSTSYNC

    # Patchklasse = hostgroup
    PATCHCLASS=$HOSTGROUP
    echo "INFO: Patchklasse ist $PATCHCLASS"

    echo "INFO: Kopiere generische postsync.d-Dateien und passe sie an"
    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/
    cp -ar $CONF_GENERIC_POSTSYNC/generic.postsync.d/* ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/postsync.d/
    sed -i "s/\(PATCHCLASS\s*\=\s*\)\(.*\)/\1\"$PATCHCLASS\"/" $POSTSYNC

    # Netzwerksettings in den postsync-pfad
    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/
    echo >  ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    for i in servername serverip basedn sambadomain domainname ; do
	grep -e $i /var/lib/linuxmuster/setup.ini | sed "s/\ //g" >> ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    done
    #cp  /var/lib/linuxmuster/network.settings ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    
    # Passworthash in den postsync-Baum schreiben
    set_password_to_postsync $PATCHCLASS
 
    # public-key des Server-roots in die authorized keys der client roots
    echo "INFO: Kopiere root-public-keys in den postsync-Baum nach /root/.ssh/authorized_keys"
    mkdir -p  ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh
    cat /root/.ssh/id_*.pub > ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh/authorized_keys
    
    colorecho "green" "Success"
}
