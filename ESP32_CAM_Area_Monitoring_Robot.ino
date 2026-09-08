#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"        // Disable brownout detector
#include "soc/rtc_cntl_reg.h"  // Disable brownout detector

void startCameraServer();

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_M5STACK_PSRAM
#define CAMERA_MODEL_AI_THINKER // PENTING: Pastikan ini yang benar untuk board 
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// ====== Pin Motor ======
#define MOTOR_1_PIN_1 12
#define MOTOR_1_PIN_2 13
#define MOTOR_2_PIN_1 14
#define MOTOR_2_PIN_2 15

// ====== Pin LED ======
int gpLed = 4; // Light

// ====== Pin Kamera ESP32-CAM (AI Thinker) ======
#if defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM        35
#define Y8_GPIO_NUM        34
#define Y7_GPIO_NUM        39
#define Y6_GPIO_NUM        36
#define Y5_GPIO_NUM        21
#define Y4_GPIO_NUM        19
#define Y3_GPIO_NUM        18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#else
#error "Camera model not selected"
#endif


// ====== Fungsi Motor ======
void moveForward() {
  digitalWrite(MOTOR_1_PIN_1, LOW);
  digitalWrite(MOTOR_1_PIN_2, HIGH);
  digitalWrite(MOTOR_2_PIN_1, HIGH);
  digitalWrite(MOTOR_2_PIN_2, LOW);
  Serial.println("Action: Forward");
}

void moveBackward() {
  digitalWrite(MOTOR_1_PIN_1, HIGH);
  digitalWrite(MOTOR_1_PIN_2, LOW);
  digitalWrite(MOTOR_2_PIN_1, LOW);
  digitalWrite(MOTOR_2_PIN_2, HIGH);
  Serial.println("Action: Backward");
}

void turnLeft() {
  digitalWrite(MOTOR_1_PIN_1, LOW);
  digitalWrite(MOTOR_1_PIN_2, HIGH); // M1 Forward
  digitalWrite(MOTOR_2_PIN_1, LOW);
  digitalWrite(MOTOR_2_PIN_2, HIGH); // M2 Backward
  Serial.println("Action: Turn Left");
}

void turnRight() {
  digitalWrite(MOTOR_1_PIN_1, HIGH); // M1 Backward
  digitalWrite(MOTOR_1_PIN_2, LOW);
  digitalWrite(MOTOR_2_PIN_1, HIGH); // M2 Forward
  digitalWrite(MOTOR_2_PIN_2, LOW);
  Serial.println("Action: Turn Right");
}

void stopMotors() {
  digitalWrite(MOTOR_1_PIN_1, LOW);
  digitalWrite(MOTOR_1_PIN_2, LOW);
  digitalWrite(MOTOR_2_PIN_1, LOW);
  digitalWrite(MOTOR_2_PIN_2, LOW);
  Serial.println("Action: Stop");
}

// Global handle for the HTTP server instance
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL; // Ini untuk kontrol URL, bukan streaming

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector
  Serial.begin(115200);
  Serial.setDebugOutput(true); // Pastikan debug output aktif
  Serial.println();

  // Inisialisasi pin motor dan LED
  pinMode(MOTOR_1_PIN_1, OUTPUT);
  pinMode(MOTOR_1_PIN_2, OUTPUT);
  pinMode(MOTOR_2_PIN_1, OUTPUT);
  pinMode(MOTOR_2_PIN_2, OUTPUT);
  pinMode(gpLed, OUTPUT);

  // Set semua pin motor dan LED ke LOW saat start
  stopMotors();
  digitalWrite(gpLed, LOW);


  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA; // Resolusi tinggi
    config.jpeg_quality = 10;           // Kualitas JPEG
    config.fb_count = 2;                // Jumlah frame buffer
  } else {
    config.frame_size = FRAMESIZE_SVGA; // Resolusi standar
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // drop down frame size for higher initial frame rate (opsional, bisa dihapus jika ingin langsung pakai resolusi tinggi)
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_CIF); // Set ke CIF dulu untuk start-up cepat

  // Koneksi WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Panggil fungsi server kamera
  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect"); // Tidak perlu menyimpan ke WiFiAddr jika tidak dipakai di tempat lain
}

void loop() {
  // Loop kosong, server web dan streaming kamera berjalan di background
}


// --- BAGIAN INI ADALAH IMPLEMENTASI DARI app_httpd.cpp ---

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

static size_t jpg_encode_stream(void *arg, size_t index, const void *data, size_t len) {
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if (!j) {
        return 0;
    }
    if (index + len > j->len) {
        len = j->len - index;
    }
    httpd_resp_send_chunk(j->req, (const char *)data, len);
    return len;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];
    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    Serial.println("JPEG conversion failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf = fb->buf;
                _jpg_buf_len = fb->len;
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
    }

    last_frame = 0;
    return res;
}

static esp_err_t cmd_handler(httpd_req_t *req){
    char* buf;
    size_t buf_len;
    char variable[32];
    char value[32];

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) {
        if(s) {
            int val = atoi(value);
            res = s->set_framesize(s, (framesize_t)val);
             // Tambahkan Serial.println untuk debug jika ingin melihat perubahan framesize
             Serial.printf("Set framesize to %d\n", val);
        }
    }
    else if (!strcmp(variable, "action")) {
        if (!strcmp(value, "forward")) {
            moveForward();
        } else if (!strcmp(value, "backward")) {
            moveBackward();
        } else if (!strcmp(value, "left")) {
            turnLeft();
        } else if (!strcmp(value, "right")) {
            turnRight();
        } else if (!strcmp(value, "stop")) {
            stopMotors();
        } else if (!strcmp(value, "led_on")) {
            digitalWrite(gpLed, HIGH);
            Serial.println("LED ON");
        } else if (!strcmp(value, "led_off")) {
            digitalWrite(gpLed, LOW);
            Serial.println("LED OFF");
        } else {
            res = -1; // Unknown action
        }
    }
    else {
        res = -1; // Unknown variable
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
}

// Halaman HTML untuk Antarmuka Kontrol
const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>PIRA BUSTAC by RATU</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 0;
            background-color: #e9ecef; /* Latar belakang lebih lembut */
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            min-height: 100vh;
        }
        .container {
            background-color: #fff;
            padding: 30px; /* Padding lebih besar */
            border-radius: 10px; /* Lebih bulat */
            box-shadow: 0 4px 10px rgba(0,0,0,0.15); /* Bayangan lebih jelas */
            text-align: center;
            max-width: 90%;
            width: 500px;
        }
        h1 {
            color: #333;
            margin-bottom: 25px; /* Spasi bawah lebih */
            text-shadow: 1px 1px 2px rgba(0,0,0,0.1); /* Bayangan teks */
        }
        .video-container {
            margin-bottom: 25px; /* Spasi bawah */
            border: 3px solid #007bff; /* Border lebih tebal, warna BIRU */
            border-radius: 8px; /* Sudut sedikit lebih bulat */
            overflow: hidden;
            box-shadow: 0 6px 12px rgba(0, 0, 0, 0.25); /* Bayangan lebih kuat */
        }
        img {
            width: 100%;
            height: auto;
            display: block;
        }
        .logo { /* Style untuk logo jika ditambahkan */
            width: 80px;
            height: auto;
            margin-bottom: 20px; /* Spasi bawah logo */
        }
        .controls { /* Mengatur tata letak kontrol lainnya */
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 12px;
            margin-bottom: 25px;
        }
        /* CSS untuk tata letak kontrol motor baru (D-pad) */
        .control-grid {
            display: flex;
            flex-direction: column; /* Baris diatur secara vertikal */
            align-items: center;    /* Pusatkan baris */
            gap: 10px; /* Jarak antara baris (atas-bawah) */
            margin-bottom: 25px; /* Jarak bawah dari grup kontrol motor */
        }
        .grid-row {
            display: flex;
            justify-content: center; /* Pusatkan tombol di setiap baris */
            gap: 12px; /* Jarak antar tombol di baris yang sama (kiri-kanan) */
            width: 100%; /* Memastikan baris mengambil lebar penuh parent */
        }
        button {
            background-color: #007bff;
            color: white;
            border: none;
            padding: 12px 20px;
            border-radius: 8px;
            cursor: pointer;
            font-size: 16px;
            min-width: 85px; /* Lebar minimum sedikit disesuaikan untuk D-pad */
            box-shadow: 2px 2px 5px rgba(0, 0, 0, 0.3);
            transition: background-color 0.2s ease, box-shadow 0.2s ease;
            font-weight: bold;
        }
        button:hover {
            background-color: #0056b3;
            box-shadow: 3px 3px 7px rgba(0, 0, 0, 0.4);
        }
        button:active {
            background-color: #003d80;
            box-shadow: 1px 1px 3px rgba(0, 0, 0, 0.2);
        }
        /* Gaya khusus untuk tombol Stop */
        .stop-button {
            background-color: #dc3545; /* Warna merah untuk Stop */
        }
        .stop-button:hover {
            background-color: #c82333;
        }
        .stop-button:active {
            background-color: #bd2130;
        }
        .control-group {
            margin-bottom: 20px;
            width: 100%;
        }
        .control-group h2 {
            font-size: 1.3em;
            color: #444;
            margin-bottom: 12px;
            text-shadow: 1px 1px 2px rgba(0,0,0,0.05);
        }
        .slider-container {
            display: flex;
            align-items: center;
            gap: 15px;
            margin-top: 15px;
        }
        .slider {
            width: 100%;
        }
        .slider-label {
            min-width: 100px;
            text-align: right;
            font-weight: bold;
            color: #555;
        }
        select {
            padding: 8px;
            border-radius: 5px;
            border: 1px solid #ccc;
            font-size: 16px;
            background-color: white;
            flex-grow: 1;
            box-shadow: inset 1px 1px 3px rgba(0,0,0,0.1);
            appearance: none;
            -webkit-appearance: none;
            -moz-appearance: none;
            background-image: url('data:image/svg+xml;utf8,<svg fill="%23333333" height="24" viewBox="0 0 24 24" width="24" xmlns="http://www.w3.org/2000/svg"><path d="M7 10l5 5 5-5z"/><path d="M0 0h24v24H0z" fill="none"/></svg>');
            background-repeat: no-repeat;
            background-position: right 8px center;
            background-size: 20px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>PIRA BUSTAC by RATU</h1>
        <div class="video-container">
            <img id="stream" src="http://172.20.10.2:81/stream">
        </div>

        <div class="control-group">
            <h2>Kontrol Motor</h2>
            <div class="control-grid">
                <div class="grid-row">
                    <button onclick="sendMomentaryCommand('forward')">Maju</button>
                </div>
                <div class="grid-row">
                    <button onclick="sendMomentaryCommand('left')">Kiri</button>
                    <button class="stop-button" onclick="sendCommand('action', 'stop')">Berhenti</button>
                    <button onclick="sendMomentaryCommand('right')">Kanan</button>
                </div>
                <div class="grid-row">
                    <button onclick="sendMomentaryCommand('backward')">Mundur</button>
                </div>
            </div>
        </div>

        <div class="control-group">
            <h2>Kontrol LED</h2>
            <div class="controls">
                <button onclick="sendCommand('action', 'led_on')">LED ON</button>
                <button onclick="sendCommand('action', 'led_off')">LED OFF</button>
            </div>
        </div>

        <div class="control-group">
            <h2>Pengaturan Kamera</h2>
            <div class="slider-container">
                <label for="framesize" class="slider-label">Ukuran Bingkai:</label>
                <select id="framesize" onchange="sendCommand('framesize', this.value)">
                    <option value="10">1600x1200 - Ultra</option>
                    <option value="6" selected>640x480 - Standar</option>
                    <option value="2">160x120 - Rendah</option>
                </select>
            </div>
        </div>

    </div>

    <script>
        // PENTING: Sesuaikan ini jika IP Anda berubah!
        const ESP32_IP = "172.20.10.2"; 
        const STREAM_PORT = "81"; // Port untuk streaming video
        const CONTROL_PORT = "80"; // Port untuk kontrol

        document.getElementById('stream').src = `http://${ESP32_IP}:${STREAM_PORT}/stream`;

        function sendCommand(variable, value) {
            const url = `http://${ESP32_IP}:${CONTROL_PORT}/control?var=${variable}&val=${value}`;
            fetch(url)
                .then(response => {
                    if (!response.ok) {
                        console.error('Network response was not ok:', response.statusText);
                    }
                    return response.text();
                })
                .then(data => {
                    console.log(`Command sent: ${variable}=${value}, Response:`, data);
                })
                .catch(error => {
                    console.error('Error sending command:', error);
                });
        }

        // Fungsi baru untuk mengirim perintah gerakan sementara
        function sendMomentaryCommand(action) {
            // Durasi gerakan (dalam milidetik)
            let duration;
            if (action === 'forward' || action === 'backward') {
                // DURASI MAJU/MUNDUR DIUBAH MENJADI 300ms
                duration = 300; // Coba 0.3 detik untuk maju/mundur yang lebih lembut
            } else {
                // DURASI BELOK KIRI/KANAN TETAP 100ms
                duration = 100; // 0.1 detik untuk belok "sedikit"
            }

            sendCommand('action', action); // Kirim perintah gerakan
            setTimeout(() => {
                sendCommand('action', 'stop'); // Kirim perintah berhenti setelah durasi
            }, duration);
        }
    </script>
</body>
</html>
)rawliteral";


// Handler baru untuk halaman utama (root path)
static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}


void startCameraServer(){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
