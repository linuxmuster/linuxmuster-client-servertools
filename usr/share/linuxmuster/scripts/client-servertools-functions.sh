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
cat#  jesko.anschuetz@linuxmuster.net
#  frank@linuxmuster.net
 <<End-of-help
Anwendung:
    linuxmuster-manage-cloop [-a action]
Aktionsauswahl:
    -a auto     Automatischer Modus, versucht das cloop aus dem Netz vollautomatisch zu installieren
    -a get-cloop Holt ein cloop aus dem Netz
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
