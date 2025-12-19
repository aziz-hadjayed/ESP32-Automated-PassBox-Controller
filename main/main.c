
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "mqtt_client.h"
#include "driver/gpio.h"

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "string.h"

// ======================= CONFIG =======================
#define WIFI_SSID   "globalnet"
#define WIFI_PASS   "changeme"

// HiveMQ Cloud (sans TLS pour simplifier)
#define MQTT_URI    "mqtt://broker.hivemq.com:1883"


// ======================= GPIO =======================
#define BTN_DEPART              GPIO_NUM_27
#define BTN_ARRET               GPIO_NUM_14
#define BTN_STERILE_OUVERT      GPIO_NUM_26
#define BTN_STERILE_FERME       GPIO_NUM_25
#define BTN_CONTAMINEE_OUVERT   GPIO_NUM_13
#define BTN_CONTAMINEE_FERME    GPIO_NUM_12

// ======================= TOPICS Publisher =======================
#define TOPIC_CYCLE_DEPART      "cycle/depart"
#define TOPIC_CYCLE_ETAPE       "cycle/etape"
#define TOPIC_URGENCE           "urgence"
#define TOPIC_PORTE_STERILE     "porte/sterile"
#define TOPIC_PORTE_CONTAM      "porte/contaminee"

// ======================= TOPICS subscriber =======================
#define TOPIC_CMD_URGENCE       "cmd/urgence"
#define TOPIC_CMD_CYCLE_DEPART  "cmd/cycle/depart"

// ========= CONFIG LCD=========
#define I2C_PORT        0
#define I2C_SDA         21
#define I2C_SCL         22
#define I2C_FREQ_HZ     100000
#define PCF8574_ADDR    0x27   // adresse 0x4E décalée de 1bit de valeur 0 pour R/W

// PCF8574 → LCD mapping (le plus courant)
#define LCD_BACKLIGHT   0x08
#define LCD_ENABLE      0x04
#define LCD_RW          0x02
#define LCD_RS          0x01

// ======================= ETAPES DU CYCLE =======================
typedef enum {
    ETAPE_IDLE = 0,
    ETAPE_EXTRACTION_AIR,           // 1: 3s
    ETAPE_ARRET_AIR,                // 2: 2s
    ETAPE_INJECTION_PRODUIT,        // 3: 2s
    ETAPE_PAUSE_STERILISATION,      // 4: 20s (20min en réel)
    ETAPE_EXTRACTION_PRODUIT,       // 5: 3s
    ETAPE_RENOUVELLEMENT_AIR,       // 6: 3s
    ETAPE_AUTORISATION_STERILE,     // 7: Déverrouillage porte stérile
    ETAPE_TERMINE                   // 8: Fin
} etape_cycle_t;

// ======================= LOG =======================
static const char *TAG = "Pass-Box";

// ======================= WIFI EVENT =======================
static EventGroupHandle_t wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// ======================= MQTT =======================
static esp_mqtt_client_handle_t mqtt_client = NULL;

// ======================= I2C =======================
static i2c_master_bus_handle_t i2c_bus;
static i2c_master_dev_handle_t lcd_dev;

// ======================= ETATS SYSTEME =======================
static volatile bool porte_sterile_ouverte = false;
static volatile bool porte_contaminee_ouverte = false;
static volatile bool cycle_en_cours = false;
static volatile bool urgence_active = false;
static volatile bool autorisation_porte_sterile = false;
static volatile etape_cycle_t etape_actuelle = ETAPE_IDLE;

// ======================= LCD (PLACEHOLDER) =======================
static void lcd_show(const char *l1, const char *l2);
static void lcd_init(void);


/* 1) ============================================ MUTEX */
static SemaphoreHandle_t lcd_mutex = NULL;   // global

/* 2) ============================================ WRAPPER BAS-NIVEAU */
static void lcd_take(void)
{
    xSemaphoreTake(lcd_mutex, portMAX_DELAY);
}
static void lcd_give(void)
{
    xSemaphoreGive(lcd_mutex);
}

/* 3) ============================================ WRAPPER HAUT-NIVEAU */
/* Fonction « thread-safe » unique à appeler depuis les tâches */
void lcd_show_mutex(const char *l1, const char *l2)
{
    lcd_take();
    lcd_show(l1, l2);          // votre ancienne fonction
    lcd_give();
}

/* 4) ============================================ INIT DU MUTEX */
static void lcd_mutex_init(void)
{
    lcd_mutex = xSemaphoreCreateMutex();
    assert(lcd_mutex != NULL);
}

/* 5) ============================================ INIT LCD (appelée depuis app_main) */
static void lcd_init_full(void)
{
    lcd_mutex_init();          // cree le mutex AVANT tout accès
    lcd_take();
    lcd_init();                
    lcd_give();
}











// ========= LOW LEVEL =========
static void lcd_write_byte(uint8_t data)
{
    i2c_master_transmit(lcd_dev, &data, 1, -1);
}

static void lcd_pulse(uint8_t data)
{
    lcd_write_byte(data | LCD_ENABLE | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_byte((data & ~LCD_ENABLE) | LCD_BACKLIGHT);
    vTaskDelay(pdMS_TO_TICKS(1));
}
static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) | LCD_BACKLIGHT;
    if (rs) data |= LCD_RS;

    lcd_write_byte(data);
    lcd_pulse(data);
}
static void lcd_cmd(uint8_t cmd)
{
    lcd_send_nibble(cmd >> 4, 0);
    lcd_send_nibble(cmd & 0x0F, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
}



static void lcd_write_nibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) | (rs ? LCD_RS : 0);
    lcd_pulse(data);
}

static void lcd_write_cmd(uint8_t cmd)
{
    lcd_write_nibble(cmd >> 4, 0);
    lcd_write_nibble(cmd & 0x0F, 0);
}

static void lcd_write_char(char c)
{
    lcd_write_nibble(c >> 4, 1);
    lcd_write_nibble(c & 0x0F, 1);
}

// ========= HIGH LEVEL =========
static void lcd_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(50));

    lcd_write_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_nibble(0x03, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_nibble(0x03, 0);
    lcd_write_nibble(0x02, 0); // 4-bit mode

    lcd_write_cmd(0x28); // 4-bit, 2 lines
    lcd_write_cmd(0x0C); // display ON
    lcd_write_cmd(0x06); // cursor move
    lcd_write_cmd(0x01); // clear
    vTaskDelay(pdMS_TO_TICKS(2));
}



static void lcd_print(const char *str)
{
    while (*str) {
        lcd_write_char(*str++);
    }
}

static void lcd_show(const char *l1, const char *l2)
{
    lcd_cmd(0x01);          // Clear LCD
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_print(l1);          // Ligne 1

    lcd_cmd(0xC0);          // Ligne 2 (LCD 16x2)
    lcd_print(l2);          // Ligne 2
}


// ======================= GPIO INIT =======================
static void gpio_init_buttons(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask =
            (1ULL << BTN_DEPART) |
            (1ULL << BTN_ARRET) |
            (1ULL << BTN_STERILE_OUVERT) |
            (1ULL << BTN_STERILE_FERME) |
            (1ULL << BTN_CONTAMINEE_OUVERT) |
            (1ULL << BTN_CONTAMINEE_FERME)
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

// ======================= HELPERS MQTT PUBLISH =======================
static void mqtt_pub(const char *topic, const char *payload)
{
    if (!mqtt_client) return;
    esp_mqtt_client_publish(mqtt_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "PUB [%s] %s", topic, payload);
}

// ======================= ACTIONS =======================
static void activer_urgence(const char *source)
{
    if (urgence_active) return;
    urgence_active = true;
    cycle_en_cours = false;
    etape_actuelle = ETAPE_IDLE;
    autorisation_porte_sterile = false;

    lcd_show_mutex("ARRET URGENCE", source);
    mqtt_pub(TOPIC_URGENCE, "true");
    mqtt_pub(TOPIC_CYCLE_DEPART, "false");
    mqtt_pub(TOPIC_CYCLE_ETAPE, "URGENCE");
    
    ESP_LOGW(TAG, "URGENCE activée depuis: %s", source);
}

static void desactiver_urgence(void)
{
    if (!urgence_active) return;
    urgence_active = false;

    lcd_show_mutex("Urgence OFF", "Etat normal");
    mqtt_pub(TOPIC_URGENCE, "false");
    ESP_LOGI(TAG, "Urgence désactivée");
}

// ======================= INTER-VERROUILLAGE =======================
static bool verifier_interverrouillage_ouverture_sterile(void)
{
    if (urgence_active) {
        lcd_show_mutex("REFUS STERILE", "Urgence active");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: urgence active");
        return false;
    }
    
    if (porte_contaminee_ouverte) {
        lcd_show_mutex("REFUS STERILE", "Porte contam. ON");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: inter-verrouillage");
        ESP_LOGW(TAG, "INTER-VERROUILLAGE: Porte contaminée ouverte!");
        return false;
    }
    
    if (cycle_en_cours && !autorisation_porte_sterile) {
        lcd_show_mutex("REFUS STERILE", "Cycle en cours");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: cycle non termine");
        return false;
    }
    
    return true;
}

static bool verifier_interverrouillage_ouverture_contaminee(void)
{
    if (urgence_active) {
        lcd_show_mutex("REFUS CONTAM.", "Urgence active");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: urgence active");
        return false;
    }
    
    if (porte_sterile_ouverte) {
        lcd_show_mutex("REFUS CONTAM.", "Porte sterile ON");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: inter-verrouillage");
        ESP_LOGW(TAG, "INTER-VERROUILLAGE: Porte stérile ouverte!");
        return false;
    }
    
    if (cycle_en_cours) {
        lcd_show_mutex("REFUS CONTAM.", "Cycle en cours");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: cycle en cours");
        return false;
    }
    
    return true;
}

// ======================= VALIDATION DEMARRAGE =======================
static bool portes_ok_pour_demarrer(void)
{
    return (!porte_sterile_ouverte && !porte_contaminee_ouverte);
}

static void demarrer_cycle(const char *source)
{
    if (urgence_active) {
        lcd_show_mutex("Refus: urgence", source);
        return;
    }
    
    if (cycle_en_cours) {
        ESP_LOGW(TAG, "Cycle déjà en cours");
        return;
    }

    if (!portes_ok_pour_demarrer()) {
        lcd_show_mutex("ERREUR PORTES", "Fermer les 2");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Erreur: portes ouvertes");
        ESP_LOGE(TAG, "Impossible démarrer: portes ouvertes");
        return;
    }

    // Démarrage effectif
    cycle_en_cours = true;
    etape_actuelle = ETAPE_EXTRACTION_AIR;
    autorisation_porte_sterile = false;
    
    lcd_show_mutex("Cycle DEMARRE", source);
    mqtt_pub(TOPIC_CYCLE_DEPART, "true");
    mqtt_pub(TOPIC_CYCLE_ETAPE, "0: Demarrage");
    
    ESP_LOGI(TAG, "=== CYCLE DEMARRE depuis %s ===", source);
}

static void arreter_cycle(const char *source)
{
    if (!cycle_en_cours) return;

    cycle_en_cours = false;
    etape_actuelle = ETAPE_IDLE;
    autorisation_porte_sterile = false;

    lcd_show_mutex("Cycle STOP", source);
    mqtt_pub(TOPIC_CYCLE_DEPART, "false");
    mqtt_pub(TOPIC_CYCLE_ETAPE, "Arrete");
    
    ESP_LOGW(TAG, "Cycle arrêté depuis: %s", source);
}

// ======================= CYCLE DE DECONTAMINATION =======================
static void cycle_task(void *arg)
{
    while (1) {
        if (!cycle_en_cours || urgence_active) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        switch (etape_actuelle) {
            
            case ETAPE_EXTRACTION_AIR:
                ESP_LOGI(TAG, "--- Etape 1: Extraction air ---");
                lcd_show_mutex("Etape 1/7", "Extraction air");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "1: Extraction air");
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_ARRET_AIR;
                break;

            case ETAPE_ARRET_AIR:
                ESP_LOGI(TAG, "--- Etape 2: Arret air ---");
                lcd_show_mutex("Etape 2/7", "Arret air");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "2: Arret air");
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_INJECTION_PRODUIT;
                break;

            case ETAPE_INJECTION_PRODUIT:
                ESP_LOGI(TAG, "--- Etape 3: Injection produit ---");
                lcd_show_mutex("Etape 3/7", "Injection produit");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "3: Injection produit");
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_PAUSE_STERILISATION;
                break;

            case ETAPE_PAUSE_STERILISATION:
                ESP_LOGI(TAG, "--- Etape 4: Pause sterilisation (20s) ---");
                lcd_show_mutex("Etape 4/7", "Sterilisation");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "4: Pause sterilisation 20s");
                
                for (int i = 20; i > 0 && cycle_en_cours && !urgence_active; i--) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "Steril: %ds", i);
                    lcd_show_mutex("Etape 4/7", buf);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                
                if (cycle_en_cours) etape_actuelle = ETAPE_EXTRACTION_PRODUIT;
                break;

            case ETAPE_EXTRACTION_PRODUIT:
                ESP_LOGI(TAG, "--- Etape 5: Extraction produit ---");
                lcd_show_mutex("Etape 5/7", "Extract. produit");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "5: Extraction produit");
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_RENOUVELLEMENT_AIR;
                break;

            case ETAPE_RENOUVELLEMENT_AIR:
                ESP_LOGI(TAG, "--- Etape 6: Renouvellement air ---");
                lcd_show_mutex("Etape 6/7", "Renouvel. air");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "6: Renouvellement air");
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_AUTORISATION_STERILE;
                break;

            case ETAPE_AUTORISATION_STERILE:
                ESP_LOGI(TAG, "--- Etape 7: Autorisation porte sterile ---");
                lcd_show_mutex("Etape 7/7", "Autorisation OK");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "7: Autorisation porte sterile");
                
                autorisation_porte_sterile = true;
                
                vTaskDelay(pdMS_TO_TICKS(2000));
                
                if (cycle_en_cours) etape_actuelle = ETAPE_TERMINE;
                break;

            case ETAPE_TERMINE:
                ESP_LOGI(TAG, "=== CYCLE TERMINE ===");
                lcd_show_mutex("CYCLE TERMINE", "Ouvrir sterile");
                mqtt_pub(TOPIC_CYCLE_ETAPE, "8: Termine");
                mqtt_pub(TOPIC_CYCLE_DEPART, "false");
                
                cycle_en_cours = false;
                etape_actuelle = ETAPE_IDLE;
                
                vTaskDelay(pdMS_TO_TICKS(2000));
                lcd_show_mutex("Pret", "Attente...");
                break;

            default:
                etape_actuelle = ETAPE_IDLE;
                break;
        }
    }
}

// ======================= MQTT EVENT HANDLER =======================
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connecté à HiveMQ");
        lcd_show_mutex("MQTT OK", "Subscribe...");

        esp_mqtt_client_subscribe(mqtt_client, TOPIC_CMD_CYCLE_DEPART, 0);
        esp_mqtt_client_subscribe(mqtt_client, TOPIC_CMD_URGENCE, 0);
        
        // Publier l'état initial
        mqtt_pub(TOPIC_PORTE_STERILE, "false");
        mqtt_pub(TOPIC_PORTE_CONTAM, "false");
        mqtt_pub(TOPIC_URGENCE, "false");
        mqtt_pub(TOPIC_CYCLE_DEPART, "false");
        mqtt_pub(TOPIC_CYCLE_ETAPE, "Systeme pret");
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT déconnecté");
        break;

    case MQTT_EVENT_DATA: {
        char topic[event->topic_len + 1];
        char data[event->data_len + 1];

        memcpy(topic, event->topic, event->topic_len);
        topic[event->topic_len] = 0;

        memcpy(data, event->data, event->data_len);
        data[event->data_len] = 0;

        ESP_LOGI(TAG, "RX [%s] %s", topic, data);

        // Commande cycle depuis Node-RED
        if (strcmp(topic, TOPIC_CMD_CYCLE_DEPART) == 0) {
            // Accepter "ON", "true", "1"
            if (strcmp(data, "ON") == 0 || strcmp(data, "true") == 0 || strcmp(data, "1") == 0) {
                demarrer_cycle("MQTT");
            } 
            // Accepter "OFF", "false", "0"
            else if (strcmp(data, "OFF") == 0 || strcmp(data, "false") == 0 || strcmp(data, "0") == 0) {
                arreter_cycle("MQTT");
            }
        }

        // Commande urgence depuis Node-RED
        if (strcmp(topic, TOPIC_CMD_URGENCE) == 0) {
            // Accepter "ON", "true", "1"
            if (strcmp(data, "ON") == 0 || strcmp(data, "true") == 0 || strcmp(data, "1") == 0) {
                activer_urgence("MQTT");
            } 
            // Accepter "OFF", "false", "0"
            else if (strcmp(data, "OFF") == 0 || strcmp(data, "false") == 0 || strcmp(data, "0") == 0) {
                desactiver_urgence();
            }
        }
        break;
    }

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error");
        break;

    default:
        break;
    }
}

// ======================= MQTT INIT =======================
static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_URI,
        .session.keepalive = 120,
        .network.reconnect_timeout_ms = 10000,
        .network.timeout_ms = 30000,
        .session.disable_clean_session = false,
    };
    
  
    mqtt_client = esp_mqtt_client_init(&cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "MQTT client init failed!");
        return;
    }
    
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    
    ESP_LOGI(TAG, "MQTT client started, connecting to %s", MQTT_URI);
}

// ======================= WIFI HANDLER =======================
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi déconnecté, reconnexion...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP obtenue: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// ======================= WIFI INIT =======================
static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &wifi_event_handler, NULL));

    wifi_config_t sta_cfg = {0};
    strncpy((char *)sta_cfg.sta.ssid, WIFI_SSID, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, WIFI_PASS, sizeof(sta_cfg.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    lcd_show_mutex("WiFi...", "Connexion");
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, 
                       pdFALSE, pdTRUE, portMAX_DELAY);
    lcd_show_mutex("WiFi OK", "IP obtenue");
}

// ======================= BUTTON TASK (MODE TOGGLE) =======================
static void button_task(void *arg)
{
    while (1) {
        // ========== URGENCE (TOGGLE) ==========
        if (gpio_get_level(BTN_ARRET) == 0) {
            if (!urgence_active) {
                activer_urgence("BTN_ARRET");
            } else {
                desactiver_urgence();
            }
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // ========== DEPART/ARRET CYCLE (TOGGLE) ==========
        if (gpio_get_level(BTN_DEPART) == 0) {
            if (!cycle_en_cours) {
                demarrer_cycle("BTN_DEPART");
            } else {
                arreter_cycle("BTN_DEPART");
            }
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // ========== PORTE STERILE OUVRIR ==========
        if (gpio_get_level(BTN_STERILE_OUVERT) == 0) {
            if (verifier_interverrouillage_ouverture_sterile()) {
                porte_sterile_ouverte = true;
                mqtt_pub(TOPIC_PORTE_STERILE, "true");
                lcd_show_mutex("Porte sterile", "OUVERTE");
                ESP_LOGI(TAG, "Porte stérile ouverte");
                
                if (autorisation_porte_sterile) {
                    autorisation_porte_sterile = false;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // ========== PORTE STERILE FERMER ==========
        if (gpio_get_level(BTN_STERILE_FERME) == 0) {
            porte_sterile_ouverte = false;
            mqtt_pub(TOPIC_PORTE_STERILE, "false");
            lcd_show_mutex("Porte sterile", "FERMEE");
            ESP_LOGI(TAG, "Porte stérile fermée");
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // ========== PORTE CONTAMINEE OUVRIR ==========
        if (gpio_get_level(BTN_CONTAMINEE_OUVERT) == 0) {
            if (verifier_interverrouillage_ouverture_contaminee()) {
                porte_contaminee_ouverte = true;
                mqtt_pub(TOPIC_PORTE_CONTAM, "true");
                lcd_show_mutex("Porte contam.", "OUVERTE");
                ESP_LOGI(TAG, "Porte contaminée ouverte");
            }
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // ========== PORTE CONTAMINEE FERMER ==========
        if (gpio_get_level(BTN_CONTAMINEE_FERME) == 0) {
            porte_contaminee_ouverte = false;
            mqtt_pub(TOPIC_PORTE_CONTAM, "false");
            lcd_show_mutex("Porte contam.", "FERMEE");
            ESP_LOGI(TAG, "Porte contaminée fermée");
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}












// ======================= APP_MAIN =======================
void app_main(void)
{
    ESP_LOGI(TAG, "=== DEMARRAGE SYSTEME PASS-BOX ===");
    
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_init_buttons();
        ESP_LOGI(TAG, "Init LCD");

    lcd_init_full();
    lcd_show_mutex("Systeme", "Init...");

    wifi_init();
    mqtt_init();
     ESP_LOGI(TAG, "Init I2C");

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c_bus));

    i2c_device_config_t dev_cfg = {
    .device_address = PCF8574_ADDR,
    .scl_speed_hz = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &dev_cfg, &lcd_dev));








    lcd_show_mutex("Pret", "Attente...");



    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);
    xTaskCreate(cycle_task, "cycle_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "=== SYSTEME OPERATIONNEL ===");
}