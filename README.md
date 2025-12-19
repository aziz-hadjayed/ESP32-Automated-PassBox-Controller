# 🔬 IoT Pass-Box Sterilization System

![ESP32](https://img.shields.io/badge/ESP32-000000?style=for-the-badge&logo=espressif&logoColor=white)
![Node-RED](https://img.shields.io/badge/Node--RED-8F0000?style=for-the-badge&logo=node-red&logoColor=white)
![MQTT](https://img.shields.io/badge/MQTT-660066?style=for-the-badge&logo=mqtt&logoColor=white)
![Broker](https://img.shields.io/badge/HiveMQ-FFB800?style=for-the-badge&logo=hivemq&logoColor=black)

Système IoT complet pour l'automatisation d'un sas de décontamination (Pass-Box) destiné aux environnements pharmaceutiques, laboratoires et salles blanches. Basé sur ESP32 avec communication MQTT via HiveMQ, monitoring temps réel Node-RED, et système d'alertes email.

## 📋 Table des matières

- [Caractéristiques](#-caractéristiques)
- [Architecture](#-architecture)
- [Matériel requis](#-matériel-requis)
- [Schéma de câblage](#-schéma-de-câblage)
- [Installation](#-installation)
- [Configuration](#-configuration)
- [Cycle de stérilisation](#-cycle-de-stérilisation)
- [Topics MQTT](#-topics-mqtt)
- [Dashboard Node-RED](#-dashboard-node-red)
- [Sécurité et inter-verrouillage](#-sécurité-et-inter-verrouillage)
- [Alertes et monitoring](#-alertes-et-monitoring)
- [Utilisation](#-utilisation)
- [Dépannage](#-dépannage)
- [Licence](#-licence)

## ✨ Caractéristiques

### Fonctionnalités principales

- **Cycle de stérilisation automatisé** : 7 étapes programmables (extraction air, injection produit, pause stérilisation 20s, etc.)
- **Inter-verrouillage des portes** : Sécurité garantie - impossibilité d'ouvrir les deux portes simultanément
- **Monitoring temps réel** : Affichage LCD I2C 16x2 + Dashboard web Node-RED
- **Double système d'alertes email** :
  - Email d'urgence immédiat lors d'activation d'arrêt d'urgence
  - Email de rapport automatique avec analyse CSV détaillée
- **Logging CSV local** : Enregistrement temps réel de tous les événements MQTT dans un fichier CSV local
- **Génération de rapports** : Analyse automatique du fichier CSV avec statistiques et derniers événements
- **Contrôle distant** : Interface Node-RED pour démarrage/arrêt du cycle et gestion urgence
- **Communication MQTT** : Publication/Souscription via broker HiveMQ Cloud

### Sécurité

- ✅ Système d'arrêt d'urgence avec notification email immédiate
- ✅ Email de rapport automatique avec analyse complète du fichier CSV
- ✅ Enregistrement temps réel de tous les événements dans un fichier CSV local
- ✅ Validation des portes fermées avant démarrage du cycle
- ✅ Blocage automatique en cas d'inter-verrouillage
- ✅ Autorisation porte stérile uniquement en fin de cycle
- ✅ Protection mutex pour accès concurrentiel à l'écran LCD

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                         ESP32 (Pass-Box)                     │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   6 Buttons  │  │  LCD 16x2    │  │   WiFi       │      │
│  │   GPIO       │  │  I2C PCF8574 │  │   MQTT       │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└───────────────────────────┬─────────────────────────────────┘
                            │ MQTT over WiFi
                            │
                   ┌────────▼────────┐
                   │  HiveMQ Broker  │
                   │ broker.hivemq.com│
                   └────────┬────────┘
                            │
                   ┌────────▼────────────────────────┐
                   │         Node-RED                │
                   │   ┌─────────────────────┐       │
                   │   │   Dashboard Web     │       │
                   │   │   - Contrôles       │       │
                   │   │   - Monitoring      │       │
                   │   └─────────────────────┘       │
                   │                                  │
                   │   ┌─────────────────────┐       │
                   │   │ Logging CSV Local   │       │
                   │   │ - Enregistrement    │       │
                   │   │   temps réel        │       │
                   │   │ - Tous événements   │       │
                   │   │   MQTT              │       │
                   │   └─────────────────────┘       │
                   │                                  │
                   │   ┌─────────────────────┐       │
                   │   │  Email System       │       │
                   │   │ ┌─────────────────┐ │       │
                   │   │ │ Alert Urgence   │ │       │
                   │   │ │ - Immédiate     │ │       │
                   │   │ │ - Source ESP32  │ │       │
                   │   │ └─────────────────┘ │       │
                   │   │ ┌─────────────────┐ │       │
                   │   │ │ Rapport CSV     │ │       │
                   │   │ │ - Lecture CSV   │ │       │
                   │   │ │ - Analyse auto  │ │       │
                   │   │ │ - Statistiques  │ │       │
                   │   │ │ - 15 derniers   │ │       │
                   │   │ │   événements    │ │       │
                   │   │ └─────────────────┘ │       │
                   │   └─────────────────────┘       │
                   └─────────────────────────────────┘
```

## 🔧 Matériel requis

### Composants principaux

| Composant | Quantité | Description |
|-----------|----------|-------------|
| ESP32 DevKit | 1 | Microcontrôleur principal |
| LCD 16x2 avec I2C | 1 | Afficheur (PCF8574, adresse 0x27) |
| Boutons poussoirs | 6 | Contrôles physiques |
| Résistances pull-up | 6 | 10kΩ (ou utiliser pull-up internes) |
| Breadboard | 1 | Pour prototypage |
| Câbles Dupont | 18 | Connexions |
| Alimentation 5V | 1 | Pour ESP32 |

### Spécifications

- **ESP32** : Compatible ESP-IDF, WiFi intégré
- **LCD** : Module I2C PCF8574 (adresse 0x27)
- **Alimentation** : 5V USB ou externe

## 📐 Schéma de câblage

### GPIO Mapping

#### Boutons (INPUT avec PULL-UP)

```
GPIO 27 → BTN_DEPART              (Démarrer/Arrêter cycle)
GPIO 14 → BTN_ARRET               (Urgence - Toggle)
GPIO 26 → BTN_STERILE_OUVERT      (Ouvrir porte stérile)
GPIO 25 → BTN_STERILE_FERME       (Fermer porte stérile)
GPIO 13 → BTN_CONTAMINEE_OUVERT   (Ouvrir porte contaminée)
GPIO 12 → BTN_CONTAMINEE_FERME    (Fermer porte contaminée)
```

#### I2C LCD 16x2

```
GPIO 21 → SDA (I2C Data)
GPIO 22 → SCL (I2C Clock)
VCC     → 5V
GND     → GND
```

### Schéma de connexion

```
                    ESP32
         ┌───────────────────────┐
         │                       │
GPIO 27──┤ BTN_DEPART            │
GPIO 14──┤ BTN_ARRET             │
GPIO 26──┤ BTN_STERILE_OUVERT    │
GPIO 25──┤ BTN_STERILE_FERME     │
GPIO 13──┤ BTN_CONTAMINEE_OUVERT │
GPIO 12──┤ BTN_CONTAMINEE_FERME  │
         │                       │
GPIO 21──┤ SDA ────────────┐     │
GPIO 22──┤ SCL ──────────┐ │     │
         │               │ │     │
         └───────────────┼─┼─────┘
                         │ │
                    ┌────▼─▼────┐
                    │  LCD 16x2 │
                    │ I2C PCF8574│
                    │  0x27     │
                    └───────────┘
```

## 💻 Installation

### 1. Prérequis

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32/get-started/index.html) v5.5.1 ou supérieur
- [Node-RED](https://nodered.org/docs/getting-started/local) installé
- Compte [HiveMQ Cloud](https://www.hivemq.com/demos/websocket-client/) (gratuit)

### 2. Cloner le repository

```bash
git clone https://github.com/aziz-hadjayed/ESP32-Automated-PassBox-Controller.git
cd ESP32-Automated-PassBox-Controller
```

### 3. Configuration WiFi et MQTT

Éditer le fichier `main/main.c` :

```c
// Configuration WiFi
#define WIFI_SSID   "votre_ssid"
#define WIFI_PASS   "votre_mot_de_passe"

// Configuration MQTT (HiveMQ)
#define MQTT_URI    "mqtt://broker.hivemq.com:1883"
```

### 4. Compilation et flash

```bash
# Configuration du projet
idf.py menuconfig

# Compilation
idf.py build

# Flash sur ESP32
idf.py -p /dev/ttyUSB0 flash

# Monitoring série
idf.py -p /dev/ttyUSB0 monitor
```

### 6. Installation Node-RED

```bash
# Installer Node-RED (si pas déjà fait)
sudo npm install -g --unsafe-perm node-red

# Démarrer Node-RED
node-red

# Accéder à l'interface : http://localhost:1880
```

### 7. Importer le flow Node-RED

1. Ouvrir Node-RED : `http://localhost:1880`
2. Menu (≡) → Import → Clipboard
3. Copier le contenu du fichier `node-red-flow.json`
4. Configurer les nœuds MQTT avec votre broker HiveMQ
5. Configurer les nœuds Email avec vos identifiants SMTP :
   - Email d'alerte urgence → `hadjayedaziz@gmail.com`
   - Email de rapport CSV → `khawlahouki95@gmail.com`
6. Configurer le chemin du fichier CSV local
7. Deploy

### 8. Configuration du logging CSV

Dans Node-RED, configurer le nœud "file" pour le logging :

```javascript
// Nœud: file (write)
Filename: /home/user/.node-red/mqtt_log.csv
Action: append to file
Add newline: true
Create dir if not exist: true
```

## ⚙️ Configuration

### Configuration I2C LCD

Le code utilise un module LCD I2C avec PCF8574 :

```c
#define I2C_PORT        0
#define I2C_SDA         21
#define I2C_SCL         22
#define I2C_FREQ_HZ     100000
#define PCF8574_ADDR    0x27   // Adresse I2C (vérifier avec i2c-scanner) 
```
Explication de l'adresse I2C 0x27 :
Configuration des broches A0, A1, A2 :

Si A0, A1, A2 non connectés (laissés flottants) → Pull-up interne → 111 en binaire
Adresse finale : 010 0 111 = 0x27 en hexadécimal

Si votre module utilise une autre adresse , modifier `PCF8574_ADDR`.

### Configuration Email (Node-RED)

Le système utilise **deux configurations email distinctes** :

#### Email 1 : Alerte Urgence
```
SMTP Server: smtp.gmail.com
Port: 465 (SSL) ou 587 (TLS)
Email expéditeur: votre-email@gmail.com
Email destinataire: hadjayedaziz@gmail.com
Password: mot-de-passe-application
Sujet: Alerte urgence – Système IoT
```

#### Email 2 : Rapport CSV
```
SMTP Server: smtp.gmail.com
Port: 465 (SSL) ou 587 (TLS)
Email expéditeur: votre-email@gmail.com
Email destinataire: khawlahouki95@gmail.com
Password: mot-de-passe-application
Sujet: Rapport CSV — [nombre] événements
```

⚠️ **Gmail** : Utiliser un [mot de passe d'application](https://support.google.com/accounts/answer/185833)

### Configuration CSV

Le fichier CSV doit être accessible en lecture/écriture par Node-RED :

```javascript
// Chemin du fichier (à adapter selon votre système)
Linux/Mac: /home/user/.node-red/mqtt_log.csv
Windows: C:\Users\user\.node-red\mqtt_log.csv

// Format des données
timestamp,topic,valeur
2025-12-18T23:26:35.256Z,urgence,true
```

## 🔄 Cycle de stérilisation

Le système exécute un cycle de décontamination en 7 étapes :

| Étape | Nom | Durée | Description | Topic MQTT |
|-------|-----|-------|-------------|------------|
| 0 | Idle | - | Attente | - |
| 1 | Extraction air | 3s | Évacuation de l'air ambiant | `cycle/etape`: "1: Extraction air" |
| 2 | Arrêt air | 2s | Stabilisation | `cycle/etape`: "2: Arret air" |
| 3 | Injection produit | 2s | Injection agent stérilisant | `cycle/etape`: "3: Injection produit" |
| 4 | Pause stérilisation | 20s | Temps de contact (20min réel) | `cycle/etape`: "4: Pause sterilisation 20s" |
| 5 | Extraction produit | 3s | Évacuation agent stérilisant | `cycle/etape`: "5: Extraction produit" |
| 6 | Renouvellement air | 3s | Air frais filtré | `cycle/etape`: "6: Renouvellement air" |
| 7 | Autorisation stérile | 2s | Déverrouillage porte stérile | `cycle/etape`: "7: Autorisation porte sterile" |
| 8 | Terminé | - | Fin de cycle | `cycle/etape`: "8: Termine" |

**Durée totale** : ~35 secondes (en mode test)

### Diagramme de flux

```
┌─────────────┐
│   START     │
│  (Button)   │
└──────┬──────┘
       │
       ▼
┌─────────────────┐
│ Vérif. portes   │◄─── Les deux portes doivent être fermées
│ fermées?        │
└────┬────────────┘
     │ OUI
     ▼
┌─────────────────┐
│ Étape 1         │
│ Extraction air  │ 3s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 2         │
│ Arrêt air       │ 2s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 3         │
│ Injection       │ 2s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 4         │
│ Stérilisation   │ 20s (compte à rebours)
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 5         │
│ Extraction prod.│ 3s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 6         │
│ Renouvellement  │ 3s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ Étape 7         │
│ Autorisation ✓  │ 2s
└────┬────────────┘
     │
     ▼
┌─────────────────┐
│ TERMINÉ         │
│ Ouvrir stérile  │
└─────────────────┘
```

## 📡 Topics MQTT

### Topics de publication (ESP32 → Node-RED)

| Topic | Type | Valeurs | Description |
|-------|------|---------|-------------|
| `cycle/depart` | État | `true` / `false` | Cycle en cours ou non |
| `cycle/etape` | Info | `"0: Demarrage"` à `"8: Termine"` | Étape actuelle du cycle |
| `urgence` | État | `true` / `false` | État de l'arrêt d'urgence |
| `porte/sterile` | État | `true` / `false` | Porte stérile ouverte/fermée |
| `porte/contaminee` | État | `true` / `false` | Porte contaminée ouverte/fermée |

### Topics de souscription (Node-RED → ESP32)

| Topic | Type | Valeurs acceptées | Description |
|-------|------|-------------------|-------------|
| `cmd/cycle/depart` | Commande | `ON`, `true`, `1` / `OFF`, `false`, `0` | Démarrer/Arrêter cycle |
| `cmd/urgence` | Commande | `ON`, `true`, `1` / `OFF`, `false`, `0` | Activer/Désactiver urgence |

### Exemples de messages

```json
// Démarrage du cycle
Topic: cycle/depart
Payload: "true"

// Étape en cours
Topic: cycle/etape
Payload: "4: Pause sterilisation 20s"

// Urgence activée
Topic: urgence
Payload: "true"

// Commande depuis Node-RED
Topic: cmd/cycle/depart
Payload: "ON"
```

## 📊 Dashboard Node-RED

### Composants du dashboard

Le dashboard Node-RED comprend :

1. **Indicateurs DHT11** (température/humidité)
   - Affichage des conditions ambiantes
   - Température (°C)
   - Humidité relative (%)

2. **Contrôles**
   - Bouton **Départ/Arrêt** du cycle
   - Toggle **Urgence**

3. **États des portes**
   - LED Porte stérile (Rouge/Vert)
   - LED Porte contaminée (Rouge/Vert)

4. **Monitoring cycle**
   - Étape actuelle
   - Progression
   - Temps écoulé

5. **Système de logging CSV**
   - Enregistrement automatique temps réel de tous les événements MQTT
   - Fichier CSV local sur le serveur Node-RED
   - Horodatage précis de chaque événement
   - Format : `timestamp, topic, valeur`

6. **Système d'emails automatiques**
   - **Email d'alerte urgence** : envoi immédiat lors d'activation de l'arrêt d'urgence
   - **Email de rapport** : sur demande via bouton dashboard
     - Lecture automatique du fichier CSV
     - Analyse et statistiques
     - Répartition par topic
     - 15 derniers événements détaillés

7. **Historique**
   - Visualisation des événements en temps réel
   - Debug MQTT

### Accès au dashboard

```
http://localhost:1880/ui
```

## 🔒 Sécurité et inter-verrouillage

### Règles de sécurité

#### 1. Porte stérile

**Conditions d'ouverture :**
- ❌ Urgence active → **REFUS**
- ❌ Porte contaminée ouverte → **REFUS** (inter-verrouillage)
- ❌ Cycle en cours sans autorisation → **REFUS**
- ✅ Toutes conditions OK → **AUTORISATION**

#### 2. Porte contaminée

**Conditions d'ouverture :**
- ❌ Urgence active → **REFUS**
- ❌ Porte stérile ouverte → **REFUS** (inter-verrouillage)
- ❌ Cycle en cours → **REFUS**
- ✅ Toutes conditions OK → **AUTORISATION**

#### 3. Démarrage du cycle

**Conditions de démarrage :**
- ❌ Urgence active → **REFUS**
- ❌ Une porte ouverte → **REFUS**
- ❌ Cycle déjà en cours → **REFUS**
- ✅ Les deux portes fermées + pas d'urgence → **DÉMARRAGE**

### Protection mutex LCD

Le système utilise un mutex FreeRTOS pour protéger l'accès concurrent à l'écran LCD :

```c
void lcd_show_mutex(const char *l1, const char *l2)
{
    lcd_take();           // Acquisition du mutex
    lcd_show(l1, l2);     // Affichage sécurisé
    lcd_give();           // Libération du mutex
}
```

## 🚨 Alertes et monitoring

### Système d'emails automatiques

Le système dispose de **deux types d'emails** automatiques :

#### 1. Email d'alerte urgence (immédiat)

Déclenché **automatiquement** lors de l'activation de l'arrêt d'urgence (bouton physique ou dashboard).

**Contenu de l'email :**
```
De: Système de supervision IoT
À: hadjayedaziz@gmail.com
Sujet: Alerte urgence – Système IoT

Bonjour,

Une situation d'urgence a été détectée par le système de supervision IoT.

Heure : 12/19/2025, 12:26:35 AM
Source : pass-box(ESP32)

Merci d'intervenir immédiatement afin de vérifier la situation.

Cordialement,
Système de supervision IoT
```

**Déclenchement :**
- Activation bouton BTN_ARRET (GPIO 14)
- Activation toggle urgence dans Node-RED
- Publication MQTT sur `urgence` = `true`

#### 2. Email de rapport CSV (sur demande)

Déclenché **manuellement** via le bouton dans le dashboard Node-RED.

**Fonctionnalités du rapport :**
- 📁 **Lecture automatique** du fichier CSV local
- 📊 **Analyse complète** des données
- 📈 **Statistiques** : nombre total d'événements, période couverte
- 🔢 **Répartition par topic** : comptage des occurrences
- 📌 **15 derniers événements** détaillés avec timestamp, topic et valeur
- ⏰ **Date/heure de génération** du rapport

**Exemple de rapport :**
```
De: Système de Monitoring Node-RED
À: khawlahouki95@gmail.com
Sujet: Rapport CSV — 1687 événements

Bonjour,

📊 RAPPORT AUTOMATIQUE - ANALYSE FICHIER CSV

📅 Date de génération : vendredi 19 décembre 2025 à 00:26:38 UTC+1
📊 Total d'événements : 1687
⏰ Période couverte : 2025-12-14T02:14:57.027Z → 2025-12-18T23:26:35.256Z

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📊 RÉPARTITION PAR TOPIC

📌 porte/sterile : 644 occurrences
📌 porte/contaminee : 644 occurrences
📌 cycle/etape : 197 occurrences
📌 urgence : 101 occurrences
📌 cycle/depart : 101 occurrences

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📋 DERNIERS 15 ÉVÉNEMENTS

[1673] 2025-12-18T23:12:43.359Z
📌 Topic : cycle/etape
💡 Valeur: URGENCE

[1674] 2025-12-18T23:12:43.361Z
📌 Topic : porte/sterile
💡 Valeur: ON

[1675] 2025-12-18T23:12:43.361Z
📌 Topic : porte/contaminee
💡 Valeur: OFF

... (12 événements suivants)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

🔍 RÉSUMÉ RAPIDE :

• Événements analysés : 1687
• Topics différents : 5
• Topic le plus fréquent : porte/sterile (644 occurrences)

Ce rapport a été généré automatiquement par Node-RED.

Cordialement,
Système de Monitoring Node-RED
```

### Logging CSV en temps réel

**Fichier CSV local** enregistré sur le serveur Node-RED :

**Emplacement :** `/chemin/vers/node-red/data/mqtt_log.csv`

**Format du fichier :**
```csv
timestamp,topic,valeur
2025-12-18T23:26:32.084Z,cycle/depart,true
2025-12-18T23:26:32.084Z,cycle/etape,0: Demarrage
2025-12-18T23:26:35.052Z,cycle/etape,1: Extraction air
2025-12-18T23:26:35.059Z,urgence,true
2025-12-18T23:26:35.256Z,cycle/depart,false
2025-12-18T23:26:35.256Z,cycle/etape,URGENCE
```

**Caractéristiques :**
- ✅ **Enregistrement automatique** de tous les messages MQTT reçus
- ✅ **Horodatage précis** avec timezone
- ✅ **Format standard CSV** facile à analyser
- ✅ **Tous les topics** surveillés (cycle, portes, urgence)
- ✅ **Persistance des données** même après redémarrage
- ✅ **Lecture et analyse** automatique pour les rapports

**Node-RED Flow pour CSV :**
```
[mqtt_subscribe] → [function: format_csv] → [file_append: mqtt_log.csv]
                                          ↓
                          [button_rapport] → [file_read: mqtt_log.csv]
                                          ↓
                          [function: analyze_csv] → [email_rapport]
```

### Logs série

Monitoring en temps réel via port série :

```
I (12345) Pass-Box: === CYCLE DEMARRE depuis BTN_DEPART ===
I (12346) Pass-Box: --- Etape 1: Extraction air ---
I (15346) Pass-Box: --- Etape 2: Arret air ---
W (17346) Pass-Box: INTER-VERROUILLAGE: Porte contaminée ouverte!
```

## 🎮 Utilisation

### Démarrage du système

1. **Mise sous tension**
   - Connecter l'ESP32
   - L'écran LCD affiche : `"Systeme" / "Init..."`

2. **Connexion WiFi**
   - Affichage : `"WiFi..." / "Connexion"`
   - Puis : `"WiFi OK" / "IP obtenue"`

3. **Connexion MQTT**
   - Affichage : `"MQTT OK" / "Subscribe..."`
   - Puis : `"Pret" / "Attente..."`

### Cycle manuel (boutons physiques)

1. **Fermer les deux portes**
   - Appuyer sur BTN_STERILE_FERME (GPIO 25)
   - Appuyer sur BTN_CONTAMINEE_FERME (GPIO 12)

2. **Démarrer le cycle**
   - Appuyer sur BTN_DEPART (GPIO 27)
   - LCD affiche : `"Cycle DEMARRE" / "BTN_DEPART"`

3. **Observer les étapes**
   - L'écran LCD affiche chaque étape
   - Étape 4 : compte à rebours de 20s

4. **Fin du cycle**
   - LCD affiche : `"CYCLE TERMINE" / "Ouvrir sterile"`
   - Appuyer sur BTN_STERILE_OUVERT (GPIO 26)

### Cycle distant (Node-RED)

1. **Ouvrir le dashboard**
   ```
   http://localhost:1880/ui
   ```

2. **Vérifier les portes**
   - Les deux LEDs doivent être vertes (fermées)

3. **Démarrer**
   - Cliquer sur le bouton "DÉPART"
   - Observer la progression sur le dashboard
   - Tous les événements sont enregistrés dans le CSV

4. **Monitoring**
   - L'étape actuelle s'affiche en temps réel
   - Historique disponible dans le debug
   - Enregistrement automatique dans le fichier CSV

5. **Générer un rapport**
   - Cliquer sur le bouton "Rapport CSV"
   - Le système lit le fichier CSV local
   - Analyse automatique des données
   - Email envoyé à `khawlahouki95@gmail.com` avec :
     - Statistiques complètes
     - Répartition par topic
     - 15 derniers événements détaillés

### Arrêt d'urgence

**Méthode 1 : Bouton physique**
```
Appuyer sur BTN_ARRET (GPIO 14)
→ LCD: "ARRET URGENCE" / "BTN_ARRET"
→ Email d'alerte envoyé à hadjayedaziz@gmail.com
→ Événement enregistré dans le CSV
```

**Méthode 2 : Dashboard Node-RED**
```
Activer le toggle "URGENCE"
→ MQTT: urgence = true
→ Email d'alerte envoyé à hadjayedaziz@gmail.com
→ Événement enregistré dans le CSV
```

**Désactivation :**
- Réappuyer sur le même bouton/toggle
- LCD: `"Urgence OFF" / "Etat normal"`
- Événement enregistré dans le CSV

### Consultation de l'historique

**Via fichier CSV local :**
```bash
# Lire le fichier CSV
cat /home/user/.node-red/mqtt_log.csv

# Dernières lignes
tail -n 50 /home/user/.node-red/mqtt_log.csv

# Filtrer par topic
grep "urgence" /home/user/.node-red/mqtt_log.csv
```

**Via email de rapport :**
1. Ouvrir le dashboard Node-RED
2. Cliquer sur le bouton "Rapport CSV"
3. Attendre quelques secondes
4. Consulter votre email (khawlahouki95@gmail.com)
5. Le rapport contient :
   - Total d'événements
   - Période couverte
   - Statistiques par topic
   - 15 derniers événements

## 🔧 Dépannage

### Problème : LCD ne s'allume pas

**Causes possibles :**
- ✅ Vérifier l'adresse I2C (0x27 ou 0x3F)
- ✅ Vérifier les connexions SDA/SCL
- ✅ Vérifier l'alimentation 5V du LCD
- ✅ Tester avec un I2C scanner

**Solution :**
```bash
# Scanner I2C
idf.py menuconfig
# Component config → ESP32-specific → I2C Scanner
```

### Problème : WiFi ne se connecte pas

**Causes possibles :**
- ❌ SSID/Password incorrects
- ❌ Signal WiFi faible
- ❌ Canal WiFi incompatible

**Solution :**
```c
// Activer les logs WiFi
esp_log_level_set("wifi", ESP_LOG_VERBOSE);
```

### Problème : MQTT ne publie pas

**Vérifications :**
```bash
# Test avec mosquitto_sub
mosquitto_sub -h broker.hivemq.com -t "cycle/#" -v

# Test avec mosquitto_pub
mosquitto_pub -h broker.hivemq.com -t "cmd/cycle/depart" -m "ON"
```

### Problème : Email non reçu

**Email d'alerte urgence :**
- ✅ Vérifier configuration SMTP dans Node-RED
- ✅ Port 465 (SSL) ou 587 (TLS)
- ✅ Mot de passe d'application (Gmail)
- ✅ Destinataire : `hadjayedaziz@gmail.com`
- ✅ Vérifier les logs Node-RED
- ✅ Tester manuellement le nœud email

**Email de rapport CSV :**
- ✅ Vérifier que le fichier CSV existe et est accessible
- ✅ Chemin du fichier correct dans Node-RED
- ✅ Destinataire : `khawlahouki95@gmail.com`
- ✅ Vérifier les permissions de lecture du fichier
- ✅ Tester la lecture du CSV manuellement

### Problème : CSV ne s'enregistre pas

**Vérifications :**
```bash
# Vérifier que le dossier existe
ls -la /home/user/.node-red/

# Vérifier les permissions
chmod 644 /home/user/.node-red/mqtt_log.csv

# Vérifier le contenu
tail -f /home/user/.node-red/mqtt_log.csv
```

**Configuration Node-RED :**
- ✅ Nœud "file" configuré en mode "append"
- ✅ Option "Create directory if not exist" activée
- ✅ Format correct : timestamp,topic,valeur
- ✅ Vérifier que les messages MQTT arrivent bien

### Problème : Rapport CSV vide ou incomplet

**Solutions :**
- ✅ Attendre quelques minutes pour que des événements soient enregistrés
- ✅ Vérifier que le fichier CSV contient des données
- ✅ S'assurer que le flow Node-RED de lecture CSV fonctionne
- ✅ Tester le nœud "read file" manuellement
- ✅ Vérifier la fonction d'analyse du CSV

### Problème : Boutons non réactifs

**Causes possibles :**
- ❌ Pull-up non activé
- ❌ Debounce insuffisant
- ❌ Mauvais branchement

**Solution :**
```c
// Vérifier la config GPIO
gpio_config_t io_conf = {
    .pull_up_en = GPIO_PULLUP_ENABLE,  // ← Important!
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
};
```

## 📁 Structure du projet

```
IoT-PassBox-Sterilization-System/
│
├── main/
│   └── main.c                 # Code principal ESP32
│
├── node-red-flow.json         # Flow Node-RED complet
│                              # - Dashboard
│                              # - MQTT subscribers
│                              # - Logging CSV
│                              # - Email alertes
│                              # - Email rapports
│
├── data/
│   └── mqtt_log.csv          # Fichier CSV (généré automatiquement)
│                              # Format: timestamp,topic,valeur
│
├── CMakeLists.txt             # Configuration CMake
├── sdkconfig                  # Configuration ESP-IDF
│
├── docs/
│   ├── images/                # Screenshots et schémas
│   │   ├── dashboard.png
│   │   ├── wiring.png
│   │   ├── lcd.png
│   │   ├── email_alert.png
│   │   └── email_report.png
│   └── datasheets/            # Datasheets composants
│
└── README.md                  # Ce fichier
```

## 🤝 Contribution

Les contributions sont les bienvenues ! Pour contribuer :

1. Fork le projet
2. Créer une branche (`git checkout -b feature/amelioration`)
3. Commit les changements (`git commit -m 'Ajout fonctionnalité'`)
4. Push vers la branche (`git push origin feature/amelioration`)
5. Ouvrir une Pull Request

## 📝 TODO / Améliorations futures

- [ ] Ajout capteur DHT11 pour température/humidité réelles
- [ ] Support TLS/SSL pour MQTT (sécurité renforcée)
- [ ] Interface web locale sur ESP32 (WebServer)
- [ ] Historique des cycles dans SPIFFS/SD Card
- [ ] Notification push (Telegram/WhatsApp)
- [ ] Mode maintenance avec diagnostics
- [ ] Calibration automatique des durées d'étape
- [ ] Support multi-langues (FR/EN/AR)

## 📄 Licence

Ce projet est sous licence MIT. Voir le fichier [LICENSE](LICENSE) pour plus de détails.

```
MIT License

Copyright (c) 2025 [Votre Nom]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction...
```

## 👨‍💻 Auteur

**Hadj Ayed Aziz**
- Email: hadjayedaziz@gmail.com
- GitHub: [@votre-username](https://github.com/votre-username)

## 🙏 Remerciements

- ESP-IDF Framework par Espressif
- Node-RED Community
- HiveMQ pour le broker MQTT gratuit
- FreeRTOS pour le système temps réel

---

⭐ **Si ce projet vous a été utile, n'hésitez pas à lui donner une étoile !**

📧 **Questions ?** Ouvrir une [issue](https://github.com/votre-username/IoT-PassBox-Sterilization-System/issues)
