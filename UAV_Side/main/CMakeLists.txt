

idf_component_register(
    SRCS "main.c"
    PRIV_REQUIRES
        esp_hw_support
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        freertos
)