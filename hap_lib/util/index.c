#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"
#include "httpd.h"
#include "settings.h"

#define KEY_SSID    "key"
#define KEY_PASS    "password"
#define KEY_SERVER  "server"
#define KEY_TOPIC   "topic"
#define KEY_PORT    "port"
#define KEY_MQ_USR  "mquser"
#define KEY_MQ_PASS "mqpass"

const char * config_index =
"<!doctype html><html lang='en'><head><title>HAP Configration</title></head>\
<body style='font-family:arial'>\
<h2>Config</h2>\
<form method='POST'>\
<h3>Wi-Fi</h3>\
SSID<br/><input type='text' name='"KEY_SSID"' value='%s' required/><br/>\
Password<br/><input type='password' name='"KEY_PASS"' value='%s' required/><br/>\
<h3>HAP</h3>\
Server Name<br/><input type='text' name='"KEY_SERVER"' value='%s' pattern='^[a-zA-Z0-9_]+$' required /><br/>\
Port<br/><input type='text' name='"KEY_PORT"' value='%d' required\
pattern='^([0-9]{1,4}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$'/><br/>\
MQTT User<br/><input type='text' name='"KEY_MQ_USR"' value='%s' pattern='^[a-zA-Z0-9_]+$' required /><br/>\
MQTT Password<br/><input type='text' name='"KEY_MQ_PASS"' value='%s' required /><br/>\
Topic<br/><input type='text' name='"KEY_TOPIC"' value='%s' required/><br/>\
<input type='submit' value='Save'/></form></body></html>";

static void ICACHE_FLASH_ATTR restart(void *arg)
{
    system_restart();
}

static void ICACHE_FLASH_ATTR urldecode2(char *dst, const char *src)
{
    char a, b;
    while (*src)
    {
        if ((*src == '%') && ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b)))
        {
            if (a >= 'a')
                a -= 'a'-'A';
            if (a >= 'A')
                a -= ('A' - 10);
            else
                a -= '0';
            if (b >= 'a')
                b -= 'a'-'A';
            if (b >= 'A')
                b -= ('A' - 10);
            else
                b -= '0';
            *dst++ = 16*a+b;
            src+=3;
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

static bool ICACHE_FLASH_ATTR handleSettingsParameter(const char* key, const char* value)
{
    if (strcmp(key, KEY_SSID) == 0)
    {
        if (strcmp(settings.ssid, value))
        {
            os_strcpy(settings.ssid, value);
            return true;
        }
    }
    else if (strcmp(key, KEY_PASS) == 0)
    {
        if (strcmp(settings.password, value))
        {
            os_strcpy(settings.password, value);
            return true;
        }
    }
    else if (strcmp(key, KEY_SERVER) == 0)
    {
        if (strcmp(settings.serverName, value))
        {
            os_strcpy(settings.serverName, value);
            return true;
        }
    }
    else if (strcmp(key, KEY_PORT) == 0)
    {
        int port = atoi(value);
        if (settings.udpPort != port)
        {
            settings.udpPort = port;
            return true;
        }
    }
    else if (strcmp(key, KEY_TOPIC) == 0)
    {
        if (strcmp(settings.mqttTopic, value))
        {
            os_strcpy(settings.mqttTopic, value);
            return true;
        }
    }
    else if (strcmp(key, KEY_MQ_USR) == 0)
    {
        if (strcmp(settings.mqttUser, value))
        {
            os_strcpy(settings.mqttUser, value);
            return true;
        }
    }
    else if (strcmp(key, KEY_MQ_PASS) == 0)
    {
        if (strcmp(settings.mqttPassword, value))
        {
            os_strcpy(settings.mqttPassword, value);
            return true;
        }
    }

    return false;
}

bool ICACHE_FLASH_ATTR index_httpd_request(struct HttpdConnectionSlot *slot, uint8_t verb, char* path, uint8_t *data, uint16_t length)
{
    if (verb == HTTPD_VERB_POST)
    {
        if (strcasecmp(path, "/") == 0)
        {
            uint8_t pos = 0;
            char *key = data, *value;
            char decoded[30];
            bool changed = false;
            while (true)
            {
                if (pos == length || data[pos] == '&')
                {
                    data[pos] = 0;
                    urldecode2(decoded, value);
                    changed |= handleSettingsParameter(key, decoded);
                    if (pos < length)
                        key = &data[++pos];
                    else
                        break;
                }
                else if (data[pos] == '=')
                {
                    data[pos] = 0;
                    value = &data[pos+1];
                }
                else
                    pos++;
            }

            if (changed)
            {
                settings_save();
                httpd_send_html(slot, 200, "Settings saved. Do a <a href='/reset'>reset</a> to apply them!");
            }
            else
            {
                httpd_send_text(slot, 200, "Settings did not change");
            }
        }
        else
            return false;

        return true;
    }

    if (strcasecmp(path, "/") == 0)
    {
        httpd_send_html(slot, 200, config_index,
                settings.ssid,
				settings.password,
                settings.serverName,
                settings.udpPort,
                settings.mqttUser,
                settings.mqttPassword,
				settings.mqttTopic);
    }
    else if (strcasecmp(path, "/clear") == 0)
    {
        if (settings.magicNumber == MAGIC_NUMBER)
        {
            settings_clear();
            httpd_send_text(slot, 200, "Settings cleared");
        }
        else
        {
            httpd_send_text(slot, 200, "No valid settings to clear");
        }
    }
    else if (strcasecmp(path, "/id") == 0)
    {
        httpd_send_text(slot, 200, "%d", system_get_chip_id());
    }
    else if (strcasecmp(path, "/heap") == 0)
    {
        httpd_send_text(slot, 200, "%d", system_get_free_heap_size());
    }
    else if (strcasecmp(path, "/reset") == 0)
    {
        httpd_send_text(slot, 200, "Resetting in 1sec");
        static ETSTimer restarttimer;
        os_timer_disarm(&restarttimer);
        os_timer_setfn(&restarttimer, (os_timer_func_t *)restart, NULL);
        os_timer_arm(&restarttimer, 1000, 0);
    }
    else if (strcasecmp(path, "/sdk") == 0)
    {
        httpd_send_text(slot, 200, system_get_sdk_version());
    }
    else
    {
        return false;
    }

    return true;
}
