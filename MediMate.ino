#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

LiquidCrystal_I2C LCD = LiquidCrystal_I2C(0x27, 16, 2);

#define NTP_SERVER     "a.ntp.br"
#define UTC_OFFSET     0
#define UTC_OFFSET_DST 0

const int melody[] = {123.47, 246.94, 185, 123.47, 92.50, 110, 123.47, 146.83, 123.47, 246.94}; // Frequências das notas (C, B, E, D, C, C)
const int noteDuration = 500;

const int buzzerPin = 12;
bool alarmTriggered = false;
unsigned long alarmStartTime;
const unsigned long alarmDuration = 6000; // Duração do alarme em milissegundos (6 segundos)
const String nomeRemedio = "Nome";

const char *fiwareServer = "http://46.17.108.113:1026/v2/entities/paciente_5";

DynamicJsonDocument doc(1024); // Adicionada a declaração aqui para tornar 'doc' global

void spinner() {
  static int8_t counter = 0;
  const char* glyphs = "\xa1\xa5\xdb";
  LCD.setCursor(15, 1);
  LCD.print(glyphs[counter++]);
  if (counter == strlen(glyphs)) {
    counter = 0;
  }
}

void playMelody() {
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    tone(buzzerPin, melody[i], noteDuration);
    delay(noteDuration + 50); // Adiciona um pequeno atraso entre as notas
  }
}

String obterInfoMedicamentos() {
  HTTPClient http;

  http.begin(fiwareServer);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    // Analisar a resposta JSON
    deserializeJson(doc, http.getString());

    if (doc.containsKey("medicamentos") && doc["medicamentos"].size() > 0) {
      String infoMedicamentos = "";
      JsonArray medicamentos = doc["medicamentos"]["value"];

      for (JsonObject medicamento : medicamentos) {
        String nomeMedicamento = medicamento["nome"];
        String horarioMedicamento = medicamento["horario"];
        infoMedicamentos += nomeMedicamento + ": " + horarioMedicamento + "\n";
      }

      return infoMedicamentos;
    } else {
      Serial.println("Nenhum medicamento encontrado para o paciente.");
      return "";
    }
  } else {
    Serial.print("Falha ao obter dados do FIWARE. Código de status: ");
    Serial.println(httpCode);

    String resposta = http.getString();
    Serial.print("Resposta do FIWARE:");
    Serial.println(resposta);
    return "";
  }

  http.end();
}

void printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    LCD.setCursor(0, 1);
    LCD.println("Connection Err");
    return;
  }

  // Ajustar o deslocamento horário para UTC -3 (AST/BRT)
  timeinfo.tm_hour -= 3;
  if (timeinfo.tm_hour < 0) {
    timeinfo.tm_hour += 24;  // Garantir que o valor de hora seja positivo
  }

  String infoMedicamentos = obterInfoMedicamentos();

  Serial.print("Informações dos Medicamentos:\n");
  Serial.println(infoMedicamentos);

  if (alarmTriggered) {
    unsigned long currentTime = millis();
    if (currentTime - alarmStartTime < alarmDuration) {
      playMelody();
      LCD.setCursor(0, 1);
      LCD.print("Tomar ");
      if (!infoMedicamentos.isEmpty()) {
        JsonArray medicamentos = doc["medicamentos"]["value"];
        for (JsonObject medicamento : medicamentos) {
          String horarioMedicamento = medicamento["horario"];
          if (horarioMedicamento == String(timeinfo.tm_hour < 10 ? "0" + String(timeinfo.tm_hour) : String(timeinfo.tm_hour)) + ":" + String(timeinfo.tm_min < 10 ? "0" + String(timeinfo.tm_min) : String(timeinfo.tm_min)) && medicamento["status"] == "pendente") {
            String nomeMedicamento = medicamento["nome"];
            String dosagemMedicamento = medicamento["dosagem"];
            
            // Alternar entre nome do medicamento e dosagem
            static bool showMedicineName = true;
            if (showMedicineName) {
              LCD.print(nomeMedicamento);
            } else {
              LCD.setCursor(0, 1);
              LCD.print(dosagemMedicamento);
            }
            
            // Alternar o sinalizador para a próxima iteração
            showMedicineName = !showMedicineName;
            break;
          }
        }
      }
    } else {
      alarmTriggered = false;
      alarmStartTime = 0;
      LCD.clear();
    }
  }

  if (!alarmTriggered) {
    LCD.setCursor(0, 0);
    LCD.println(&timeinfo, "%H:%M        ");

    LCD.setCursor(0, 1);
    LCD.println(&timeinfo, "%d/%m/%Y     ");

    if (!infoMedicamentos.isEmpty()) {
      JsonArray medicamentos = doc["medicamentos"]["value"];

      for (JsonObject medicamento : medicamentos) {
        String nomeMedicamento = medicamento["nome"];
        String horarioMedicamento = medicamento["horario"];

        if (horarioMedicamento == String(timeinfo.tm_hour < 10 ? "0" + String(timeinfo.tm_hour) : String(timeinfo.tm_hour)) + ":" + String(timeinfo.tm_min < 10 ? "0" + String(timeinfo.tm_min) : String(timeinfo.tm_min)) && medicamento["status"] == "pendente") {
          Serial.println("Alarme acionado para " + nomeMedicamento);
          alarmTriggered = true;
          alarmStartTime = millis();
          break; 
        }
      }
    }
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);

  LCD.init();
  LCD.backlight();
  LCD.setCursor(0, 0);
  LCD.print("Connecting to ");
  LCD.setCursor(0, 1);
  LCD.print("WiFi ");

  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    spinner();
  }
  
  LCD.clear();
  LCD.setCursor(0, 0);
  LCD.println("Online");
  LCD.setCursor(0, 1);
  LCD.println("Updating time...");

  configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

void loop() {
  printLocalTime();
  delay(250);
}
