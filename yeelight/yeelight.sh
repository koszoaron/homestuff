#!/bin/bash

YEELIGHT_IP=10.121.1.71
YEELIGHT_PORT=55443
NC_QUIT_SEC=1

SCENE_HANDWORK=("ct" 3400 90)
SCENE_SUNRISE=("cf" 3 1 "50,1,16731136,1,360000,2,1700,10,540000,2,2700,100")
SCENE_SUNSET=("cf" 3 2 "50,2,2700,10,180000,2,1700,5,420000,1,16731136,1")
SCENE_ROMANTIC=("cf" 0 1 "4000,1,5838189,1,4000,1,6689834,1")
SCENE_NIGHT=("color" 16750848 1)
SCENE_HOME=("ct" 3200 80)
SCENE_MOVIE=("color" 1315890 50)
SCENE_DATING=("color" 16737792 50)
SCENE_BDAY=("cf" 0 1 "2000,1,14438425,80,2000,1,14448670,80,2000,1,11153940,80")
SCENE_WHITISH=("ct" 3550 100)

ONOFF_FADE_TIME_MS=500

function send_to_yeelight {
    echo -ne "$1\r\n" | nc $YEELIGHT_IP $YEELIGHT_PORT -q $NC_QUIT_SEC
}

function call_yeelight_method {
    send_to_yeelight "{\"id\":1, \"method\":\"$1\", \"params\":[$2]}"
}

function set_scene {
    case $1 in
    "Working")
	    STR="\"${SCENE_HANDWORK[0]}\", ${SCENE_HANDWORK[1]}, ${SCENE_HANDWORK[2]}"
        ;;
    "Sunrise")
        STR="\"${SCENE_SUNRISE[0]}\", ${SCENE_SUNRISE[1]}, ${SCENE_SUNRISE[2]}, \"${SCENE_SUNRISE[3]}\""
        ;;
    "Sunset")
    	STR="\"${SCENE_SUNSET[0]}\", ${SCENE_SUNSET[1]}, ${SCENE_SUNSET[2]}, \"${SCENE_SUNSET[3]}\""
        ;;
    "Romantic")
	    STR="\"${SCENE_ROMANTIC[0]}\", ${SCENE_ROMANTIC[1]}, ${SCENE_ROMANTIC[2]}, \"${SCENE_ROMANTIC[3]}\""
        ;;
    "Night")
	    STR="\"${SCENE_NIGHT[0]}\", ${SCENE_NIGHT[1]}, ${SCENE_NIGHT[2]}"
        ;;
    "Home")
	    STR="\"${SCENE_HOME[0]}\", ${SCENE_HOME[1]}, ${SCENE_HOME[2]}"
        ;;
    "Movie")
	    STR="\"${SCENE_MOVIE[0]}\", ${SCENE_MOVIE[1]}, ${SCENE_MOVIE[2]}"
        ;;
    "Date")
	    STR="\"${SCENE_DATING[0]}\", ${SCENE_DATING[1]}, ${SCENE_DATING[2]}"
        ;;
    "Birthday")
	    STR="\"${SCENE_BDAY[0]}\", ${SCENE_BDAY[1]}, ${SCENE_BDAY[2]}, \"${SCENE_BDAY[3]}\""
        ;;
    "Bright")
	    STR="\"${SCENE_WHITISH[0]}\", ${SCENE_WHITISH[1]}, ${SCENE_WHITISH[2]}"
        ;;
    *)
	echo "Unknown scene"
	exit -1
esac

    call_yeelight_method "set_scene" "$STR"
}

if [ "$#" -lt 1 ]; then
    echo "Wrong number of arguments!"
    exit -1
fi

if [ $1 = "off" ]; then
    call_yeelight_method "set_power" "\"off\", \"smooth\", $ONOFF_FADE_TIME_MS"
elif [ $1 = "on" ]; then
    call_yeelight_method "set_power" "\"on\", \"smooth\", $ONOFF_FADE_TIME_MS"
elif [ $1 = "raw" ]; then
    send_to_yeelight "$2"
elif [ $1 = "scene" ]; then
    set_scene "$2"
else
    echo "Unknown command"
    exit -1
fi

