/*
 * hap.c
 *
 *  Created on: Apr 15, 2015
 *      Author: Andrei
 */

#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "util.h"
#include "hap.h"
#include "mqtt/mqtt.h"
#include "settings.h"
#include "mem.h"

#include "httpd.h"
#include "config.h"

static MQTT_Client mqttClient;
static hapMqttCallback mqttConnected, mqttDisconnected, mqttPublished;
static hapMqttDataCallback mqttData;

static void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args)
{
    DEBUG_PRINT("[MQTT]Connected\n");
    MQTT_Client* client = (MQTT_Client*)args;

    if (mqttConnected) mqttConnected(client);
}

static void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    DEBUG_PRINT("[MQTT]Disconnected\n");
    if (mqttDisconnected) mqttDisconnected(client);
}

static void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    DEBUG_PRINT("[MQTT]Published\n");
    if (mqttPublished) mqttPublished(client);
}

static void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    char *topicBuf = (char*)os_zalloc(topic_len+1);
    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    DEBUG_PRINT("[MQTT]Data on topic: %s, length: %d\n", topicBuf, data_len);

    if (mqttData) mqttData(client, topicBuf, data, data_len);
    os_free(topicBuf);
}

static void ICACHE_FLASH_ATTR udp_received(void *arg, char *data, unsigned short len)
{
    struct espconn *udpconn= (struct espconn*)arg;
    if (len > 5 && strncmp(data, "HAP", 3) == 0)
    {
        const char* hapServer = &data[5];
        if (strcmp(settings.serverName, hapServer) != 0)
            return;

        static uint16_t hapPort;
        static uint32_t hapAddress = 0;

        remot_info *info = NULL;
        espconn_get_connection_info(udpconn, &info, 0);

        uint16_t port = (data[3] << 8) | data[4];
        uint32_t address = *(uint32_t*)info->remote_ip;

        if (port == hapPort && address == hapAddress)
            return;

        hapAddress = address;
        hapPort = port;

        static bool inited = false;

        DEBUG_PRINT("[HAP]Discover from "IPSTR":%d\n", IP2STR(&address), port);

        if (inited)
        {
            MQTT_Disconnect(&mqttClient);
            DEBUG_PRINT("[HAP]Disconnect MQTT\n");
        }

        DEBUG_PRINT("[HAP]Initializing MQTT ("IPSTR":%d)\n", IP2STR(&address), hapPort);

        char aux[20];
        os_sprintf(aux, IPSTR, IP2STR(&address));
        MQTT_InitConnection(&mqttClient, aux, hapPort);
        MQTT_InitClient(&mqttClient, settings.nodeName, settings.hapUserName, settings.hapPassword, MQTT_KEEPALIVE, 1);
        MQTT_OnConnected(&mqttClient, mqttConnectedCb);
        MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
        MQTT_OnPublished(&mqttClient, mqttPublishedCb);
        MQTT_OnData(&mqttClient, mqttDataCb);
        MQTT_Connect(&mqttClient);

        inited = true;
    }
}

static void ICACHE_FLASH_ATTR udp_init()
{
    static struct espconn udpServer;
    static esp_udp udp;
    udpServer.type = ESPCONN_UDP;
    udpServer.state = ESPCONN_NONE;
    udpServer.proto.udp = &udp;
    udpServer.proto.udp->local_port = settings.udpPort;

    if (espconn_create(&udpServer) == 0)
    {
        espconn_regist_recvcb(&udpServer, udp_received);
        DEBUG_PRINT("[HAP]Started UDP server\n");
    }
}


bool ICACHE_FLASH_ATTR index_httpd_request(struct HttpdConnectionSlot *slot, uint8_t verb, char* path, uint8_t *data, uint16_t length);

bool ICACHE_FLASH_ATTR hap_init()
{
    settings_load();
    httpd_register(index_httpd_request);

    bool result;

    if (!settings_valid())
    {
        DEBUG_PRINT("[HAP]Settings not valid, using defaults, starting AP\n");

        settings.password[0] = 0;
        settings.ssid[0] = 0;

        strcpy(settings.serverName, "hap_server");
        strcpy(settings.hapUserName, "user");
        strcpy(settings.hapPassword, "password");
        settings.udpPort = 5100;
        os_sprintf(settings.nodeName, "hap_%d", system_get_chip_id());
        result = setup_wifi_ap_mode(settings.nodeName);
    }
    else
    {
        DEBUG_PRINT("[HAP]Settings valid, connecting to AP %s\n", settings.ssid);

        udp_init();
        result = setup_wifi_st_mode(settings.ssid, settings.password);
    }

    if (result) httpd_init(80);

    return result;
}

void user_rf_pre_init(void)
{
	//system_phy_set_rfoption(2);
}

void ICACHE_FLASH_ATTR hap_setConnectedCb(hapMqttCallback callback)         { mqttConnected = callback; }
void ICACHE_FLASH_ATTR hap_setDisconnectedCb(hapMqttCallback callback)      { mqttDisconnected = callback; }
void ICACHE_FLASH_ATTR hap_setPublishedCb(hapMqttCallback callback)         { mqttPublished = callback; }
void ICACHE_FLASH_ATTR hap_setDataReceivedCb(hapMqttDataCallback callback)  { mqttData = callback; }
