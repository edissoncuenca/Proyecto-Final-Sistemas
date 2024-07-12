#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

#define FAN_PIN 15 // Pin donde está conectado el ventilador
#define DHTPIN 8 // Pin digital donde está conectado el sensor
#define DHTTYPE DHT11 // Tipo de sensor (DHT11 o DHT22)
DHT dht(DHTPIN, DHTTYPE); // Inicializar el objeto del sensor

// Configuración de la red WiFi
const char* ssid = "pruebas";
const char* password = "12345678";

// Configuración de ThingsBoard
const char* thingsboardServer = "192.168.126.213"; 
const char* accessToken = "5MjYIr34aKE6jQxysXRQ";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

bool automaticControl = false; // Indica si el control automático está activo

// Función de conexión WiFi
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
  Serial.println("Dirección IP: ");
  Serial.println(WiFi.localIP());
}

// Función de reconexión MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect("ESP32Client", accessToken, NULL)) {
      Serial.println("conectado");
      // Suscribirse al topic de comandos para recibir comandos de ThingsBoard
      client.subscribe("v1/devices/me/rpc/request/+");
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentamos de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

// Función para manejar mensajes MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Parsear el payload
  payload[length] = '\0';
  String message = String((char*)payload);

  // Debug: Imprimir el mensaje completo recibido
  Serial.println("Mensaje completo recibido:");
  Serial.println(message);

  // Comprobar si el mensaje es para el control del ventilador
  if (message.indexOf("params") > 0) {
    if (message.indexOf("true") > 0) {
      digitalWrite(FAN_PIN, LOW); // Encender el ventilador
      Serial.println("Ventilador encendido manualmente");
      automaticControl = false; // Desactivar control automático
    } else if (message.indexOf("false") > 0) {
      digitalWrite(FAN_PIN, HIGH); // Apagar el ventilador
      Serial.println("Ventilador apagado manualmente");
      automaticControl = false; // Desactivar control automático
    } else {
      Serial.println("Comando no reconocido para el control del ventilador");
    }
  } else {
    Serial.println("Mensaje no contiene parámetros para el control del ventilador");
  }
}

// Función para controlar el ventilador automáticamente
void controlFan(float temperatura) {
  if (temperatura >= 25.0) {
    digitalWrite(FAN_PIN, LOW); // Encender el ventilador
    Serial.println("Ventilador encendido por control automático");
    automaticControl = true; // Activar control automático
  } else if (temperatura <= 23.5 && automaticControl) {
    digitalWrite(FAN_PIN, HIGH); // Apagar el ventilador
    Serial.println("Ventilador apagado por control automático");
    automaticControl = false; // Desactivar control automático
  }
}

void setup() {
  Serial.begin(9600); // Iniciar comunicación serial
  setup_wifi();
  client.setServer(thingsboardServer, 1883);
  client.setCallback(callback);
  dht.begin(); // Iniciar el sensor DHT
  pinMode(FAN_PIN, OUTPUT); // Configurar el pin del ventilador como salida
  digitalWrite(FAN_PIN, HIGH); // Asegurarse de que el ventilador esté apagado inicialmente
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(2000); // Esperar 2 segundos entre lecturas
  float humedad = dht.readHumidity(); // Leer la humedad relativa del sensor
  float temperatura = dht.readTemperature(); // Leer la temperatura relativa del sensor

  // Comprobar si alguna lectura ha fallado y salir temprano (para intentarlo de nuevo)
  if (isnan(humedad) || isnan(temperatura)) {
    Serial.println("Fallo al leer del sensor DHT!");
    return;
  }

  Serial.print("Temperatura: ");
  Serial.print(temperatura); // Imprimir los valores de temperatura en el monitor serial
  Serial.print(" °C ");
  Serial.print("Humedad: ");
  Serial.print(humedad); // Imprimir los valores de humedad en el monitor serial
  Serial.println(" %HR");

  // Enviar datos a ThingsBoard
  String payload = "{\"temperature\":" + String(temperatura) + ", \"humidity\":" + String(humedad) + "}";
  client.publish("v1/devices/me/telemetry", payload.c_str());

  // Controlar el ventilador automáticamente basado en la temperatura
  if (!automaticControl) {
    controlFan(temperatura);
  }
}

