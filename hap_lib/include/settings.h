/*
 * settings.h
 *
 *  Created on: Apr 15, 2015
 *      Author: Andrei
 */

#ifndef INCLUDE_SETTINGS_H_
#define INCLUDE_SETTINGS_H_

#include <c_types.h>

typedef struct {
#define MAGIC_NUMBER    0xD1C5A13B
    uint32_t magicNumber;

    char ssid[32];
    char password[64];

    char serverName[64];
    uint16_t udpPort;

    char mqttUser[20];
    char mqttPassword[20];
    char mqttTopic[32];
} HapSettings;

extern HapSettings settings;

void ICACHE_FLASH_ATTR settings_load();
void ICACHE_FLASH_ATTR settings_save();
void ICACHE_FLASH_ATTR settings_clear();
bool ICACHE_FLASH_ATTR settings_valid();

#endif /* INCLUDE_SETTINGS_H_ */
