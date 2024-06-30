#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

#define DHTPIN 27
#define DHTTYPE DHT22
#define RELAY_PIN 25

DHT dht(DHTPIN, DHTTYPE);

//parametros wifi
const char* ssid = "";
const char* password = "";

//parametros mqtt
const char* mqtt_server = "maqiatto.com";
const int mqtt_port = 1883;
const char* mqtt_user = "sara.silva.114@ufrn.edu.br";
const char* mqtt_password = "";
const char* mqtt_topic_temp = "sara.silva.114@ufrn.edu.br/temp";
const char* mqtt_topic_umidade = "sara.silva.114@ufrn.edu.br/umidade";
const char* mqtt_topic_relay = "sara.silva.114@ufrn.edu.br/relay";

WiFiClient espClient; 
PubSubClient client(espClient);

unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
unsigned long lastRelayActivation = 0;
bool relayState = LOW;

//conectar ao wifi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

//Callback MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == mqtt_topic_relay) {
    if (message == "ON") {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = HIGH;
      lastRelayActivation = millis();
      Serial.println("Relé ligado!");
    } else if (message == "OFF") {
      digitalWrite(RELAY_PIN, LOW);
      relayState = LOW;
      Serial.println("Relé desligado!");
    }
  }
}

//Reconectar ao MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("conectado");

      // Inscreva-se nos tópicos MQTT
      client.subscribe(mqtt_topic_relay);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println("; tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  dht.begin();
  delay(10);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    float humidity = dht.readHumidity();
    float temp = dht.readTemperature();

    if (isnan(humidity) || isnan(temp)) {
      Serial.println("Falha ao ler do sensor DHT!");
      return;
    }
  // Publicar a umidade
    snprintf (msg, MSG_BUFFER_SIZE, "%.2f", humidity);
    Serial.print("Publicando umidade: ");
    Serial.println(msg);
    client.publish(mqtt_topic_umidade, msg);

    // Publicar a temperatura
    snprintf (msg, MSG_BUFFER_SIZE, "%.2f", temp);
    Serial.print("Publicando temperatura: ");
    Serial.println(msg);
    client.publish(mqtt_topic_temp, msg);

    // Controlar o relé com base na temperatura e umidade
    if (temp > 25 || humidity < 30) {
      digitalWrite(RELAY_PIN, HIGH);
      relayState = HIGH;
      lastRelayActivation = millis();
      Serial.println("Relé acionado!");
      delay(1000);
    } else if (relayState && (now - lastRelayActivation > 60000)) {
      digitalWrite(RELAY_PIN, LOW);
      relayState = LOW;
      Serial.println("Relé desligado após 1 minuto!");
    }
  }
}