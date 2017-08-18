#include "hue.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <curl/curl.h>
#include <zconf.h>

namespace bduer {
    Hue* Hue::instance = NULL;
    void (*Hue::_log_function)(int log_level, const char *log_content);
    char Hue::_log_buffer[LOG_BUFFER_SIZE];
    unsigned char Hue::_data_buffer[DATA_BUFFER_SIZE];

    pthread_t Hue::pid;
    int Hue::_multicast_send_socket;
    int Hue::_multicast_receive_socket;
    int Hue::_unicast_receive_socket;
    bool Hue::_searchingIpBridge = false;
    char Hue::_ipBridgeAddress[20];
    char Hue::_newIpBridgeAddress[20];
    char Hue::_userName[USER_NAME_SIZE];
    char Hue::_url_buffer[URL_BUFFER_SIZE];
    char Hue::_body_buffer[BODY_BUFFER_SIZE];
    char Hue::_response_buffer[RESPONSE_BUFFER_SIZE];

    Hue::Hue() {
    }

    Hue* Hue::get_instance() {
        if(instance == NULL) {
            return new Hue();
        }
        return instance;
    }

    void Hue::init(void (*log_function)(int log_level, const char *log_content) = NULL) {
        _log_function = log_function;
        loadDefaultConfiguration();
        if(!init_sockets()) {
            return;
        }
    }

    void Hue::loadDefaultConfiguration() {
        sprintf(_ipBridgeAddress, "%s", "192.168.1.172");
        sprintf(_userName, "%s", "RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB");
    }

    bool Hue::init_sockets() {
        int enabled = 1;
        _multicast_send_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (_multicast_send_socket < 0) {
            log(LOG_ERROR, "_multicast_send_socket Error to create socket");
            return false;
        }
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(12345);
        if(bind(_multicast_send_socket, (struct sockaddr *)&address, sizeof(struct sockaddr)) == -1){
            log(LOG_ERROR, "_multicast_send_socket listen_socket bind fail");
            return false;
        }

        _multicast_receive_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (_multicast_receive_socket < 0) {
            log(LOG_ERROR, "_multicast_receive_socket Error to create socket");
            return false;
        }
        if(setsockopt(_multicast_receive_socket, SOL_SOCKET, SO_REUSEADDR, (char *) &enabled, sizeof(enabled)) < 0) {
            log(LOG_ERROR, "_multicast_receive_socket Error to setsockopt SO_REUSEADDR, reason = %s", strerror(errno));
            return false;
        }
        struct sockaddr_in receive_address;
        receive_address.sin_family = AF_INET;
        receive_address.sin_addr.s_addr = htonl(INADDR_ANY);
        receive_address.sin_port = htons(UPNP_PORT);
        if(bind(_multicast_receive_socket, (struct sockaddr *)&receive_address, sizeof(struct sockaddr)) == -1){
            log(LOG_ERROR, "_multicast_receive_socket listen_socket bind fail");
            return false;
        }
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr("239.255.255.250");
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);
        if(setsockopt(_multicast_receive_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))) {
            log(LOG_ERROR, "_multicast_receive_socket IP_ADD_MEMBERSHIP error");
            return false;
        }
        return true;
    }

    bool Hue::searchIpBridge(int timeout, char *ipBridgeAddress) {
        if(ipBridgeAddress == NULL) {
            return false;
        }
        memset(_newIpBridgeAddress, 0, sizeof(_newIpBridgeAddress));
        time_t start, end;
        time(&start);
        pthread_create(&pid, NULL, receive_thread, NULL);
        char ssdpDiscover[] = "M-SEARCH * HTTP/1.1\r\n"
                "HOST: 239.255.255.250:1900\r\n"
                "ST: upnp:all\r\n"
                "MAN: \"ssdp:discover\"\r\n"
                "MX: 3\r\n";
        _searchingIpBridge = true;
        while(_searchingIpBridge) {
            send_multicast_packet((unsigned char *)ssdpDiscover, strlen(ssdpDiscover));
            time(&end);
            if(difftime(end, start) > timeout) {
                log(LOG_DEBUG, "Searching IpBridge timeout");
                break;
            }
            usleep(100);
        }
        if(strlen(_newIpBridgeAddress) > 0) {
            strcpy(ipBridgeAddress, _newIpBridgeAddress);
            log(LOG_DEBUG, "Searching IpBridge done, the ip is %s", ipBridgeAddress);
            return true;
        } else {
            return false;
        }
    }

    void Hue::setIpBridge(char *ipBridgeAddress) {
        strcpy(_ipBridgeAddress, ipBridgeAddress);
    }

    void* Hue::receive_thread(void *arg) {
        unsigned char buf[4096];
        while(true) {
            receive_unicast_packet(buf, sizeof(buf));
            if(strstr((char *)buf, "IpBridge")) {
                int ip_start_index = 0;
                int ip_end_index = 0;
                char token[] = "LOCATION: http://";
                int offset;
                string s((char *)buf);
                offset = s.find(token);
                if(offset < 0) {
                    continue;
                }
                ip_start_index = offset + strlen(token);
                ip_end_index = s.find(":", ip_start_index);
                strcpy(_newIpBridgeAddress, s.substr(ip_start_index, ip_end_index - ip_start_index).c_str());
                _searchingIpBridge = false;
                break;
            }
        }
    }

    void Hue::log(int log_level, const char *fmt,...) {
        va_list ap;
        va_start(ap, fmt);
        vsprintf(_log_buffer, fmt, ap);
        va_end(ap);
        if(_log_function) {
            _log_function(log_level, _log_buffer);
        }
    }

    void Hue::log_hex_dump(int log_level, const unsigned char *data, const int data_length) {
        for(int i=0; i < data_length; i++) {
            sprintf(_log_buffer + i*3, "%02X ", data[i]);
        }
        _log_buffer[data_length * 3] = 0;
        if(_log_function) {
            _log_function(log_level, _log_buffer);
        }
    }

    int Hue::send_multicast_packet(const unsigned char *data, const int data_length) {
        struct sockaddr_in broadcard_address;
        broadcard_address.sin_family = AF_INET;
        broadcard_address.sin_addr.s_addr = inet_addr("239.255.255.250");
        broadcard_address.sin_port = htons(UPNP_PORT);

        int bytes_sent = 0;
        bytes_sent = sendto(_multicast_send_socket, data, data_length, 0, (struct sockaddr *)&broadcard_address, sizeof(struct sockaddr));
        if (bytes_sent > 0 ) {
            //log(LOG_INFO, "%d bytes have been send out", bytes_sent);
            //log_hex_dump(LOG_DEBUG, data, data_length);
        } else {
            log(LOG_ERROR, "Failed to send data(%d bytes), return value of sendto() is %d, reason = %s", data_length, bytes_sent, strerror(errno));
        }
    }

    int Hue::receive_multicast_packet(unsigned char *data, const int data_length) {
        int bytes_received = 0;
        if ((bytes_received = recv(_multicast_receive_socket, data, data_length, 0)) > 0 ) {
            //log(LOG_INFO, "%d bytes have been received", bytes_received);
            //log_hex_dump(LOG_DEBUG, data, bytes_received);
        } else {
            log(LOG_ERROR, "Failed to receive data, reason = %s", strerror(errno));
        }
        return bytes_received;
    }

    int Hue::receive_unicast_packet(unsigned char *data, const int data_length) {
        int bytes_received = 0;
        if ((bytes_received = recv(_multicast_send_socket, data, data_length, 0)) > 0 ) {
            //log(LOG_INFO, "%d bytes have been received", bytes_received);
            //log_hex_dump(LOG_DEBUG, data, bytes_received);
        } else {
            log(LOG_ERROR, "Failed to receive data, reason = %s", strerror(errno));
        }
        return bytes_received;
    }


    size_t Hue::curl_process_data(void *data, size_t size, size_t nmemb, string &content) {
        long sizes = size * nmemb;
        string temp;
        temp = string((char*)data,sizes);
        content += temp;
        return sizes;
    }

    bool Hue::curl_customer_request(char *method, char *url, char *body, char *response) {
        string responseContent;
        CURL *curl;
        CURLcode res;
        curl = curl_easy_init();
        if(curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_process_data);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseContent);
            res = curl_easy_perform(curl);
            if (res == CURLE_OK) {
                log(LOG_DEBUG, responseContent.c_str());
                if (response != NULL) {
                    strcpy(response, responseContent.c_str());
                }
                if(strstr(responseContent.c_str(),"success") == NULL) {
                    return false;
                }
            } else {
                log(LOG_ERROR, curl_easy_strerror(res));
                return false;
            }
            curl_easy_cleanup(curl);
        }
        return true;
    };

    bool Hue::getJsonStringValue(const char *content, const char *tag, char *value) {
        int offset = 0;
        int value_start_index = 0;
        int value_end_index = 0;
        string s(content);
        offset = s.find(tag);
        if(offset < 0) {
            return false;
        }
        offset += strlen(tag) + strlen("\""); //skip right " of tag
        value_start_index = s.find("\"", offset) + 1; //skip left " of value
        value_end_index = s.find("\"", value_start_index);
        strcpy(value, s.substr(value_start_index, value_end_index - value_start_index).c_str());
    }

    void Hue::getCongiguration() {
        char method[] = "GET";
        sprintf(_url_buffer, "http://%s/api/%s/config", _ipBridgeAddress, _userName);
        sprintf(_body_buffer, "{\"on\":true}");
        curl_customer_request(method, _url_buffer, _body_buffer, NULL);
    }

    bool Hue::create_user(char *autoGeneratedUserName) {
        /* username list
         * RN4xQqwDXECau-BztF0GqBUB1ABGCQKJMxEoGAJB
         * B0G1Zvc6RvDO66R9pqIqB3LeMNs9yfwWr1CqOSQI
         * HH8E2YIDgealIYHgU0Zr5v0qr0qn3sNnuku5TBN3
         */
        bool result;
        char method[] = "POST";
        sprintf(_url_buffer, "http://%s/api", _ipBridgeAddress);
        sprintf(_body_buffer, "{\"devicetype\":\"hue#ubuntu\"}");
        result = curl_customer_request(method, _url_buffer, _body_buffer, _response_buffer);
        if(result == true) {
            char username[100];
            getJsonStringValue(_response_buffer, "username", username);
            log(LOG_DEBUG, "User created, the auto generated username is:%s",username);
            if(autoGeneratedUserName != NULL) {
                strcpy(autoGeneratedUserName, username);
            }
        }

        return result;
    }

    bool Hue::turn_on_light(int light_number) {
        char method[] = "PUT";
        sprintf(_url_buffer, "http://%s/api/%s/lights/%d/state", _ipBridgeAddress, _userName, light_number);
        sprintf(_body_buffer, "{\"on\":true}");
        return curl_customer_request(method, _url_buffer, _body_buffer, NULL);
    }

    bool Hue::turn_off_light(int light_number) {
        char method[] = "PUT";
        sprintf(_url_buffer, "http://%s/api/%s/lights/%d/state", _ipBridgeAddress, _userName, light_number);
        sprintf(_body_buffer, "{\"on\":false}");
        return curl_customer_request(method, _url_buffer, _body_buffer, NULL);
    }

    bool Hue::set_light_brightness(int light_number, int brightness) {
        char method[] = "PUT";
        sprintf(_url_buffer, "http://%s/api/%s/lights/%d/state", _ipBridgeAddress, _userName, light_number);
        sprintf(_body_buffer, "{\"bri\":%d}", brightness);
        return curl_customer_request(method, _url_buffer, _body_buffer, NULL);
    }

    bool Hue::set_light_color(int light_number, int color) {
        char method[] = "PUT";
        sprintf(_url_buffer, "http://%s/api/%s/lights/%d/state", _ipBridgeAddress, _userName, light_number);
        sprintf(_body_buffer, "{\"hue\":%d}", color);
        return curl_customer_request(method, _url_buffer, _body_buffer, NULL);
    }
}
