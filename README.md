# Stacja pogodowa — uruchomienie w VS Code

## 1. Zainstaluj narzędzia

1. Zainstaluj [Visual Studio Code](https://code.visualstudio.com/).
2. Zainstaluj [Arduino CLI](https://arduino.github.io/arduino-cli/latest/installation/).
3. W VS Code zainstaluj rozszerzenie **Wokwi for VS Code**.
4. Naciśnij `F1`, uruchom `Wokwi: Request a new License` i dokończ autoryzację w przeglądarce.

Nie trzeba instalować rozszerzenia Arduino do kompilacji — używany jest Arduino CLI.

## 2. Przygotuj Arduino CLI

W terminalu wykonaj:

```powershell
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit SSD1306" "DHT sensor library" "Adafruit Unified Sensor" "Adafruit BMP085 Library"
```

## 3. Zbuduj i uruchom symulator

Otwórz w VS Code folder projektu zawierający `Pogodynka.ino`, `diagram.json` i `wokwi.toml`. W terminalu, w tym folderze, zbuduj firmware:

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build .
```

Następnie naciśnij `F1` i wybierz **Wokwi: Start Simulator**. Monitor portu szeregowego jest dostępny w panelu symulatora.

## 4. Otwórz stronę stacji

Przy uruchomionej symulacji otwórz:

- Strona: <http://127.0.0.1:9080/>
- Dane API: <http://127.0.0.1:9080/api>

Przekierowanie portu 9080 jest skonfigurowane w `wokwi.toml`; nie uruchamiaj osobno `wokwigw.exe`.
