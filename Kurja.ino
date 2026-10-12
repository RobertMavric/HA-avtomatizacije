#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <RtcDS3231.h>
#include <EEPROM.h>

// Nastavitve Wi-Fi točke
const char* ssid = "kurnk";
const char* password = "kokokoko";

// Nastavitev strojne opreme
const byte DNS_PORT = 53;
const int PIN_VRATA = 2;  // Rele vrat na D4 (GPIO2)
const int PIN_LUC = 14;   // Rele luči na D5 (GPIO14)

DNSServer dnsServer;
ESP8266WebServer server(80);
RtcDS3231<TwoWire> Rtc(Wire);

// Spremenljivke za ročni preklop (override)
bool rocniNacinVrata = false;
bool rocnoStanjeVrata = false;
bool rocniNacinLuc = false;
bool rocnoStanjeLuc = false;

// Zadnje stanje urnika (za zaznavanje prehoda/spremembe urnika)
bool zadnjeStanjeUrnikaVrata = false;
bool zadnjeStanjeUrnikaLuc = false;

// Struktura za shranjevanje nastavitev v EEPROM
struct Nastavitve {
    int vrataVklopUra;
    int vrataVklopMin;
    int vrataIzklopUra;
    int vrataIzklopMin;
    int lucVklopUra;
    int lucVklopMin;
    int lucIzklopUra;
    int lucIzklopMin;
};

Nastavitve nastavitve;

void naloziNastavitve() {
    EEPROM.get(0, nastavitve);
    if (nastavitve.vrataVklopUra < 0 || nastavitve.vrataVklopUra > 23) {
        nastavitve.vrataVklopUra = 7;   nastavitve.vrataVklopMin = 0;
        nastavitve.vrataIzklopUra = 20; nastavitve.vrataIzklopMin = 0;
        nastavitve.lucVklopUra = 6;     nastavitve.lucVklopMin = 0;
        nastavitve.lucIzklopUra = 8;    nastavitve.lucIzklopMin = 0;
    }
}

void shraniNastavitve() {
    EEPROM.put(0, nastavitve);
    EEPROM.commit();
}

void handleRoot() {
    RtcDateTime now = Rtc.GetDateTime();
    
    char casStr[10];
    char datumStr[15];
    sprintf(casStr, "%02d:%02d", now.Hour(), now.Minute());
    sprintf(datumStr, "%02d.%02d.%04d", now.Day(), now.Month(), now.Year());

    char vvStr[10], viStr[10], lvStr[10], liStr[10];
    sprintf(vvStr, "%02d:%02d", nastavitve.vrataVklopUra, nastavitve.vrataVklopMin);
    sprintf(viStr, "%02d:%02d", nastavitve.vrataIzklopUra, nastavitve.vrataIzklopMin);
    sprintf(lvStr, "%02d:%02d", nastavitve.lucVklopUra, nastavitve.lucVklopMin);
    sprintf(liStr, "%02d:%02d", nastavitve.lucIzklopUra, nastavitve.lucIzklopMin);

    // Določanje barv in napisov glede na trenutno fizično stanje izhodov
    String stVrat = (digitalRead(PIN_VRATA) == HIGH) ? "ODPRTA" : "ZAPRTA";
    String barvaVrat = (digitalRead(PIN_VRATA) == HIGH) ? "#28a745" : "#dc3545";
    
    String stLuci = (digitalRead(PIN_LUC) == HIGH) ? "PRIŽGANA" : "UGAŠENA";
    String barvaLuci = (digitalRead(PIN_LUC) == HIGH) ? "#28a745" : "#dc3545";

    // Oznaka, če je aktiviran ročni način
    String infoVrata = rocniNacinVrata ? " <small style='color:#ffc107;'>(ROČNO)</small>" : "";
    String infoLuc = rocniNacinLuc ? " <small style='color:#ffc107;'>(ROČNO)</small>" : "";

    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    // Avtomatsko osveževanje spletne strani na vsakih 5 sekund
    html += "<meta http-equiv='refresh' content='5'>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; padding: 10px; margin: 0; color: #333; }";
    html += ".container { max-width: 420px; margin: auto; background: white; padding: 20px; border-radius: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }";
    html += ".trenutni-cas { font-size: 4rem; font-weight: bold; color: #007bff; margin: 10px 0 0 0; }";
    html += ".trenutni-datum { font-size: 1.6rem; color: #666; margin-bottom: 20px; }";
    html += ".btn-stanje { font-size: 1.6rem; font-weight: bold; width: 100%; padding: 18px; margin-bottom: 12px; border: none; border-radius: 10px; color: white; cursor: pointer; text-align: center; display: block; box-shadow: 0 3px 6px rgba(0,0,0,0.1); }";
    html += "h2 { font-size: 1.8rem; color: #111; border-bottom: 2px solid #007bff; padding-bottom: 5px; margin-top: 30px; }";
    html += "h3 { font-size: 1.3rem; margin: 15px 0 5px 0; color: #444; }";
    html += "input[type=time], input[type=date] { font-size: 1.8rem; padding: 12px; width: 85%; text-align: center; border-radius: 8px; border: 2px solid #ccc; margin-bottom: 5px; }";
    html += ".btn-shrani { font-size: 1.5rem; width: 90%; padding: 15px; background-color: #007bff; color: white; border: none; border-radius: 10px; font-weight: bold; cursor: pointer; margin-top: 10px; }";
    html += ".btn-rumen { background-color: #ffc107; color: black; font-size: 1.5rem; width: 90%; padding: 15px; border: none; border-radius: 10px; font-weight: bold; cursor: pointer; margin-top: 10px; }";
    html += "button:active { opacity: 0.8; }";
    html += "</style></head><body>";
    
    html += "<div class='container'>";
    html += "<div class='trenutni-cas'>" + String(casStr) + "</div>";
    html += "<div class='trenutni-datum'>" + String(datumStr) + "</div>";
    
    // Gumbi za ročni preklop izhodov
    html += "<button class='btn-stanje' style='background-color:" + barvaVrat + ";' onclick=\"location.href='/preklop-vrata'\">🚪 Vrata: " + stVrat + infoVrata + "</button>";
    html += "<button class='btn-stanje' style='background-color:" + barvaLuci + ";' onclick=\"location.href='/preklop-luc'\">💡 Luč: " + stLuci + infoLuc + "</button>";
    
    // URNIK ZA VRATA
    html += "<form action='/shrani-vrata' method='POST'>";
    html += "<h2>Nastavitev Vrat</h2>";
    html += "<h3>Ura odpiranja:</h3><input type='time' name='vv' value='" + String(vvStr) + "'>";
    html += "<h3>Ura zapiranja:</h3><input type='time' name='vi' value='" + String(viStr) + "'>";
    html += "<button type='submit' class='btn-shrani'>SHRANI URNIK VRAT</button>";
    html += "</form>";

    // URNIK ZA LUČ
    html += "<form action='/shrani-luc' method='POST'>";
    html += "<h2>Nastavitev Luči</h2>";
    html += "<h3>Ura vklopa:</h3><input type='time' name='lv' value='" + String(lvStr) + "'>";
    html += "<h3>Ura izklopa:</h3><input type='time' name='li' value='" + String(liStr) + "'>";
    html += "<button type='submit' class='btn-shrani'>SHRANI URNIK LUČI</button>";
    html += "</form>";

    // ROČNA NASTAVITEV URE
    html += "<form action='/nastavi-cas' method='POST'>";
    html += "<h2>Nastavitev Ure Naprave</h2>";
    html += "<h3>Nastavi datum:</h3><input type='date' name='n_dat'>";
    html += "<h3>Nastavi čas:</h3><input type='time' name='n_cas'>";
    html += "<button type='submit' class='btn-rumen'>POSODOBI TRENUTNI ČAS</button>";
    html += "</form>";
    
    html += "</div></body></html>";
    
    server.send(200, "text/html", html);
}

// Prožilci za ročni preklop ob kliku na gumb stanja
void handlePreklopVrata() {
    rocniNacinVrata = true;
    rocnoStanjeVrata = !digitalRead(PIN_VRATA); // Obrnemo trenutno stanje izhoda
    digitalWrite(PIN_VRATA, rocnoStanjeVrata);
    server.sendHeader("Location", "/", true);
    server.send(303);
}

void handlePreklopLuc() {
    rocniNacinLuc = true;
    rocnoStanjeLuc = !digitalRead(PIN_LUC); // Obrnemo trenutno stanje izhoda
    digitalWrite(PIN_LUC, rocnoStanjeLuc);
    server.sendHeader("Location", "/", true);
    server.send(303);
}

void handleShraniVrata() {
    if (server.hasArg("vv") && server.hasArg("vi")) {
        String vv = server.arg("vv");
        String vi = server.arg("vi");
        nastavitve.vrataVklopUra = vv.substring(0, 2).toInt();
        nastavitve.vrataVklopMin = vv.substring(3, 5).toInt();
        nastavitve.vrataIzklopUra = vi.substring(0, 2).toInt();
        nastavitve.vrataIzklopMin = vi.substring(3, 5).toInt();
        shraniNastavitve();
        rocniNacinVrata = false; // Ob shranjevanju urnika ponastavimo ročni način
    }
    server.sendHeader("Location", "/", true);
    server.send(303);
}

void handleShraniLuc() {
    if (server.hasArg("lv") && server.hasArg("li")) {
        String lv = server.arg("lv");
        String li = server.arg("li");
        nastavitve.lucVklopUra = lv.substring(0, 2).toInt();
        nastavitve.lucVklopMin = lv.substring(3, 5).toInt();
        nastavitve.lucIzklopUra = li.substring(0, 2).toInt();
        nastavitve.lucIzklopMin = li.substring(3, 5).toInt();
        shraniNastavitve();
        rocniNacinLuc = false; // Ob shranjevanju urnika ponastavimo ročni način
    }
    server.sendHeader("Location", "/", true);
    server.send(303);
}

void handleNastaviCas() {
    if (server.hasArg("n_dat") && server.hasArg("n_cas")) {
        String d = server.arg("n_dat");
        String u = server.arg("n_cas");
        if (d.length() == 10 && u.length() == 5) {
            int leto = d.substring(0, 4).toInt();
            int mesec = d.substring(5, 7).toInt();
            int dan = d.substring(8, 10).toInt();
            int ura = u.substring(0, 2).toInt();
            int minuta = u.substring(3, 5).toInt();
            
            RtcDateTime novaKonfiguracija(leto, mesec, dan, ura, minuta, 0);
            Rtc.SetDateTime(novaKonfiguracija);
        }
    }
    server.sendHeader("Location", "/", true);
    server.send(303);
}

bool preveriUrnik(int trMin, int vkUra, int vkMin, int ixUra, int ixMin) {
    int minVklop = (vkUra * 60) + vkMin;
    int minIzklop = (ixUra * 60) + ixMin;
    if (minVklop < minIzklop) {
        return (trMin >= minVklop && trMin < minIzklop);
    } else {
        return (trMin >= minVklop || trMin < minIzklop);
    }
}

void setup() {
    EEPROM.begin(sizeof(Nastavitve));
    naloziNastavitve();

    pinMode(PIN_VRATA, OUTPUT);
    pinMode(PIN_LUC, OUTPUT);
    digitalWrite(PIN_VRATA, LOW);
    digitalWrite(PIN_LUC, LOW);

    // Fiksna in varna I2C konfiguracija za vaš Wemos
    // D7 (GPIO 13) = SDA
    // D6 (GPIO 12) = SCL
    Wire.begin(13, 12); 
    Rtc.Begin();

    if (!Rtc.GetIsRunning()) {
        Rtc.SetIsRunning(true);
    }
    
    Rtc.Enable32kHzPin(false);
    IPAddress lokalni_IP(10, 10, 10, 10);
    IPAddress prehod_Gateway(10, 10, 10, 10);
    IPAddress maska_Subnet(255, 255, 255, 0);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(lokalni_IP, prehod_Gateway, maska_Subnet);
    WiFi.softAP(ssid, password);

    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/preklop-vrata", handlePreklopVrata);
    server.on("/preklop-luc", handlePreklopLuc);
    server.on("/shrani-vrata", HTTP_POST, handleShraniVrata);
    server.on("/shrani-luc", HTTP_POST, handleShraniLuc);
    server.on("/nastavi-cas", HTTP_POST, handleNastaviCas);
    server.onNotFound(handleRoot);
    server.begin();
}

void loop() {
    dnsServer.processNextRequest();
    server.handleClient();

    // Osveževanje izhodov vsakih 5 sekund
    static unsigned long prejsnjiCas = 0;
    if (millis() - prejsnjiCas > 5000) {
        prejsnjiCas = millis();
        
        RtcDateTime now = Rtc.GetDateTime();
        if (now.IsValid()) {
            int trenutneMinute = (now.Hour() * 60) + now.Minute();

  // 1. LOGIKA ZA VRATA
    bool trenutnoUrnikVrata = preveriUrnik(trenutneMinute, nastavitve.vrataVklopUra, nastavitve.vrataVklopMin, nastavitve.vrataIzklopUra, nastavitve.vrataIzklopMin);
            // Če je urnik spremenil svoje stanje (prehod iz vklopa v izklop ali obratno), prekličemo ročni način!
           if (trenutnoUrnikVrata != zadnjeStanjeUrnikaVrata) {
              rocniNacinVrata = false;
            zadnjeStanjeUrnikaVrata = trenutnoUrnikVrata;
            }
            if (!rocniNacinVrata) {
                digitalWrite(PIN_VRATA, trenutnoUrnikVrata ? HIGH : LOW);
            } 

            
 // 2. LOGIKA ZA LUČ
        bool trenutnoUrnikLuc = preveriUrnik(trenutneMinute, nastavitve.lucVklopUra, nastavitve.lucVklopMin, nastavitve.lucIzklopUra, nastavitve.lucIzklopMin);
         if (trenutnoUrnikLuc != zadnjeStanjeUrnikaLuc) {
            rocniNacinLuc = false;
        zadnjeStanjeUrnikaLuc = trenutnoUrnikLuc;
        }
            if (!rocniNacinLuc) {
              digitalWrite(PIN_LUC, trenutnoUrnikLuc ? HIGH : LOW);
            }
        }
    }
  }
 }
}
