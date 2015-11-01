#include "osapi.h"
#include "user_interface.h"

#include "uart.h"
#include "user_config.h"
#include "gpio.h"
#include "mqtt/mqtt.h"
#include "settings.h"
#include "hap.h"

static MQTT_Client *client = NULL;

static void ICACHE_FLASH_ATTR on_timeout(void *arg)
{
	static uint8_t lastPin0 = 0xFF, lastPin1 = 0xFF;
	uint8_t pin0 = GPIO_INPUT_GET(PIN0);
	uint8_t pin1 = GPIO_INPUT_GET(PIN1);

	if ((pin0 != lastPin0 || pin1 != lastPin1) && client != NULL) {
		uint8_t data[2];
		data[0] = pin0 ? '1' : '0';
		data[1] = pin1 ? '1' : '0';
		MQTT_Publish(client, "/hap/in", data, 2, 0, 0);
		lastPin0 = pin0;
		lastPin1 = pin1;
	}
}

static void ICACHE_FLASH_ATTR onMqttConnected(MQTT_Client *c)
{
    client = c;
}

static void ICACHE_FLASH_ATTR onMqttDisconnected(MQTT_Client *c)
{
    client = NULL;
}

void ICACHE_FLASH_ATTR user_init()
{
	uart_init(BIT_RATE_115200);

	PIN_FUNC_SELECT(PIN0_MUX, PIN0_FUNC);
	GPIO_DIS_OUTPUT(PIN0);
	PIN_FUNC_SELECT(PIN1_MUX, PIN1_FUNC);
	GPIO_DIS_OUTPUT(PIN1);

	static ETSTimer timer;
	os_timer_disarm(&timer);
	os_timer_setfn(&timer, (os_timer_func_t *)on_timeout, &timer);
	os_timer_arm(&timer, 250, 1);

    hap_setConnectedCb(onMqttConnected);
    hap_setDisconnectedCb(onMqttDisconnected);
    hap_init();
}
