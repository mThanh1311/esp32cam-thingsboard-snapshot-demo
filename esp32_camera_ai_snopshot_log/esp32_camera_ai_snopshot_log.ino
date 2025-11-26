/*******************************************************
 * ESP32-CAM AI Thinker + Edge Impulse + ThingsBoard Cloud
 * Version: ADVANCED TERMINAL LOG EDITION
 *
 * ✔ AI Inference (96×96 RGB)
 * ✔ Snapshot upload
 * ✔ Terminal realtime log (each line = 1 telemetry)
 * ✔ FIFO log buffer (auto resend when WiFi reconnect)
 * ✔ Timestamp HH:MM:SS
 * ✔ Log tagging: INFO / WARN / ERR
 *******************************************************/

#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include "base64.h"

// ====== WIFI ======
const char* ssid     = "KDT";
const char* password = "dkdt1813";

// ====== THINGSBOARD HTTP ======
String TB_TOKEN = "kqx3BKLRbOTY0HhbeVg4";
String TB_URL   = "http://thingsboard.cloud/api/v1/";

// ====== EDGE IMPULSE ======
#include <plant_bean_classification_custom_mt_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

// ====== CAMERA MODEL ======
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// ====== EI INPUT SIZE ======
#define EI_COLS 96
#define EI_ROWS 96
uint8_t* ei_buf;
uint8_t* resize_buf;

// ====== PERFORMANCE METRICS ======
unsigned long lastFrame = 0;
float fps = 0;

// =======================================================
// TERMINAL LOG: FIFO BUFFER
// =======================================================
#define LOG_FIFO_SIZE 50
String logFIFO[LOG_FIFO_SIZE];
int fifo_head = 0;
int fifo_tail = 0;

bool fifo_empty() { return fifo_head == fifo_tail; }
bool fifo_full()  { return (fifo_head + 1) % LOG_FIFO_SIZE == fifo_tail; }

void fifo_push(String msg) {
    if (fifo_full()) {
        fifo_tail = (fifo_tail + 1) % LOG_FIFO_SIZE; // drop oldest
    }
    fifo_head = (fifo_head + 1) % LOG_FIFO_SIZE;
    logFIFO[fifo_head] = msg;
}

String fifo_pop() {
    if (fifo_empty()) return "";
    fifo_tail = (fifo_tail + 1) % LOG_FIFO_SIZE;
    return logFIFO[fifo_tail];
}

// =======================================================
// LOG FORMAT
// =======================================================
String timeTag() {
    unsigned long t = millis() / 1000;
    int h = t / 3600;
    int m = (t % 3600) / 60;
    int s = (t % 60);

    char buf[20];
    sprintf(buf, "[%02d:%02d:%02d]", h, m, s);
    return String(buf);
}

void LOG_INFO(String msg) {
    fifo_push(timeTag() + " [INFO] " + msg);
    Serial.println("[INFO] " + msg);
}

void LOG_WARN(String msg) {
    fifo_push(timeTag() + " [WARN] " + msg);
    Serial.println("[WARN] " + msg);
}

void LOG_ERR(String msg) {
    fifo_push(timeTag() + " [ERR] " + msg);
    Serial.println("[ERR] " + msg);
}

// =======================================================
// SEND LOG TO THINGSBOARD (ONE LINE EACH PACKET)
// =======================================================
void sendLogFIFO() {
    if (WiFi.status() != WL_CONNECTED) return;
    if (fifo_empty()) return;

    HTTPClient http;
    String url = TB_URL + TB_TOKEN + "/telemetry";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // send up to 5 lines per loop (avoid spam)
    for (int i = 0; i < 5; i++) {
        if (fifo_empty()) break;

        String line = fifo_pop();
        line.replace("\"", "\\\"");
        line.replace("\n", " ");

        String json = "{\"log\":\"" + line + "\"}";
        http.POST(json);

        Serial.println("[TB] " + line);
    }

    http.end();
}

void checkThresholds(int leaf, int soil, int uv, int aqi) {
    // Leaf Wetness
    if (leaf > 80) LOG_ERR("Leaf Wetness CRITICAL (" + String(leaf) + "%)");
    else if (leaf > 60) LOG_WARN("Leaf Wetness HIGH (" + String(leaf) + "%)");

    // Soil Moisture
    if (soil > 90) LOG_ERR("Soil Moisture FLOOD RISK (" + String(soil) + "%)");
    else if (soil < 20) LOG_ERR("Soil Moisture TOO DRY (" + String(soil) + "%)");
    else if (soil > 70 || soil < 40) LOG_WARN("Soil Moisture WARNING (" + String(soil) + "%)");

    // UV Index
    if (uv >= 11) LOG_ERR("UV EXTREME (" + String(uv) + ")");
    else if (uv >= 8) LOG_ERR("UV VERY HIGH (" + String(uv) + ")");
    else if (uv >= 6) LOG_WARN("UV HIGH (" + String(uv) + ")");

    // AQI
    if (aqi >= 200) LOG_ERR("AQI HAZARDOUS (" + String(aqi) + ")");
    else if (aqi >= 150) LOG_ERR("AQI UNHEALTHY (" + String(aqi) + ")");
    else if (aqi >= 100) LOG_WARN("AQI POOR (" + String(aqi) + ")");
}

// =======================================================
// CAMERA INIT
// =======================================================
bool initCamera() {
    camera_config_t config;

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 15;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_PSRAM;

    esp_err_t err = esp_camera_init(&config);
    return (err == ESP_OK);
}

// =======================================================
// EI CALLBACK
// =======================================================
int ei_get_data(size_t offset, size_t length, float *out_ptr) {
    size_t ix = offset * 3;
    while (length--) {
        out_ptr[0] = 
            (resize_buf[ix + 2] << 16) |
            (resize_buf[ix + 1] << 8) |
             resize_buf[ix];
        ix += 3;
        out_ptr++;
    }
    return 0;
}

// =======================================================
// CAPTURE + RESIZE TO 96x96 FOR EI
// =======================================================
bool capture_ei_frame() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;

    bool ok = fmt2rgb888(fb->buf, fb->len, fb->format, ei_buf);
    esp_camera_fb_return(fb);
    if (!ok) return false;

    ei::image::processing::resize_image(
        ei_buf, 320, 240,
        resize_buf, EI_COLS, EI_ROWS,
        3
    );
    return true;
}

// =======================================================
// SEND SNAPSHOT & AI RESULT TO THINGSBOARD
// =======================================================
void send_snapshot_ai(String b64, String ai_class, float conf) {
    HTTPClient http;
    String url = TB_URL + TB_TOKEN + "/telemetry";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String json = "{";
    json += "\"image\":\"data:image/jpeg;base64," + b64 + "\",";
    json += "\"ai_class\":\"" + ai_class + "\",";
    json += "\"ai_conf\":" + String(conf, 3);
    json += "}";

    http.POST(json);
    http.end();
}

void sendSensorMock() {
    if (WiFi.status() != WL_CONNECTED) return;

    int leaf = random(20, 96);
    int soil = random(30, 91);
    int uv   = random(0, 11);
    int aqi  = random(10, 121);

    HTTPClient http;
    String url = TB_URL + TB_TOKEN + "/telemetry";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    String payload = "{";
    payload += "\"leaf_wet\":" + String(leaf) + ",";
    payload += "\"soil_moist\":" + String(soil) + ",";
    payload += "\"uv_index\":" + String(uv) + ",";
    payload += "\"aqi\":" + String(aqi);
    payload += "}";
    
    checkThresholds(leaf, soil, uv, aqi);

    http.POST(payload);
    http.end();

    LOG_INFO("Sensor Mock → Leaf=" + String(leaf) +
             "%, Soil=" + String(soil) +
             "%, UV=" + String(uv) +
             ", AQI=" + String(aqi));
}

// =======================================================
// SETUP
// =======================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    if (!initCamera()) {
        LOG_ERR("Camera init FAILED!");
        while (1);
    }

    ei_buf = (uint8_t*)malloc(320 * 240 * 3);
    resize_buf = (uint8_t*)malloc(EI_COLS * EI_ROWS * 3);

    WiFi.begin(ssid, password);
    LOG_INFO("Connecting WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(300);
    }

    LOG_INFO("WiFi OK, IP=" + WiFi.localIP().toString());
}

// =======================================================
// LOOP
// =======================================================
void loop() {

    // ==== FPS ====
    unsigned long now = millis();
    float frameMs = now - lastFrame;
    lastFrame = now;
    fps = 1000.0 / frameMs;

    // ==== CAPTURE EI ====
    if (!capture_ei_frame()) {
        LOG_ERR("EI frame capture failed");
        return;
    }

    // ==== SIGNAL ====
    ei::signal_t signal;
    signal.total_length = EI_COLS * EI_ROWS;
    signal.get_data = &ei_get_data;

    // ==== RUN AI ====
    unsigned long t_ai = millis();
    ei_impulse_result_t result;
    run_classifier(&signal, &result, false);
    t_ai = millis() - t_ai;

    // Best class
    int best = 0;
    float best_conf = 0;
    for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > best_conf) {
            best_conf = result.classification[i].value;
            best = i;
        }
    }
    String ai_class = ei_classifier_inferencing_categories[best];

    // ==== CAPTURE JPEG ====
    unsigned long t_up = millis();
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        LOG_ERR("JPEG capture failed");
        return;
    }

    String b64 = base64::encode(fb->buf, fb->len);
    esp_camera_fb_return(fb);

    // ==== UPLOAD ====
    send_snapshot_ai(b64, ai_class, best_conf);
    t_up = millis() - t_up;

    // ==== LOG ====
    LOG_INFO("Frame OK (" + String(frameMs,0) + "ms, FPS=" + String(fps,1) + ")");
    LOG_INFO("AI " + ai_class + " (" + String(best_conf,3) + "), infer=" + String(t_ai) + "ms");
    LOG_INFO("Upload " + String(fb->len/1024.0,1) + "KB (" + String(t_up) + "ms)");

    // ==== SEND LOG ====
    sendLogFIFO();

    static unsigned long lastSensor = 0;
    if (millis() - lastSensor > 5000) {
        sendSensorMock();
        lastSensor = millis();
    }

    delay(200);
}