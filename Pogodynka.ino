#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>

// Wyświetlacz OLED 128x64
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS  0x3C

// DHT22
#define DHT_PIN 15
#define DHT_TYPE DHT22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_BMP085 bmp;
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;

bool bmpAvailable = false;
unsigned long lastUpdate = 0;
bool wifiAvailable = false;
bool accessPointMode = false;

float temperaturaDHT = NAN;
float wilgotnosc = NAN;
float temperaturaBMP = 0.0;
float cisnienieHPA = 0.0;
int32_t wysokosc = 0;

const byte DNS_PORT = 53;
const char* AP_NAME = "Stacja-Pogodowa";
const char* AP_PASSWORD = "pogodynka";

void showWiFiStatus()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("STACJA POGODOWA");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 20);
  if (accessPointMode)
  {
    display.println("Tryb konfiguracji");
    display.setCursor(0, 32);
    display.println(AP_NAME);
    display.setCursor(0, 44);
    display.printf("IP: %s", WiFi.softAPIP().toString().c_str());
  }
  else
  {
    display.println("WLAN polaczony");
    display.setCursor(0, 32);
    display.println(WiFi.localIP().toString().c_str());
  }
  display.display();
}

void handleRoot();
void handleApi();
void handleConfigure();
void handleNotFound();

void startAccessPoint()
{
  accessPointMode = true;
  wifiAvailable = false;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  Serial.println("Uruchomiono portale konfiguracji Wi-Fi");
  Serial.print("Siec: ");
  Serial.println(AP_NAME);
  Serial.print("Haslo: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Otworz: http://");
  Serial.println(WiFi.softAPIP());
  showWiFiStatus();
}

void connectWiFi()
{
  preferences.begin("pogoda", false);
  String ssid = preferences.getString("ssid", "");
  String password = preferences.getString("password", "");
  preferences.end();

  if (ssid.length() == 0)
  {
    // Wokwi udostepnia domyslnie siec Wokwi-GUEST.
    // Najpierw probujemy ja polaczyc; portal AP uruchomi sie dopiero,
    // gdy stacja nie zostanie uruchomiona w symulatorze lub innej sieci.
    ssid = "Wokwi-GUEST";
    password = "";
  }

  WiFi.mode(WIFI_STA);
  if (ssid == "Wokwi-GUEST")
  {
    // Kanał 6 jest wymagany przez domyslna siec Wokwi-GUEST.
    WiFi.begin(ssid.c_str(), password.c_str(), 6);
  }
  else
  {
    WiFi.begin(ssid.c_str(), password.c_str());
  }

  Serial.print("Laczenie z siecia ");
  Serial.println(ssid);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000)
  {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiAvailable = true;
    accessPointMode = false;

    preferences.begin("pogoda", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();

    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Strona: http://");
    Serial.println(WiFi.localIP());
    Serial.print("API: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/api");
    showWiFiStatus();
  }
  else
  {
    Serial.println("Nie udalo sie polaczyc. Uruchamiam portal konfiguracji.");
    startAccessPoint();
  }
}

void displayError(const char* message)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(message);
  display.display();
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  // Przewody SDA i SCL dla BMP180 i OLED
  Wire.begin(21, 22);

  dht.begin();

  if (!bmp.begin())
  {
    Serial.println("Nie znaleziono BMP180!");
    bmpAvailable = false;
  }
  else
  {
    bmpAvailable = true;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("Nie znaleziono OLED!");
    while (true)
    {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println("Stacja pogodowa");
  display.setCursor(0, 35);
  display.println("Inicjalizacja...");
  display.display();

  delay(2000);

  connectWiFi();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api", HTTP_GET, handleApi);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Serwer WWW uruchomiony");
}

void loop()
{
  if (accessPointMode)
  {
    dnsServer.processNextRequest();
  }
  server.handleClient();

  // DHT22 nie powinien być odczytywany częściej niż co 2 sekundy
  if (millis() - lastUpdate < 3000)
  {
    return;
  }

  lastUpdate = millis();

  temperaturaDHT = dht.readTemperature();
  wilgotnosc = dht.readHumidity();

  if (isnan(temperaturaDHT) || isnan(wilgotnosc))
  {
    Serial.println("Blad odczytu DHT22!");
    displayError("Blad odczytu DHT22");
    return;
  }

  if (bmpAvailable)
  {
    temperaturaBMP = bmp.readTemperature();
    cisnienieHPA = bmp.readPressure() / 100.0F;
    wysokosc = bmp.readAltitude(101325.0F); // Cisnienie na poziomie morza w Pa
  }

  // Dane do Serial Monitor
  Serial.println("--- Stacja pogodowa ---");
  Serial.printf("DHT22 temperatura: %.1f C\n", temperaturaDHT);
  Serial.printf("Wilgotnosc: %.1f %%\n", wilgotnosc);

  if (bmpAvailable)
  {
    Serial.printf("BMP180 temperatura: %.1f C\n", temperaturaBMP);
    Serial.printf("Cisnienie: %.1f hPa\n", cisnienieHPA);
    Serial.printf("Wysokosc: %ld m\n", (long)wysokosc);
  }

  // Ekran OLED
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("STACJA POGODOWA");

  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  display.setCursor(0, 16);
  display.printf("Temp DHT: %.1f C", temperaturaDHT);

  display.setCursor(0, 28);
  display.printf("Wilg.: %.1f %%", wilgotnosc);

  if (bmpAvailable)
  {
    display.setCursor(0, 40);
    display.printf("Cisn.: %.1f hPa", cisnienieHPA);

    display.setCursor(0, 52);
    display.printf("BMP: %.1f C", temperaturaBMP);
  }
  else
  {
    display.setCursor(0, 40);
    display.println("BMP180: brak");
  }

  display.display();
}

void handleRoot()
{
  if (accessPointMode)
  {
    String page = R"rawliteral(
<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Konfiguracja Wi-Fi</title>
  <style>
    body{font-family:Arial,sans-serif;background:#dbeafe;margin:0;padding:20px}
    form{max-width:420px;margin:auto;background:white;padding:24px;border-radius:14px}
    input,button{box-sizing:border-box;width:100%;padding:12px;margin:8px 0}
    button{border:0;border-radius:8px;background:#2563eb;color:white;font-size:16px}
  </style>
</head>
<body>
  <form action="/configure" method="POST">
    <h2>Konfiguracja Wi-Fi</h2>
    <label>SSID</label>
    <input name="ssid" required placeholder="Wokwi-GUEST">
    <label>Haslo</label>
    <input name="password" type="password">
    <button type="submit">Zapisz i polacz</button>
  </form>
</body>
</html>
)rawliteral";
    server.send(200, "text/html; charset=utf-8", page);
    return;
  }

  String page = R"rawliteral(
<!doctype html>
<html lang="pl">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>Stacja pogodowa</title>
  <style>
    :root{font-family:Arial,sans-serif;color:#172033}
    body{margin:0;background:#eef5ff}
    header{background:#2563eb;color:white;padding:18px;text-align:center}
    main{max-width:900px;margin:auto;padding:16px}
    .cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(145px,1fr));gap:12px}
    .card{background:white;border-radius:12px;padding:16px;text-align:center;
          box-shadow:0 4px 14px #0001}
    .value{display:block;font-size:28px;font-weight:bold;margin-top:8px}
    canvas{width:100%;height:260px;background:white;border-radius:12px;
           box-shadow:0 4px 14px #0001;margin-top:16px}
  </style>
</head>
<body>
  <header><h1>Stacja pogodowa</h1></header>
  <main>
    <section class="cards">
      <div class="card">Temperatura DHT<span id="temp" class="value">-- C</span></div>
      <div class="card">Wilgotnosc<span id="humidity" class="value">-- %</span></div>
      <div class="card">Cisnienie<span id="pressure" class="value">-- hPa</span></div>
      <div class="card">Temperatura BMP<span id="bmpTemp" class="value">-- C</span></div>
    </section>
    <canvas id="chart" width="900" height="260"></canvas>
  </main>
  <script>
    const history=[];
    const canvas=document.getElementById('chart');
    const ctx=canvas.getContext('2d');

    async function refresh(){
      try{
        const data=await (await fetch('/api')).json();
        document.getElementById('temp').textContent=data.tempDHT.toFixed(1)+' C';
        document.getElementById('humidity').textContent=data.humidity.toFixed(1)+' %';
        document.getElementById('pressure').textContent=data.pressure.toFixed(1)+' hPa';
        document.getElementById('bmpTemp').textContent=data.tempBMP.toFixed(1)+' C';
        history.push({t:data.tempDHT,h:data.humidity,p:data.pressure});
        if(history.length>30) history.shift();
        drawChart();
      }catch(e){
        console.log('Brak odpowiedzi serwera',e);
      }
    }

    function drawChart(){
      ctx.clearRect(0,0,canvas.width,canvas.height);
      ctx.fillStyle='#172033';
      ctx.font='14px Arial';
      ctx.fillText('Temperatura DHT (C) - ostatnie pomiary',18,22);
      if(history.length<2)return;
      const values=history.map(x=>x.t);
      let min=Math.min(...values)-1,max=Math.max(...values)+1;
      if(min===max) max=min+1;
      ctx.beginPath();
      ctx.strokeStyle='#2563eb';
      ctx.lineWidth=3;
      history.forEach((x,i)=>{
        const px=30+i*(840/(history.length-1));
        const py=230-(x.t-min)*(190/(max-min));
        i?ctx.lineTo(px,py):ctx.moveTo(px,py);
      });
      ctx.stroke();
    }
    refresh();
    setInterval(refresh,3000);
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html; charset=utf-8", page);
}

void handleApi()
{
  String json = "{";
  json += "\"tempDHT\":" + String(isnan(temperaturaDHT) ? 0.0 : temperaturaDHT, 1) + ",";
  json += "\"humidity\":" + String(isnan(wilgotnosc) ? 0.0 : wilgotnosc, 1) + ",";
  json += "\"tempBMP\":" + String(temperaturaBMP, 1) + ",";
  json += "\"pressure\":" + String(cisnienieHPA, 1) + ",";
  json += "\"altitude\":" + String(wysokosc) + ",";
  json += "\"bmpAvailable\":" + String(bmpAvailable ? "true" : "false") + ",";
  json += "\"ip\":\"" + (wifiAvailable ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleConfigure()
{
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  ssid.trim();

  if (ssid.length() == 0)
  {
    server.send(400, "text/plain", "SSID nie moze byc pusty");
    return;
  }

  preferences.begin("pogoda", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  server.send(200, "text/html; charset=utf-8",
    "<html><body><h2>Zapisano. ESP32 uruchomi sie ponownie.</h2>"
    "<p>Po restarcie otworz adres pokazany na OLED.</p></body></html>");
  delay(1000);
  ESP.restart();
}

void handleNotFound()
{
  if (accessPointMode)
  {
    handleRoot();
  }
  else
  {
    server.send(404, "text/plain", "Nie znaleziono");
  }
}