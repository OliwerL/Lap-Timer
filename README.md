# ESP32 Lap Timer z BLE i Czujnikiem Ultrasonograficznym

Ten projekt wykorzystuje ESP32 z czujnikiem ultradźwiękowym i komunikacją Bluetooth Low Energy (BLE), aby mierzyć czasy okrążeń (lap timer) dla modelu samochodu lub innego obiektu przejeżdżającego przed czujnikiem.

## Wymagane komponenty
- ESP32 (np. ESP32-S3, WROOM, DEVKIT v1 itd.)
- Czujnik ultradźwiękowy (HC-SR04 lub inny, z pinami `trig` i `echo`)
- Zasilanie dla ESP32
- Aplikacja BLE (np. **nRF Connect** na Android/iOS) do komunikacji

## Schemat podłączenia
| Czujnik | ESP32 GPIO |
|--------:|-----------:|
| TRIG    | 4          |
| ECHO    | 16         |
| VCC     | 5V         |
| GND     | GND        |

## Jak to działa?

- Czujnik mierzy odległość co kilka milisekund.
- Gdy obiekt wjeżdża pod czujnik (np. samochodzik) i dystans spada poniżej **100 cm**, system uznaje to za początek lub zakończenie okrążenia.
- BLE pozwala na zdalne:
  - rozpoczęcie pomiarów (`reset`)
  - zatrzymanie (`stop`)
  - ping/pong testy (`ping`)
  - tryb testowy z nadpisywaniem ostatniego okrążenia (`test`)
- Gdy zostaną ukończone 4 okrążenia, BLE wysyła komunikat `end`.

## Komendy BLE

- `reset` – resetuje wszystkie czasy, uzbraja system
- `stop` – zatrzymuje pomiary
- `test` – uruchamia tryb nadpisywania ostatniego okrążenia
- `ping` – odpowiada `pong`
- Czasy są wysyłane co 500 ms w formacie CSV: `12.43,11.95,13.02,12.76`

## Uruchomienie
1. Wgraj plik `.ino` do ESP32 przez Arduino IDE.
2. Po uruchomieniu, ESP zacznie nadawać jako `ESP32-S3 LapTimer`.
3. Połącz się aplikacją BLE (np. **nRF Connect**) i wyślij komendę `reset`.
4. Obiekt przejeżdża pod czujnikiem — system liczy okrążenia.
5. Po 4 okrążeniach wysyłany jest komunikat `end`.

##  Uwaga
- Kod używa histerezy: dystans musi spaść **poniżej 100 cm**, a potem wzrosnąć **powyżej 120 cm**, żeby uznać zakończenie i rozpoczęcie kolejnego okrążenia.
- Jeśli BLE się rozłączy, reklama BLE zostanie wznowiona automatycznie po 0.5 sekundy.


