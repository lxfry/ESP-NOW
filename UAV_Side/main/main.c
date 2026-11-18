

#include <stdint.h>                 // Fixed-width integer types such as uint8_t and uint32_t.
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"      // FreeRTOS types and constants such as BaseType_t, pdPASS, and portMAX_DELAY.
#include "freertos/queue.h"         // Task functions such as xTaskCreate().

#include "esp_err.h"                // ESP-IDF error codes and helpers such as ESP_OK, ESP_ERROR_CHECK(), and esp_err_to_name().
#include "esp_event.h"              // Creates the default event loop used for Wi-Fi system notifications.
#include "esp_mac.h"
#include "esp_netif.h"              // Provides esp_netif_init() to initialize the network stack infrastructure
#include "esp_now.h"                // Initializes ESP-NOW, registers the receiver, and sends controller reports
#include "esp_wifi.h"               // Configures and starts the Wi-Fi driver and radio required by ESP-NOW
#include "nvs_flash.h"              // Initializes persistent flash storage used by the wireless subsystem. Allow you to use nvs_flash_init()



#define ESPNOW_CHANNEL 1                //define the WiFi channel for ESP-NOW, should be the same as the transmitter
#define MESSAGE_BUFFER_SIZE 250         // The sender uses char message[250], sending at most 249 bytes.

typedef struct {
    char text[MESSAGE_BUFFER_SIZE];     // Buffer holding the message characters and final '\0'.
} received_message_t;                   // Name of this new type.

static QueueHandle_t receive_queue;



// Called by the Wi-Fi task whenever an ESP-NOW packet arrives.
static void receive_callback(const esp_now_recv_info_t *info, const uint8_t *data, int length) {

    if (info == NULL || data == NULL || length <= 0 || length >= MESSAGE_BUFFER_SIZE) {
        return;
    }

    received_message_t message = {0};

    // ESP-NOW supplies bytes and a length, not necessarily a C string.
    memcpy(message.text, data, (size_t)length);
    message.text[length] = '\0';        // Mark the end of the copied text.

    // Do not block the Wi-Fi task. Drop the message if the queue is full.
    (void)xQueueSend(receive_queue, &message, 0);
}


// Initialisation of ESP-NOW
static void wireless_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());                  // Initialize Non-Volatile Storage, used by the default Wi-Fi configuration.

    ESP_ERROR_CHECK(esp_netif_init());                  // Initialize ESP-IDF's network interface infrastructure.
    ESP_ERROR_CHECK(esp_event_loop_create_default());   // Create the default event loop used for system events, including Wi-Fi events.

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();     // Supplies the standard settings for the ESP-IDF build

    ESP_ERROR_CHECK(esp_wifi_init(&config));                    // Initialize the driver using the config defined in previous line and create the Wifi task
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));    // Select which RAM to use
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));          // Select the WiFi station mode
    ESP_ERROR_CHECK(esp_wifi_start());                          // Start the WiFi with the defined mode

    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));   // Set the radio channel, WIFI_SECOND_CHAN_NONE means no second channel choosen

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));     // Disable the WiFi power saving -> better response but consumes more
    ESP_ERROR_CHECK(esp_now_init());                    // Initialize ESP-NOW now that WiFi is running

    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));
}








// Entry point of the firmware
void app_main(void)
{
    receive_queue = xQueueCreate(10, sizeof(received_message_t));       // Create a queue holding up to 10 message structures. 
    ESP_ERROR_CHECK(receive_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);   // Abort if allocating the queue failed.

    wireless_init();    // Start Wi-Fi and ESP-NOW, and register the receive callback.

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac));    // Read the station MAC into mac; abort on error.

    printf("UAV station MAC: " MACSTR "\n", MAC2STR(mac));
    printf("Listening for ESP-NOW messages on channel %d\n", ESPNOW_CHANNEL);

    received_message_t message;

    while (1) {
        if (xQueueReceive(receive_queue, &message, portMAX_DELAY) == pdTRUE) {  // Wait for a queued message, copy it into message, and remove it from the queue.
            // The firmware of the sender already includes a newline.
            printf("%s", message.text);
        }
    }
}
