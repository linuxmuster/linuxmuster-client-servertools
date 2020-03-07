#!/bin/bash

# Funktionsbibliothek für die serverseitigen 
# Clientscripte zur Integration eines Community-Cloops
# GPL V3
# Frank Schiebel <frank@linuxmuster.net>
# Jesko Anschuetz <jesko|linuxmuster.net>

ts=$(date +%Y%m%d-%H%M%S)


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
# Gibt einen Hilfetext aus
#
function print_help() {
cat <<End-of-help
Anwendung:
    $0 -a list-available
        Aktualisiert die Liste aller verfügbaren cloops und zeigt sie an.

    $0 -a auto -c <Imagename> [-H <hardware-class>] [-p <password>]
        Automatischer Modus, versucht das cloop aus dem Netz vollautomatisch zu installieren
        <Imagename> muss einer der verfügbaren Cloops sein. Hardwareklasse und Passwort sind optional.

    $0 -a get-cloop -c <Imagename> [-H <hardware-class>]
        Holt ein cloop aus dem Netz und speichert die nötigen dateien in ${CONF_LINBODIR} ab.
        Ist die Hardwareklasse nicht angegeben wird eine Datei start.conf.<Imagename> erstellt.

    $0 -a configure -c <Imagename> [-H <hardware-class>] [-p <password>]
        Bereitet ein heruntergeladenes cloop-Dateiset mit einem passenden postsync 
        Skript zur Synchronisation auf Clients vor. Setzt das Passwort des Benutzers linuxadmin 
	in der Hardwareklasse auf das mit -p angegebene Passwort, andernfalls auf den Wert in der 
        Konfigurationsdatei.
          
Optionen:
    -f                   überschreibe alle Dateien und fahre mit dem Download fort, autobackup start.conf
    -c <Imagename>      welches cloop aus dem Netz verwendet wird
    -H <hardware-class>  für welche Hardwareklasse das cloop heruntergeladen und konfiguriert wird
    -p <password>        Ermöglicht es, das Passwort einer Hardwareklasse manuell zu setzen. 

    Beispiel:

     $0 -a auto -H myxenial -c xenial916
    
     Holt das Image "xenial916" vom linuxmuster-Server und legt anschließend eine HW-Klasse "myxenial" an, 
     die das Image verwendet. Die Postsync-Einstellungen und Skripte werden im Verzeichnis
        ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/myxenial/
     angelegt. 
     Das Passwort für linuxadmin wird bei der Synchronisation auf den in der Konfigurationsdatei 
     vorgegebenen Wert gesetzt.

End-of-help
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
    wget --mirror -A.txt -np -P /var/cache/linuxmuster/client-servertools $CONF_CLOOP_SERVER > /dev/null 2>&1 && colorecho "green" "OK"
    
}

#
# Testet, ob ein Image mit diesem Namen (lokal) existiert
# 
# $1 Imagename
#
function is_image_available {

    desc_files=$(find /var/cache/linuxmuster/client-servertools/ -name 'description.txt')

    for desc in $desc_files; do
        name=$(grep ^Name: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
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
    desc_files=$(find /var/cache/linuxmuster/client-servertools/ -name 'description.txt')
    echo 
    echo -e "Imagename\tInfo"
    echo "-----------------------------------------------"
    for desc in $desc_files; do
        name=$(grep ^Name: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        info=$(grep ^Info: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
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
    for key in postsync desc macct; do 
        TARGET_FILESET["$key"]=${CONF_LINBODIR}/$2.cloop.$key 
    done
    for key in md5sums md5sig; do 
        TARGET_FILESET["$key"]=${CONF_LINBODIR}/$2_hashes.$key 
    done
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
    SOURCE_FILESET["md5sums"]=md5sums 
    SOURCE_FILESET["md5sig"]=md5sums.sig 
    for key in desc macct; do 
        SOURCE_FILESET["$key"]=$1.cloop.$key 
    done
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
    for key in startconf cloop desc macct md5sums md5sig; do 
        if [ -e ${TARGET_FILESET["$key"]} ]; then 
            echo "Die Datei ${TARGET_FILESET["$key"]}  existiert bereits."
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
}

#
# Hole die Cloop-Dateien vom cloop-Server
# @Argumente
# $1  Hardwareklasse
# $2 Remote Cloop Name
#
function get_remote_cloop {
    cloop_name=${2%.cloop}
    get_target_fileset $1 $cloop_name
    get_source_fileset $cloop_name
    check_target_fileset

    STARTCONF=${TARGET_FILESET["startconf"]};
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach $STARTCONF ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi

    for key in startconf cloop desc macct md5sums md5sig; do 
        echo "Hole ${TARGET_FILESET[$key]} von"
        echo "     ${CONF_CLOOP_SERVER}/cloops/$cloop_name/${SOURCE_FILESET[$key]}"
        if wget -c ${CONF_CLOOP_SERVER}/cloops/$cloop_name/${SOURCE_FILESET[$key]} -O ${TARGET_FILESET[$key]}; then 
            colorecho "green" "Success."
        else 
            colorecho "red" "Failed"
            #rm -f ${TARGET_FILESET[$key]}
        fi
    done
    echo "INFO: Überprüfe download"
    md5sum -c ${TARGET_FILESET["md5sums"]} --status
    if [ $? -eq 0 ]; then
 	colorecho "green" "Success."
    else
	colorecho "red" "Failed"
	md5sum -c ${TARGET_FILESET["md5sums"]} 
	exit_error "Lösche die entsprechenden Dateien und versuche es erneut."
    fi
}

#
# Sets password hash to postsync file
# $1 Name der Patchklasse
#
function set_password_to_postsync {
    #if [ x$1 == "x" ]; then 
    #    echo "ERROR: Zum setzen eines neuen Passworts muss die Patchklasse angegeben werden"
    #    exit 1
    #fi
    if [ ! -d ${CONF_LINBODIR}/linuxmuster-client/$1/common/ ]; then 
        echo "ERROR: Das Verzeichnis ${CONF_LINBODIR}/linuxmuster-client/$1/common/ existiert nicht."
        echo "ERROR: Die Hardwareklasse $1 gibt es nicht?!"
	exit_error "Failed. Aborting."
    fi

    if [ x$LAPASS == "x" ]; then 
        # postsync konfiguration anpassen
        # linuxadmin-Passworthash aus der Konfiguration bestimmen und für das postsync Skript bereitstellen
	echo "INFO: Setze Passwort auf Konfigurationswert"
        PWHASH=$(echo "$CONF_LINUXADMIN_PW" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
        echo "linuxadmin|$PWHASH" > ${CONF_LINBODIR}/linuxmuster-client/$1/common/passwords
    else 
	echo "INFO: Setze Passwort auf Kommandozeilenwert"
        PWHASH=$(echo "$LAPASS" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
        echo "linuxadmin|$PWHASH" > ${CONF_LINBODIR}/linuxmuster-client/$1/common/passwords
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

#    if [ $CONF_HOSTGROUP_AS_PATCHCLASS != 0 ]; then 
#        # Patchklasse aus config und Kommandozeile wird überschrieben
#        # wenn die Konfiguration die Hostgruppe als Patchklasse erzwingt
#        echo "WARN: Konfiguration erzwingt Hardwareklasse als Patchklasse"
#        PATCHCLASS=$HOSTGROUP
#    fi

    PATCHCLASS=$HOSTGROUP
    echo "INFO: Patchklasse ist $PATCHCLASS"

    # Gibt es das Patchklassenverzeichnise schon?
    # Wenn ja: Sichern!
    if [ -d ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ]; then 
        echo "WARN: Sichere das vorhandene Patchklassenverzeichnis"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS  nach"
        echo "      ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup"
        mv ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS.$ts.autobackup
    fi

    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/
    cp -ar $CONF_GENERIC_POSTSYNC/generic.postsync.d ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/postsync.d
    sed -i "s/\(PATCHCLASS\s*\=\s*\)\(.*\)/\1\"$PATCHCLASS\"/" $POSTSYNC

    # Netzwerksettings in den postsync-pfad
    mkdir -p ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/
    cp  /var/lib/linuxmuster/network.settings ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings
    
    # Passworthash in den postsync-Baum schreiben
    set_password_to_postsync $PATCHCLASS
 
    # public-key des Server-roots in die authorized keys der client roots
    mkdir -p  ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh
    cat /root/.ssh/id_*.pub > ${CONF_LINBODIR}/${CONF_POSTSYNCDIR}/$PATCHCLASS/common/root/.ssh/authorized_keys
    
    colorecho "green" "Success"
}
