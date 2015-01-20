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
    $0 [-a action]
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
    -a update-available
        Holt eine Liste verfügbarer cloops vom server
    -a list-available
        Listet die verfügbaren cloops auf. Aktualisiert zuvor die Liste der verfügbaren 
        cloops vom Server  

Optionen:
    -h <Hardware-Klasse>  
        Legt unter verwendung des heruntergeladenen cloops 
        eine Konfiguration mit dem angegebenen Namen an.
        Standard ist "ubuntuclient"
    -c <cloop-Dateiname>
        Kann bei der Aktion "configure" angegeben werden. Name der 
        cloop-Datei ohne den Pfad /var/linbo
    -p <patchklasse> 
        Legt die Standard-Postyncpatches im UVZ /var/linbo/linuxmuster-client/<patchklasse> an.

Beispiele:

 $0 -a configure -h myubuntu -p mypatches -c trusty714.cloop

 Konfiguriert das vorhandene Cloop trusty714.cloop als 
 linbo-Grupp "myubuntu" mit einem universellen postsync Skript, 
 verwendet dafür das Patchklassenverzeichnis 
 /var/linbo/linuxmuster-client/mypatches 
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
# Aktualisiert die Liste der aktuell verfügbaren Online-Images
# 
function get_available_images {
    if [ -d /var/cache/linuxmuster-client-servertools/ ]; then 
        rm -rf /var/cache/linuxmuster-client-servertools/
    fi
    echo  -n "Hole Liste der verfügbaren cloops..."
    wget --mirror -A.txt -np -P /var/cache/linuxmuster-client-servertools http://cloop.linuxmuster.net/meta/ > /dev/null 2>&1 && colorecho "green" "OK"
}

# 
# Listet die  aktuell verfügbaren Online-Images auf
# 
function list_available_images {
    get_available_images
    desc_files=$(find /var/cache/linuxmuster-client-servertools/ -name 'description.txt')
    echo 
    echo "Imagename                 Info"
    echo "-----------------------------------------------"
    for desc in $desc_files; do
        name=$(grep ^Name: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        info=$(grep ^Info: $desc | awk -F: '{print $2}' | sed "s/^[ \t]*//")
        echo "$name                          $info"
    done
    echo "-----------------------------------------------"
    echo 
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
    cloop_name=${2%.cloop}
    get_source_fileset $cloop_name
    check_target_fileset

    for key in startconf cloop postsync desc macct; do 
        echo "Hole ${TARGET_FILESET[$key]} von"
        echo "     ${CONF_CLOOP_SERVER}/cloops/$cloop_name/${SOURCE_FILESET[$key]}"
        if wget ${CONF_CLOOP_SERVER}/cloops/$cloop_name/${SOURCE_FILESET[$key]} -O ${TARGET_FILESET[$key]}; then 
            colorecho "green" "Success."
        else 
            colorecho "red" "Failed"
            rm -f ${TARGET_FILESET[$key]}
        fi
    done
}


#
# @Argunmente
# $1  Name der cloop-Datei, die eingerichtet werden soll
#
function configure_cloop {
    # Gibt es das Cloop?
    if [ ! -e $CONF_LINBODIR/$1 ]; then 
        echo "Cloop Datei nicht gefunden: $CONF_LINBODIR/$1"
        exit 1
    fi

    echo "INFO: Cloop-Datei ist $CONF_LINBODIR/$1"

    CLOOP_HWCLASS=${1%.cloop}
    STARTCONF_SRC=$CONF_LINBODIR/start.conf.$CLOOP_HWCLASS
    if [ ! -e $STARTCONF_SRC ]; then 
        echo "ERROR: Keine start.conf Vorlage für cloop $CLOOP_HWCLASS gefunden"
        exit 1
    fi

    if [ $HWCLASS == "" ]; then
        HWCLASS=$CLOOP_HWCLASS
    fi
    
    STARTCONF=$CONF_LINBODIR/start.conf.$HWCLASS
    echo "INFO: start.conf Quelle ist $STARTCONF_SRC"
    echo "INFO: start.conf Ziel ist $STARTCONF"
    ts=$(date +%Y%m%d-%H%M%S)
    if [ -f $STARTCONF ]; then
        echo "INFO: Sichere $STARTCONF nach $STARTCONF ${STARTCONF}.$ts.autobackup"
        cp $STARTCONF ${STARTCONF}.$ts.autobackup
    fi
    cp $STARTCONF_SRC $STARTCONF

    echo "INFO: Passe $STARTCONF an"

    # start.conf anpassen
    sed -i "s/\(Server\s*\=\s*\) \(.*\)/\1 $serverip/" $STARTCONF
    sed -i "s/\(Group\s*\=\s*\) \(.*\)/\1 $HWCLASS/" $STARTCONF
    sed -i "s/\(BaseImage\s*\=\s*\) \(.*\)/\1 $1/" $STARTCONF

    # Imageverteilung via rsync oder ist bittorrent enabled?
    BITTORRENT_ON=$(grep START_BTTRACK /etc/default/bittorrent  | awk -F= '{print $2}')
    if [ "x$BITTORRENT_ON" == "x0" ]; then 
        sed -i "s/\(DownloadType\s*\=\s*\) \(.*\)/\1 rsync/" $STARTCONF
    fi


    echo "INFO: Erstelle postsync aus Vorlage"
    # postsync aus vorlage holen
    POSTSYNC=$CONF_LINBODIR/$1.postsync
    cp $CONF_GENERIC_POSTSYNC $POSTSYNC

    if [ $CONF_HOSTGROUP_AS_PATCHCLASS != 0 ]; then 
        # Patchklasse aus config und Kommandozeile wird überschrieben
        # wenn die Konfiguration die Hostgruppe als Patchklasse erzwingt
        echo "WARN: Konfiguration erzwingt Hardwareklasse als Patchklasse"
        PATCHCLASS=$HWCLASS
    fi
    echo "INFO: Patchclasse ist $PATCHCLASS"

    # Gibt es das Patchklassenverzeichnise schon?
    # Wenn ja: Sichern!
    if [ -d /var/linbo/linuxmuster-client/$PATCHCLASS ]; then 
        echo "WARN: Sichere das vorhandene Patchklassenverzeichnis"
        echo "      /var/linbo/linuxmuster-client/$PATCHCLASS  nach"
        echo "      /var/linbo/linuxmuster-client/$PATCHCLASS.$ts.autobackup"
        mv /var/linbo/linuxmuster-client/$PATCHCLASS /var/linbo/linuxmuster-client/$PATCHCLASS.$ts.autobackup
    fi

    mkdir -p /var/linbo/linuxmuster-client/$PATCHCLASS/common/
    cp -ar /var/linbo/linuxmuster-client/_templates/postsync.d /var/linbo/linuxmuster-client/$PATCHCLASS/common/
    sed -i "s/\(PATCHCLASS\s*\=\s*\)\(.*\)/\1\"$PATCHCLASS\"/" $POSTSYNC

    # Netzwerksettings in den postsync-pfad
    mkdir -p /var/linbo/linuxmuster-client/$PATCHCLASS/common/etc/linuxmuster-client/
    cp  /var/lib/linuxmuster/network.settings /var/linbo/linuxmuster-client/$PATCHCLASS/common/etc/linuxmuster-client/server.network.settings

    # postsync konfiguration anpassen
    # linuxadmin-Passworthash aus der Konfiguration bestimmen und für das postsync Skript bereitstellen
    PWHASH=$(echo "$CONF_LINUXADMIN_PW" | makepasswd --clearfrom=- --crypt-md5 |awk '{ print $2 }')
    echo "linuxadmin|$PWHASH" > /var/linbo/linuxmuster-client/$PATCHCLASS/common/passwords
    
    # public-key des Server-roots in die authorized keys der client roots
    mkdir -p  /var/linbo/linuxmuster-client/$PATCHCLASS/common/root/.ssh
    cat /root/.ssh/id_dsa.pub > /var/linbo/linuxmuster-client/$PATCHCLASS/common/root/.ssh/authorized_keys
    
}
