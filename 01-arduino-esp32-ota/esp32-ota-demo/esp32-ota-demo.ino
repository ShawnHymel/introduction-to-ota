/**
 * OTA demo for the ESP32
 */

#include <WiFi.h>
#include <ArduinoOTA.h>
#include "esp_ota_ops.h"

// WiFi and OTA settings
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* OTA_HOSTNAME = "ota-demo";
const char* OTA_PASSWORD = "OtaPassword";

// Version settings
#define FW_VERSION              "1.0.0"

// App configuration
#define NUM_BLINKS              1
#define HEALTH_CHECK_TIMEOUT_MS 15000

// Pins
#define LED_PIN                 LED_BUILTIN 

/******************************************************************************
 * Functions
 */

// By default, the Arduino ESP32 core confirms a new image before calling
// setup. Override this function (return true) to say we'll confirm manually.
extern "C" bool verifyRollbackLater() { 
  return true;
}

// Called once on first boot of a new OTA update. Can check sensors, etc. Most
// important: make sure you can receive a new OTA update in the future.
static bool OtaHealthCheck() {
  // Wait for WiFi to connect and get an IP address
  Serial.println("Health check: waiting for WiFi...");
  uint32_t start_ms = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start_ms > HEALTH_CHECK_TIMEOUT_MS) {
      Serial.println("ERROR: No WiFi connection (timed out)");
      return false;
    }
    delay(100);
  }

  // Print IP address
  Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());

  return true;
}

// Confirm image or roll back to known good if health check fails
void OtaConfirmImage() {
  // Get the state of the current app image
  const esp_partition_t *partition = esp_ota_get_running_partition();
  esp_ota_img_states_t img_state;
  esp_err_t err = esp_ota_get_state_partition(partition, &img_state);

  // Show which slot we're running from
  if (partition != NULL) {
    Serial.printf("App running from slot %s\n", partition->label);
  }

  // Use the image state to determine how to handle the verification process
  if (err == ESP_OK) {
    switch (img_state) {

      // Image in probation: perform health checks
      case ESP_OTA_IMG_PENDING_VERIFY:
        if (OtaHealthCheck()) {
          // If health check passes, mark image as valid
          Serial.println("Health check OK: marking app as valid");
          esp_ota_mark_app_valid_cancel_rollback();
        } else {
          // If health check fails, roll back to known good version
          Serial.println("Health check FAILED: rolling back to previous version");
          Serial.flush();
          blinkLed(5, 50);
          esp_ota_mark_app_invalid_rollback_and_reboot();
        }
        break;
      
      // Image is valid, boot normally
      case ESP_OTA_IMG_VALID:
        Serial.println("Image valid, continuing to application");
        break;

      // No OTA tracking for this image (e.g. flashed over USB)
      case ESP_OTA_IMG_UNDEFINED:
        Serial.println("Image not tracked by OTA (e.g. USB upload)");
        break;

      // Unknown image state
      default:
        Serial.println("WARNING: image unverified, continuing anyway");
    }

  // Could not get image state, so throw a warning and boot normally
  } else {
    Serial.printf("WARNING (%s): OTA state unavailable, skip health check\n",
                  esp_err_to_name(err));
  }
}

// On OTA start: print message
void OtaOnStart() {
  Serial.println("OTA: receiving new image...");
}

// On OTA progress: flash LED
void OtaOnProgress(unsigned int done, unsigned int total) {
  static bool led_state = false;
  digitalWrite(LED_PIN, led_state ? LOW : HIGH);
  led_state = !led_state;
}

// On OTA end: print message and turn LED off
void OtaOnEnd() {
  Serial.println("OTA: image received, rebooting...");
  digitalWrite(LED_PIN, HIGH);
}

// On OTA error: print message
void OtaOnError(ota_error_t e) {
  Serial.printf("ERROR (%u): could not receive OTA image\n", e);
}

// Blink LED a number of given times (blocking)
void blinkLed(uint32_t num_blinks, uint32_t delay_ms) {
  for (uint32_t i = 0; i < num_blinks; i++) {
    digitalWrite(LED_PIN, LOW);
    delay(delay_ms);
    digitalWrite(LED_PIN, HIGH);
    delay(delay_ms);
  }
}

/******************************************************************************
 * Main
 */

void setup() {
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Initialize serial (for debugging: wait a few seconds)
  Serial.begin(115200);
  delay(5000);
  Serial.println();
  Serial.println("OTA Demo");

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Confirm application image or roll back to previous version
  OtaConfirmImage();

  // Register OTA callback functions
  ArduinoOTA.onStart(OtaOnStart);
  ArduinoOTA.onProgress(OtaOnProgress);
  ArduinoOTA.onEnd(OtaOnEnd);
  ArduinoOTA.onError(OtaOnError);

  // Initialize OTA service
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.begin();
}

void loop() {
  // Advertise OTA service on the network and accept incoming image requests
  ArduinoOTA.handle();

  // Do something: blink the LED, print uptime
  blinkLed(NUM_BLINKS, 100);
  Serial.printf("[v%s] Uptime: %lu ms\n", 
                FW_VERSION, 
                (unsigned long)millis());

  // Wait some time (better: make this non-blocking)
  delay(1000);
}
