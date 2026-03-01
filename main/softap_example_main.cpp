#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_random.h" 
#include "led_strip.h"
#include "kabir_corpus.h"

static const char *TAG = "KABIR_AP";

// --- RGB LED CONFIG ---
#define RGB_GPIO 38 
led_strip_handle_t led_strip;

// --- MARKOV CHAIN DATA ---
std::map<std::string, std::vector<std::string>> markov_chain;
std::vector<std::string> starting_words;

// --- WIFI BEACON DATA ---
uint8_t beacon_raw[] = {
    0x80, 0x00,                         // Frame Control: Beacon
    0x00, 0x00,                         // Duration
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // Destination: Broadcast
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, // Source MAC (will be randomized)
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, // BSSID (will be randomized)
    0x00, 0x00,                         // Seq Control
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,                         // Beacon Interval (100ms)
    0x21, 0x04,                         // Capability Info
    0x00, 0x00                          // SSID Tag (Tag Number 0, Length 0)
};

// Array for 6 networks now
char dohe_lines[6][33]; 

// --- RGB ANIMATION TASK ---
void rgb_breath_task(void *pv) {
    uint16_t hue = 0;
    uint32_t step = 0;
    
    while (1) {
        uint8_t brightness = 10 + abs((int)(step % 180) - 90);
        hue = (hue + 1) % 360;
        
        led_strip_set_pixel_hsv(led_strip, 0, hue, 255, brightness);
        led_strip_refresh(led_strip);
        
        step++;
        vTaskDelay(pdMS_TO_TICKS(150)); 
    }
}

// --- MARKOV CHAIN LOGIC ---
void build_markov_chain(const std::string& text) {
    std::string word, next_word;
    size_t start = 0, end = 0;
    std::vector<std::string> words;

    while ((end = text.find(' ', start)) != std::string::npos) {
        words.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    words.push_back(text.substr(start));

    for (size_t i = 0; i < words.size() - 1; i++) {
        markov_chain[words[i]].push_back(words[i+1]);
        if (i == 0 || (esp_random() % 10) > 7) {
            starting_words.push_back(words[i]);
        }
    }
}

void generate_dohe() {
    // 1. Hardcode the first SSID
    snprintf(dohe_lines[0], 33, "MESSAGE FROM KABIRA");
    ESP_LOGI(TAG, "Network 0: %s", dohe_lines[0]);

    // 2. Generate the remaining 5 using the Markov Chain
    for (int i = 1; i < 6; i++) {
        if (starting_words.empty()) break;
        
        std::string current_word = starting_words[esp_random() % starting_words.size()];
        std::string ssid = current_word;
        
        for (int j = 0; j < 4; j++) {
            if (markov_chain[current_word].empty()) break;
            
            std::vector<std::string>& next_words = markov_chain[current_word];
            current_word = next_words[esp_random() % next_words.size()];
            
            if (ssid.length() + current_word.length() + 1 > 32) break;
            
            ssid += " " + current_word;
        }
        
        if (!ssid.empty()) ssid[0] = toupper(ssid[0]);
        snprintf(dohe_lines[i], 33, "%s", ssid.c_str());
        ESP_LOGI(TAG, "Network %d: %s", i, dohe_lines[i]);
    }
}

// --- WIFI BROADCASTER ---
void broadcast_task(void *pv) {
    uint32_t ticks = 0;
    
    // Generate the first batch immediately
    generate_dohe(); 
    
    while (1) {
        // Change the 5 Markov SSIDs every 15 seconds (150 loops of 100ms)
        if (ticks % 150 == 0 && ticks != 0) {
            ESP_LOGI(TAG, "Rotating Kabir's words...");
            generate_dohe();
        }
        ticks++;

        // Broadcast all 6 networks constantly
        for (int i = 0; i < 6; i++) {
            beacon_raw[15] = i; // Alter Source MAC
            beacon_raw[21] = i; // Alter BSSID so phones see them as 6 separate networks
            
            uint8_t ssid_len = strlen(dohe_lines[i]);
            uint8_t packet[128];
            memcpy(packet, beacon_raw, sizeof(beacon_raw));
            packet[37] = ssid_len; 
            memcpy(&packet[38], dohe_lines[i], ssid_len); 
            
            uint8_t rates[] = {0x01, 0x08, 0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c};
            memcpy(&packet[38 + ssid_len], rates, sizeof(rates));

            // Note: Sending on WIFI_IF_STA now!
            esp_wifi_80211_tx(WIFI_IF_STA, packet, 38 + ssid_len + sizeof(rates), true);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

extern "C" void app_main(void) {
    led_strip_config_t strip_config = {}; 
    strip_config.strip_gpio_num = RGB_GPIO;
    strip_config.max_leds = 1;

    led_strip_rmt_config_t rmt_config = {}; 
    rmt_config.resolution_hz = 10 * 1000 * 1000;

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    ESP_LOGI(TAG, "Training Markov Chain...");
    build_markov_chain(std::string(kabir_corpus));

    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    
    // CRITICAL FIX: Set to Station mode to kill the default "myssid" AP!
    esp_wifi_set_mode(WIFI_MODE_STA); 
    esp_wifi_start();

    // The sniffer is now purely optional and can be used for logging if you want, 
    // but the broadcasting runs independently of it now.
    
    xTaskCreate(rgb_breath_task, "rgb_task", 2048, NULL, 4, NULL);
    xTaskCreate(broadcast_task, "dohe_tx", 4096, NULL, 5, NULL);
}