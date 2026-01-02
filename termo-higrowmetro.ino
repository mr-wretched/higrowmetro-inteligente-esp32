//Sensor DHT22
#include "DHT.h"
#define DHTPIN 23      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT22  // DHT 22 (AM2302), AM2321
DHT dht(DHTPIN, DHTTYPE);
float rh = 0.0;
float t = 0.0;
float ah = 0.0;
float svp = 0.0;
float avp = 0.0;
float vpd = 0.0;
float hic = 0.0;
unsigned long lastTimeDHT = 0;
unsigned long tempoLeituraDHT = 2000;
bool isDHTok = true;
//

//Display
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

byte wifiOk[8] = {  //Símbolo Wifi Conectado e com internet
  B00000, B00000, B01110, B10001, B00100, B01010, B00000, B00100
};

byte wifiErro[8] = {  //Símbolo Wifi Desconectado ou Conectado sem internet
  B00000, B10001, B01010, B00100, B01010, B10001, B00000, B00000
};

byte internetOk[8] = {
  B00000, B00001, B00001, B00010, B00010, B10100, B10100, B01000
};

byte internetErro[8] = {
  B00000, B10001, B01010, B00100, B01010, B10001, B00000, B00000
};

byte cubo[8] = {  // Caractere para o "³"
  B11000, B00100, B11100, B00100, B11000, B00000, B00000, B00000
};

byte pa[8] = {  // Caractere para o "Pa"
  B11100, B10010, B11100, B10111, B10001, B10111, B10101, B10111
};
//

//Outros
unsigned long currentTime = 0;
bool isSetupFinished = false;
//

//Wifi
#include <WiFi.h>

struct WifiCred {
  const char* ssid;
  const char* pass;
};

WifiCred wifiList[] = { // Colocar redes wifi para que o esp tente conectar na que estiver com o melhor sinal
  { "rede1", "senha1" },
  { "rede2", "senh2" },
  //ad infinitum, só seguir o padrão
};


unsigned long tempoConexaoWifiLast = 0;
int tempoConexaoWifi = 5000;  // Timeout para tentativa de se conectar à rede Wifi
int contagemTentativasConexaoWifi = 0;
bool internetLastState = false;
const char* internetTestHost = "www.google.com";  // Servidor para pingar e verificar se há internet
const int internetTestPort = 80;                  // Porta 80 para HTTP
int contagemSemInternet = 0;                      // Conta minutos sem conexao com internet
unsigned long tempoConexaoInternetLast = 0;
int tempoConexaoInternet = 5000;          // Tempo entre verificações de conexão com a internet
const int tentativasConexaoInternet = 5;  // Se estiver sem internet X vezes, reinicia wifi
bool wifiState = false;
unsigned long conectarWifiInteligenteLastTime = 0;
unsigned long conectarWifiInteligenteTempo = 10000;
//

//Planilha google
#include <ArduinoJson.h>
#include <HTTPClient.h>
const char* scriptURL = "https://script.google.com/macros/s/exemplo/exec";  //URL do script da planilha google sheets
unsigned long lastTimeGoogle = 0;
unsigned long intervaloGoogle = 30000;  // Tempo para envio das informações para a planilha
//

//Data, Hora e Servidor NTP
#include "time.h"
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -10800;  // Ajuste para o Brasil (UTC -3: 3 x 3600 segundos)
const int daylightOffset_sec = 0;   // Ajuste de horário de verão (0 se não houver)
struct tm timeinfo;
//


//MQTT
#include <PubSubClient.h>
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* mqtt_topic = "exemplo/exemplo";
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastTimeReconexaoMQTT = 0;
unsigned long tempoReconexaoMQTT = 10000;
bool mqttLastState = false;
unsigned long lastTimeEnvioMQTT = 0;
unsigned long tempoEnvioMQTT = 10000;
//

int escolherMelhorRede() {

  const int wifiCount = sizeof(wifiList) / sizeof(wifiList[0]);

  Serial.println(">>>Procurando redes Wifi");
  int n = WiFi.scanNetworks(false, true);  // sinc scan, mostrar redes ocultas
  
  if (n <= 0) {
    Serial.println(">>>Nenhuma rede encontrada");
    return -1;
  }

  int melhorIndex = -1;
  int melhorRSSI = -1000;

  for (int i = 0; i < n; i++) {
    String ssidEncontrado = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    for (int j = 0; j < wifiCount; j++) {
      if (ssidEncontrado == wifiList[j].ssid) {
        if (rssi > melhorRSSI) {
          melhorRSSI = rssi;
          melhorIndex = j;
        }
      }
    }
  }

  if (melhorIndex >= 0) {
    Serial.print(">>>Melhor rede encontrada: ");
    Serial.print(wifiList[melhorIndex].ssid);
    Serial.print(" RSSI=");
    Serial.println(melhorRSSI);

  } else {
    Serial.println(">>>Nenhuma rede conhecida disponível");
  }

  return melhorIndex;
}

void conectarWifiInteligente() {

  if (currentTime - conectarWifiInteligenteLastTime > conectarWifiInteligenteTempo) {
    conectarWifiInteligenteLastTime = currentTime;

    resetarWifi();

    int index = escolherMelhorRede();
    if (index < 0) return;

    WiFi.begin(wifiList[index].ssid, wifiList[index].pass);

    Serial.print(">>>Conectando em ");
    Serial.println(wifiList[index].ssid);
  }
}

void resetarWifi() {
  Serial.println(">>>Resetando WiFi");
  WiFi.disconnect(true, true);  // esquecer AP atual + apagar configs
  WiFi.mode(WIFI_OFF);
  delay(300);
  WiFi.mode(WIFI_STA);
  delay(300);
}

String obterDataHoraString() {
  if (!getLocalTime(&timeinfo)) {
    return "sem_horario";
  }

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

void setupLcd() {
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, wifiOk);
  lcd.createChar(1, wifiErro);
  lcd.createChar(2, internetOk);
  lcd.createChar(3, internetErro);
  lcd.createChar(4, cubo);
  lcd.createChar(5, pa);
}

void setup() {
  Serial.begin(9600);

  //WiFi.begin(ssid, pass); //WiFi
  conectarWifiInteligente();
  Serial.println(">>>Iniciando Wifi");

  setupLcd();  //Display LCD
  Serial.println(F(">>>Iniciando display"));

  dht.begin();  //Sensor DHT
  Serial.println(F(">>>Iniciando sensor DHT"));


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  //Data e hora RTC
  getLocalTime(&timeinfo);
  Serial.println(F(">>>Configurando Data e Hora"));

  client.setServer(mqtt_server, mqtt_port);  //Servidor MQTT

  currentTime = millis();

  isSetupFinished = true;
}

void dataHora() {

  if (!getLocalTime(&timeinfo)) {
    Serial.println(">>>Falha ao obter data e hora");
    return;
  }
}

void termo() {


  if (isSetupFinished == false) {
    return;
  }

  if (currentTime - lastTimeDHT > tempoLeituraDHT) {
    lastTimeDHT = currentTime;
    rh = dht.readHumidity();    // Ler umidade relativa (250ms)
    t = dht.readTemperature();  // Ler temperatura

    if (isnan(rh) || isnan(t)) {  // Checar se leitura do sensor deu erro
      Serial.println(F(">>>Falha na leitura do sensor DHT"));
      isDHTok = false;
      return;

    } else {
      if (isDHTok == false) {
        isDHTok = true;
      }
    }

    ah = (6.112 * exp((17.67 * t) / (t + 243.5)) * rh * 2.1674) / (273.15 + t);  // Calcular umidade absoluta em g/m³
    svp = 0.61078 * exp((17.27 * t) / (t + 237.3));                              // Calcular SVP (Saturation Vapor Defict) em kPa
    avp = svp * (rh / 100);                                                      // Calcular AVP (Actual Vapor Deficit) em kPa
    vpd = svp - avp;                                                             // Calcular VPD (Vapor Pressure Defict) em kPa
    hic = dht.computeHeatIndex(t, rh, false);                                    // Calcular Sensação Térmica

    Serial.print(&timeinfo, "%d/%m/%Y %H:%M:%S ");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.print(("°C "));
    Serial.print("Sensação Térmica: ");
    Serial.print(hic);
    Serial.print("°C ");
    Serial.print("Umidade Relativa: ");
    Serial.print(rh);
    Serial.println("% ");
    Serial.print("Umidade Absoluta: ");
    Serial.print(ah);
    Serial.print("g/m3 ");
    Serial.print("V.P.D.: ");
    Serial.print(vpd, 1);
    Serial.println("kPa ");


    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(t, 1);
    lcd.print("/");
    lcd.print(hic, 1);
    lcd.print("C ");
    lcd.print(rh, 1);
    lcd.print("%");
    lcd.setCursor(0, 1);
    lcd.print(ah, 1);
    lcd.print("gm");
    lcd.write(4);
    lcd.print(" ");
    lcd.print(vpd, 1);
    lcd.print("k");
    lcd.write(5);
    lcd.print(" ");

    if (wifiState == true) {
      lcd.write(0);  // Desenha o ícone wifiOk

    } else {
      lcd.write(1);  // Desenha o ícone wifiErro
    }

    if (internetLastState == true) {
      lcd.write(2);  // Desenha o ícone internetOk

    } else {
      lcd.write(3);  // Desenha o ícone internetErro
    }
  }
}

void conexaoInternet() {  // Gerencia e monitora WiFi e internet, reinicia quando perde conexão

  if (WiFi.status() != 3) {  // Verifica se o Wifi está conectado, e caso não esteja, tenta reconectar
    if (internetLastState == true) {
      internetLastState = false;
      Serial.print(">>>Wifi Desconectado. Status: ");
      Serial.println(WiFi.status());
      wifiState = false;
    }


    if (WiFi.status() != 3) {  // Bloqueia o sistema enquanto o wifi estiver desconectado
      wifiState = false;
      if (currentTime - tempoConexaoWifiLast > tempoConexaoWifi) {
        tempoConexaoWifiLast = currentTime;
        contagemTentativasConexaoWifi++;

        if (contagemTentativasConexaoWifi == 10) {  // Aumenta de 5 para 20 segundos o tempo entre tentativas de conexão
          Serial.println(">>>Aumentando tempo entre tentativas (20s)");
          tempoConexaoWifi = 20000;
        }
        if (contagemTentativasConexaoWifi == 30) {  // Aumenta de 20 para 60 segundos o tempo entre tentativas de conexão
          Serial.println(">>>Aumentando tempo entre tentativas (60s)");
          tempoConexaoWifi = 60000;
        }
        if (contagemTentativasConexaoWifi > 100) {  // Reinicia o ESP32 caso o número de tentativas de conexão à rede WiFi ultrapasse 100
          Serial.println(">>>Limite de tentativas alcançado. Reiniciando sistema...");
          ESP.restart();
        }

        //WiFi.begin(ssid, pass);
        conectarWifiInteligente();
        Serial.print(">>>Conectando à rede Wifi: [");
        //Serial.print(ssid);
        Serial.print("] Tentativa ");
        Serial.println(contagemTentativasConexaoWifi);
      }
    }


  } else {  // Caso o Wifi esteja conectado, verifica conexão com a internet a cada 60 segundos
    if (wifiState == false) {
      wifiState = true;
    }
    if (currentTime - tempoConexaoInternetLast > tempoConexaoInternet) {
      if (internetLastState == false) {
        Serial.print(">>>Wifi Conectado: ");
        Serial.println(WiFi.SSID());

        contagemTentativasConexaoWifi = 0;
        tempoConexaoWifi = 5000;
        Serial.println(">>>Verificando conexão com a Internet");
      }

      tempoConexaoInternetLast = currentTime;
      HTTPClient http;
      http.begin(internetTestHost, internetTestPort, "/generate_204");
      int httpCode = http.GET();
      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT) {  // Conexão com a internet está OK
          if (internetLastState == false) {
            Serial.println(">>>Conexão com a Internet OK");
            Serial.print(">>>Verificações ocorrerão a cada ");
            tempoConexaoInternet = 60000;
            Serial.print(tempoConexaoInternet / 1000);
            Serial.println(" segundos");
            contagemSemInternet = 0;
            internetLastState = true;
            dataHora();
          }

          http.end();  // Libera os recursos
        }


      } else {  // Erro na conexão HTTP
        if (internetLastState == true) {
          internetLastState = false;
        }
        Serial.print(">>>Não há conexão com a Internet (");
        tempoConexaoInternet = 5000;

        contagemSemInternet++;
        Serial.print(contagemSemInternet);
        Serial.println(" vezes)");
        if (contagemSemInternet == 5) {
          Serial.println(">>>Reiniciando conexão Wifi");
          //WiFi.disconnect();
          //apagado apos mudança de banco de dados de redes wifi que ja gerencia reinicio do wifi
        }
        if (contagemSemInternet == 15) {
          Serial.println(">>>Reiniciando Sistema");
          ESP.restart();
        }
      }
      http.end();
    }
  }

  if (!internetLastState && client.connected()) {
    client.disconnect();
  }
}

void enviarPlanilha() {  // Envia medições do higrômetro para a planilha no google sheets
  if (currentTime - lastTimeGoogle > intervaloGoogle) {
    lastTimeGoogle = currentTime;
    HTTPClient http;

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(scriptURL);
    http.addHeader("Content-Type", "application/json");

    // Criar o JSON com os nomes que o Script do Google Sheets vai ler
    StaticJsonDocument<200> doc;
    doc["temp"] = t;
    doc["humi"] = rh;
    doc["ah"] = ah;
    doc["vpd"] = vpd;

    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);

    if (httpResponseCode > 0) {
      Serial.println(">>>Planilha Atualizada com Sucesso!");

    } else {
      Serial.print(">>>Erro no envio: ");
      Serial.println(http.errorToString(httpResponseCode).c_str());
      //internetLastState = false;
    }
    http.end();
  }
}

void reconnectMQTT() {
  // Loop até conectar ao broker
  if (currentTime - lastTimeReconexaoMQTT > tempoReconexaoMQTT) {
    lastTimeReconexaoMQTT = currentTime;
    Serial.print(">>>Tentando conexão MQTT...");
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println(">>>Conectado ao Broker!");

    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente...");
    }
  }
}

void enviarMQTT() {


  if (currentTime - lastTimeEnvioMQTT > tempoEnvioMQTT) {

    lastTimeEnvioMQTT = currentTime;

    if (!client.connected()) {
      reconnectMQTT();

    } else {

      // Criar JSON para o MQTT
      StaticJsonDocument<512> doc;

      doc["t"] = t;
      doc["rh"] = rh;
      doc["ah"] = roundf(ah * 10) / 10;
      doc["vpd"] = roundf(vpd * 10) / 10;
      doc["b"] = obterDataHoraString();
      doc["SSID"] = WiFi.SSID();

      char buffer[256];
      serializeJson(doc, buffer);

      // Publicar no tópico
      if (client.publish(mqtt_topic, buffer, true)) {
        Serial.println(">>>MQTT enviado");

      } else {
        Serial.println(">>>Falha ao enviar MQTT");
      }
    }
  }
}

void loop() {
  currentTime = millis();
  termo();
  conexaoInternet();
  if (isDHTok == true) {  //Só atualiza planilha e mqtt se o sensor estiver funcionando pra não dar bo nos dados
    enviarPlanilha();
    enviarMQTT();
  }
  if (client.connected()) {  //Só roda client.loop se client estiver conectado. Loop precisa rodar sempre para manter o keep alive e para verificar recebimentos
    client.loop();
  }
}