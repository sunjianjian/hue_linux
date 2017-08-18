#include "hue.h"
#include <stdio.h>

using bduer::Hue;

void log(int log_level, const char *log_content) {
    printf("%s\n", log_content);
}

int main() {
    char ipBridgeAddress[20];

    Hue *sc = Hue::get_instance();
    sc->init(log);
    sc->searchIpBridge(5, ipBridgeAddress);
    sc->setIpBridge(ipBridgeAddress);
    sc->getCongiguration();
    //sc->create_user(NULL);
    sc->turn_on_light(3);
    sc->set_light_brightness(3, 0);
    sc->set_light_color(3, 65280);
    sc->turn_off_light(3);
    return 0;
}

/*
1. Response to SSDP discover
HTTP/1.1 200 OK
HOST: 239.255.255.250:1900
EXT:
CACHE-CONTROL: max-age=100
LOCATION: http://192.168.1.172:80/description.xml
SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.16.0
hue-bridgeid: 001788FFFE466A99
ST: urn:schemas-upnp-org:device:basic:1
USN: uuid:2f402f80-da50-11e1-9b23-001788466a99

2. create user
curl -X POST -d '{"devicetype":"hue#ubuntu"}' http://192.168.1.172/api
[{"success":{"username":"RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB"}}]
[{"error":{"type":101,"address":"","description":"link button not pressed"}}]

3. on/off
curl -X PUT -d '{"on":false}' http://192.168.1.172/api/RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB/lights/3/state
[{"success":{"/lights/3/state/on":false}}]
curl -X PUT -d '{"on":true}' http://192.168.1.172/api/RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB/lights/3/state
[{"success":{"/lights/3/state/on":true}}]
[{"error":{"type":3,"address":"/lights/4/state","description":"resource, /lights/4/state, not available"}}]

4. brightness
curl -X PUT -d '{"bri":100}' http://192.168.1.172/api/RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB/lights/3/state
[{"success":{"/lights/3/state/bri":100}}]
 */