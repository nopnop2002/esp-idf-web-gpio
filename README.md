# esp-idf-web-gpio
GPIO control using WEB.

![web-gpio-1](https://user-images.githubusercontent.com/6020549/124352292-84d99d00-dc3a-11eb-8f8a-45472e45eebe.jpg)

You can change gpio direction and gpio value using built-in web server.   

![web-gpio-2](https://user-images.githubusercontent.com/6020549/124352331-a2a70200-dc3a-11eb-9161-4053ef0315f5.jpg)
![web-gpio-3](https://user-images.githubusercontent.com/6020549/124352333-a3d82f00-dc3a-11eb-879f-2fb976e43646.jpg)

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
![web-gpio-11](https://user-images.githubusercontent.com/6020549/124352392-ee59ab80-dc3a-11eb-9a71-fe199db0476d.jpg)


# Definition GPIO   
GPIO pin numbers are defined in csv/gpio.csv.   
The file gpio.csv has three columns.   
In the first column you need to specify the GPIO number.   
On the ESP32, GPIO up to 39.   
On the ESP32-S2, GPIO up to 53.   
This project don't care about the maximum number of GPIO. You need to define it carefully.   
   
In the second column you have to specify the GPIO direction.   
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

# How to browse image data
http server of esp-idf does not support this:   
```
httpd_resp_sendstr_chunk(req, "<img src=\"/spiffs/picture.jpg\" width=\"128\" height=\"128\">");
```

You need to convert the image file to base64.   
```
httpd_resp_sendstr_chunk(req, "<img src=\"data:image/png;base64,BASE64_ENCODE_STRING\" width=\"128\" height=\"128\">");
```

Images in base64 format are stored in the icons folder.

