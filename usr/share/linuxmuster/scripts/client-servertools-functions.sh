# Funktionsbibliothek für die serverseitigen 
# Clientscripte zur Integration eines Community-Cloops
# Jesko Anschütz <jesko.anschuetz@linuxmuster.net>
# Frank Schiebel <frank@linuxmuster.net>
# Version 01 09.12.2014
# GPL v3

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
# Gibt einen Hilfetext aus
#
function print_help() {
cat <<End-of-help
Anwendung:
    linuxmuster-manage-cloop [-a action]
Aktionsauswahl:
    -a auto      
        Automatischer Modus, versucht das cloop aus dem Netz vollautomatisch zu installieren
    -a get-cloop 
        Holt ein cloop aus dem Netz und speichert die nötigen dateien in /var/linbo/ ab. 
        Bei Konflikten mit vorhandenen Dateien wird der Benutzer informiert, es werden
        keine Daten überschrieben 
    -a configure  
        Bereitet ein heruntergeladenes cloop-Dateiset mit einem passenden postsync 
        Skript zur Synchronisation auf Clients vor

Optionen
    -h <Hardware-Klasse>  
        Legt unter verwendung des heruntergeladenen cloops 
        eine Konfiguration mit dem angegebenen Namen an.
        Standard ist "ubuntuclient"
    -c <cloop-Dateiname>
        Kann bei der Aktion "configure" angegeben werden. Name der 
        cloop-Datei ohne den Pfad /var/linbo
End-of-help
}


#
# Einige Checks, die vor dem Start des eigentlichen 
# Skripts ausgeführt werden
#
function check_startup {
    # Are we root?
    if [[ ! "$USER" == "root" ]]; then
        echo "Dieses Skript muss als root laufen. Abbruch."
        exit 1
    fi

    # Gibt es das linbo-Verzeichnis?
    if [[ ! -d ${CONF_LINBODIR} ]]; then
        echo "Das angegebene Linboverzeichnis existiert nicht. Abbruch"
        exit 1
    fi
}

#
# Erzeugt ein Array mit den Zieldateien
# @Argumente
# $1 hardwareklasse
# @Anwendung
# get_target_fileset mylinux
#
function get_target_fileset {
    TARGET_FILESET["startconf"]=${CONF_LINBODIR}/start.conf.$1 
    TARGET_FILESET["cloop"]=${CONF_LINBODIR}/$1.cloop 
    for key in postsync desc macct; do 
        TARGET_FILESET["$key"]=${CONF_LINBODIR}/$1.cloop.$key 
    done

}
#
# Erzeugt ein Array mit den Quelldateien
# @Argumente
# $1 Remote cloop Name
# @Anwendung
# get_source_fileset trusty714
#
function get_source_fileset {
    SOURCE_FILESET["startconf"]=start.conf.$1 
    SOURCE_FILESET["cloop"]=$1.cloop 
    for key in postsync desc macct; do 
        SOURCE_FILESET["$key"]=$1.cloop.$key 
    done
}


#
# Überprüft, ob die geplanten Zieldateien schon existieren
# @Argumente
# $1 hardwareklasse
# @Anwendung
# check_conflicting_files mylinux
#
function check_target_fileset {
    stop="0";
    for key in startconf cloop postsync desc macct; do 
        if [ -e ${TARGET_FILESET["$key"]} ]; then 
            echo "Die Datei ${TARGET_FILESET["$key"]}  existiert bereits."
            stop="1"
        fi
    done
    if [ "x$stop" == "x1" ]; then 
        colorecho "red" "Werde keine Dateien überschreiben, lösen Sie den Konflikt bitte zuerst auf"
        exit 1
    fi
}

#
# Hole die Cloop-Dateien vom cloop-Server
# @Argumente
# $1  Hardwareklasse
# $2 Remote Cloop Name
#
function get_remote_cloop {
    get_target_fileset $1
    get_source_fileset ${2%.cloop}
    check_target_fileset

    for key in startconf cloop postsync desc macct; do 
        echo "Hole ${TARGET_FILESET[$key]} von"
        echo "     ${CONF_CLOOP_SERVER}/${SOURCE_FILESET[$key]}"
        if wget ${CONF_CLOOP_SERVER}/${SOURCE_FILESET[$key]} -O ${TARGET_FILESET[$key]}; then 
            colorecho "green" "Success."
        else 
            colorecho "red" "Failed"
            rm -f ${TARGET_FILESET[$key]}
        fi
    done
}


#
# @Agrunmente
# $1  Name der cloop-Datei, die eingerichtet werden soll
#
function configure_cloop {
    # Gibt es das Cloop?
    if [ ! -e $CONF_LINBODIR/$1 ]; then 
        echo "Cloop Datei nicht gefunden: $CONF_LINBODIR/$1"
        exit 1
    fi
    HWCLASS=${1%.cloop}
    STARTCONF=$CONF_LINBODIR/start.conf.$HWCLASS
    if [ ! -e $STARTCONF ]; then 
        echo "Keine start.conf Vorlage für $HWCLASS gefunden"
        exit 1
    fi
    ts=$(date +%Y%m%d-%H%M%S)
    cp $STARTCONF ${STARTCONF}.$ts.autobackup

    # start.conf anpassen
    sed -i "s/\(Server\s*\=\s*\) \(.*\)/\1 $serverip/" $STARTCONF
    sed -i "s/\(Group\s*\=\s*\) \(.*\)/\1 $HWCLASS/" $STARTCONF
    sed -i "s/\(BaseImage\s*\=\s*\) \(.*\)/\1 $1/" $STARTCONF

    BITTORRENT_ON=$(grep START_BTTRACK /etc/default/bittorrent  | awk -F= '{print $2}')
    if [ "x$BITTORRENT_ON" == "x0" ]; then 
        sed -i "s/\(DownloadType\s*\=\s*\) \(.*\)/\1 rsync/" $STARTCONF
    fi


    # postsync aus vorlage holen
    POSTSYNC=$CONF_LINBODIR/$1.postsync
    cp $CONF_GENERIC_POSTSYNC $POSTSYNC
    mkdir -p /var/linbo/linuxmuster-client/$PATCHCLASS/common/
    cp -ar /var/linbo/linuxmuster-client/_templates/postsync.d /var/linbo/linuxmuster-client/$PATCHCLASS/common/
    sed -i "s/\(PATCHCLASS\s*\=\s*\)\(.*\)/\1\"$PATCHCLASS\"/" $POSTSYNC

    # postsync konfiguration anpassen

    # linuxadmin-Passworthash aus der Konfiguration bestimmen und für das postsync Skript bereitstellen
    PWHASH=$(echo "$CONF_LINUXADMIN_PW" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
    echo "linuxadmin|$PWHASH" > /var/linbo/linuxmuster-client/$PATCHCLASS/common/passwords
    
    # public-key des Server-roots in die authorized keys der client roots
    mkdir -p  /var/linbo/linuxmuster-client/$PATCHCLASS/common/root/.ssh
    cat /root/.ssh/id_dsa.pub > /var/linbo/linuxmuster-client/$PATCHCLASS/common/root/.ssh/authorized_keys
    
 

    

    
    

}
