# esp-idf-web-gpio
GPIO control using web browser.

![web-gpio-1](https://user-images.githubusercontent.com/6020549/124352292-84d99d00-dc3a-11eb-8f8a-45472e45eebe.jpg)

You can change gpio direction and gpio value using built-in web server.   

![web-gpio-2](https://user-images.githubusercontent.com/6020549/124352331-a2a70200-dc3a-11eb-9161-4053ef0315f5.jpg)
![web-gpio-3](https://user-images.githubusercontent.com/6020549/124352333-a3d82f00-dc3a-11eb-879f-2fb976e43646.jpg)

# Software requirements
esp-idf v4.4 or later.   
The mDNS strict mode [issue](https://github.com/espressif/esp-idf/issues/6190) has been resolved.   

# Installation for ESP32

```
git clone https://github.com/nopnop2002/esp-idf-web-gpio
cd esp-idf-web-gpio/
idf.py set-target esp32
idf.py menuconfig
idf.py flash
```

# Installation for ESP32-S2

```
git clone https://github.com/nopnop2002/esp-idf-web-gpio
cd esp-idf-web-gpio/
idf.py set-target esp32s2
idf.py menuconfig
idf.py flash
```

# Configuration   
You have to set this config value with menuconfig.   

![config-main](https://user-images.githubusercontent.com/6020549/124352357-c1a59400-dc3a-11eb-9bde-6a3fc43ef755.jpg)
![config-app](https://user-images.githubusercontent.com/6020549/124352358-c36f5780-dc3a-11eb-875b-88899585923f.jpg)

You can use static ip.   
![config-static](https://user-images.githubusercontent.com/6020549/124352360-c5d1b180-dc3a-11eb-9cdb-82162f31cf14.jpg)

You can connect using mDNS host name.
![config-mDNS](https://user-images.githubusercontent.com/6020549/124352362-c79b7500-dc3a-11eb-85be-199e1ea3bae6.jpg)

![web-gpio-mdns](https://user-images.githubusercontent.com/6020549/134443304-64e76a3a-beec-4072-ab68-131d9bec01fd.jpg)


# Definition GPIO   
The GPIO pin number to control is defined in csv/gpio.csv.   
The file gpio.csv has three columns.   
In the first column you need to specify the GPIO number.   
On the ESP32, GPIO up to 39.   
On the ESP32-S2, GPIO up to 53.   
This project don't care about the maximum number of GPIO.   
This project don't care if the number is valid. You need to define it carefully.   
   
In the second column you have to specify the GPIO Initial direction.   
The GPIO direction is either I(for INPUT) or O(for OUTPUT).   
On the ESP32, GPIOs 35-39 are input-only so cannot be used as outputs.   
On the ESP32-S2, GPIO 46 is input-only so cannot be used as outputs.   
This project don't care if GPIO is input-only. You need to define it carefully.   
   
In the last column you have to specify the GPIO Initial value for OUTPUT.   
INPUT pin don't care.   

```
12,O,0
13,O,1
14,I,0
15,I,0
```

The simplest circuit connects GPIO12 and GPIO14, and GPIO13 and GPIO15.   
If you change the value of GPIO12, the value of GPIO14 will change.   
If you change the value of GPIO13, the value of GPIO15 will change.   

# Using RESTful API   
|API|Method|Resource Example|Description|
|:-:|:-:|:-:|:-:|
|/api/gpio/info|GET||Used for clients to get gpio information|
|/api/gpio/mode|POST|{"gpio":12, "mode":"INPUT"}<br>{"gpio":12, "mode":"OUTPUT"}|Used for clients to set gpio direction|
|/api/gpio/value|POST|{"gpio":12, "value":0}<br>{"gpio":12, "value":1}|Used for clients to set gpio value|

# Using RESTful API with curl command   

## /api/gpio/info   

```
$ curl 'http://esp32-server.local:8080/api/gpio/info' | python -m json.tool
  % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                 Dload  Upload   Total   Spent    Left  Speed
100   254  100   254    0     0   1881      0 --:--:-- --:--:-- --:--:--  1881
[
    {
        "id": 0,
        "gpio": 12,
        "mode": "OUTPUT",
        "value": 0
    },
    {
        "id": 1,
        "gpio": 13,
        "mode": "OUTPUT",
        "value": 1
    },
    {
        "id": 2,
        "gpio": 14,
        "mode": "INPUT",
        "value": 0
    },
    {
        "id": 3,
        "gpio": 15,
        "mode": "INPUT",
        "value": 1
    }
]
```

## /api/gpio/mode   
```
$ curl -X POST -H "Content-Type: application/json" -d '{"gpio":12, "mode":"INPUT"}' http://esp32-server.local:8080/api/gpio/mode
GPIO mode set successfully

$ curl -X POST -H "Content-Type: application/json" -d '{"gpio":12, "mode":"OUTPUT"}' http://esp32-server.local:8080/api/gpio/mode
GPIO mode set successfully

```

## /api/gpio/value   
```
$ curl -X POST -H "Content-Type: application/json" -d '{"gpio":12, "value":1}' http://esp32-server.local:8080/api/gpio/value
GPIO value set successfully

$ curl -X POST -H "Content-Type: application/json" -d '{"gpio":12, "value":0}' http://esp32-server.local:8080/api/gpio/value
GPIO value set successfully
```

# How to browse image data using built-in http server   
Even if there are files in SPIFFS, the esp-idf http server does not support this:   
```
httpd_resp_sendstr_chunk(req, "<img src=\"/spiffs/picture.jpg\">");
```

You need to convert the image file to base64 string.   
```
httpd_resp_sendstr_chunk(req, "<img src=\"data:image/jpeg;base64,");
httpd_resp_sendstr_chunk(req, (char *)BASE64_ENCODE_STRING);
httpd_resp_sendstr_chunk(req, "\">");
```

Images in base64 format are stored in the icons folder.   
I converted using the base64 command.   
```
$ base64 png/box-in-icon.png > icons/box-in-icon.txt
```

