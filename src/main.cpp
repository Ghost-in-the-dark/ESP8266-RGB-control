#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266SSDP.h>
#include <WiFiManager.h>   //https://github.com/tzapu/WiFiManager 
#include <DNSServer.h>


#define debug false
// храним % позицию курсора с клиента, потому что высчитать позицию по rgb каналу очень сложно
int position_x = 50, position_y = 99;

ESP8266WebServer webServer(80);

//Webpage html Code
String webpage = ""
"<!DOCTYPE html><html><head><title>RGB control</title><meta name='mobile-web-app-capable' content='yes' />\n"
"<meta name='viewport' content='width=device-width' /></head><body style='margin: 0px; padding: 0px;'>\n"
""
"</body>\n"
"<canvas id='colorspace'></canvas>\n"
"<script type='text/javascript'>\n"
"(function () {\n"
"  var canvas = document.getElementById('colorspace');\n"
"  var ctx = canvas.getContext('2d');\n"
"  var x = 0, y = 0;\n"
"  function drawCanvas() {\n"
"       var colours = ctx.createLinearGradient(0, 0, window.innerWidth, 0);\n"
"       for(var i=0; i <= 360; i+=10) {\n"
"           colours.addColorStop(i/360, 'hsl(' + i + ', 100%, 50%)');\n"
"       }\n"
"       ctx.shadowBlur = 0;\n"
"       ctx.fillStyle = colours;\n"
"       ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);\n"
"       var luminance = ctx.createLinearGradient(0, 0, 0, ctx.canvas.height);\n"
"       luminance.addColorStop(0, '#ffffff');\n"
"       luminance.addColorStop(0.05, '#ffffff');\n"
"       luminance.addColorStop(0.5, 'rgba(0,0,0,0)');\n"
"       luminance.addColorStop(0.95, '#000000');\n"
"       luminance.addColorStop(1, '#000000');\n"
"       ctx.fillStyle = luminance;\n"
"       ctx.fillRect(0, 0, ctx.canvas.width, ctx.canvas.height);\n"
"   }\n"
"   function drawPoint(x, y) {\n"
"       ctx.fillStyle = 'black';\n"
"       ctx.shadowBlur = 6;\n"
"       ctx.shadowColor = 'white';\n"
"       ctx.beginPath();\n"
"       ctx.arc(x,y,8,0,2*Math.PI);\n"
"       ctx.fill();\n"
"   }\n"
"   var eventLocked = false;\n"
"   function request(method, params) {\n"
"       var req = new XMLHttpRequest();\n"
"       params = params ? '?' + params : '';\n"
"       req.open(method, 'config/' + params, true);\n"
"       req.send();\n"
"       eventLocked = true;\n"
"       req.onreadystatechange = function() {\n"
"           if(req.readyState === 4) {\n"
"               eventLocked = false;\n"
"               console.log(req.responseText);\n"
"               var jr = JSON.parse(req.responseText);\n"
"               x = Math.round((jr.x/100)*canvas.width);\n"
"               y = Math.round((jr.y/100)*canvas.height);\n"
"               drawPoint(x, y);\n"
"           }\n"
"       };\n"
"   }\n"
"   function handleEvent(clientX, clientY) {\n"
"       if(eventLocked) {\n"
"           return;\n"
"       }\n"
"       function colourCorrect(v) {\n"
"           return Math.round(1023-(v*v)/64);\n"
"       }\n"
"       var data = ctx.getImageData(clientX, clientY, 1, 1).data;\n"
"       var params = [\n"
"               'r=' + colourCorrect(data[0]),\n"
"               'g=' + colourCorrect(data[1]),\n"
"               'b=' + colourCorrect(data[2]),\n"
"               'x=' + Math.round((clientX/canvas.width)*100),\n"
"               'y=' + Math.round((clientY/canvas.height)*100)\n"
"       ].join('&');\n"
"       request('POST', params);\n"
"   }\n"
"   canvas.addEventListener('click', function(event) {\n"
"       drawCanvas();\n"
"       handleEvent(event.clientX, event.clientY, true);\n"
"   }, false);\n"
"   canvas.addEventListener('touchmove', function(event){\n"
"       drawCanvas();\n"
"       handleEvent(event.touches[0].clientX, event.touches[0].clientY);\n"
"   }, false);\n"
"   function resizeCanvas() {\n"
"       canvas.width = window.innerWidth;\n"
"       canvas.height = window.innerHeight;\n"
"   }\n"
"   window.addEventListener('resize', resizeCanvas, false);\n"
"   request('GET', '');\n"
"   resizeCanvas();\n"
"   drawCanvas();\n"
"   document.ontouchmove = function(e) {e.preventDefault()};\n"
"})();\n"
"</script></html>";

int validateArg(const String& arg, const int max = 1023) {
    // Валидируем аргументы
    int i_arg = arg.toInt();
    if (i_arg < 0 or i_arg > max) {
        i_arg = 0;
    }
    return i_arg;
}

void wifimanstart() { // Волшебная процедура начального подключения к Wifi.
                      // Если не знает к чему подцепить - создает точку доступа ESP8266 и настроечную таблицу http://192.168.4.1
                      // Подробнее: https://github.com/tzapu/WiFiManager
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(debug);
  //wifiManager.resetSettings();            // сброс настроек
  wifiManager.setMinimumSignalQuality();
  if (!wifiManager.autoConnect("ESP8266 RGB")) {
  if (debug) Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000); }
if (debug) Serial.println("connected...");
}

void handleRGBConfig() {
    int red, green, blue;

    if (webServer.method() == HTTP_POST) {

        // Сохраняем позицую указателя с клиента
        position_x = validateArg(webServer.arg("x"), 100);
        position_y = validateArg(webServer.arg("y"), 100);

        // Получаем параметры цвета
        red = validateArg(webServer.arg("r"));
        green = validateArg(webServer.arg("g"));
        blue = validateArg(webServer.arg("b"));

        // Пишем параметры
        analogWrite(0, 1023 - red);
        analogWrite(2, 1023 - green);
        analogWrite(3, 1023 - blue);

    }

    // упоролся в щи
    // {"x": 0, "y": 0}
    String resp = "";
    resp.concat("{\"x\": ");
    resp.concat(position_x);
    resp.concat(",\"y\": ");
    resp.concat(position_y);
    resp.concat("}");

    // Отправляем параметры x и y которые прошли валидацию на клиент для отрисовки
    webServer.send(200, "application/json", resp);
}

void handleRoot() {
    webServer.send(200, "text/html", webpage);
}

void setup() {
    pinMode(0, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    wifimanstart();
    delay(1000);
    //WiFi.mode(WIFI_STA);
    webServer.on("/",  handleRoot);
    webServer.on("/config/",  handleRGBConfig);
    webServer.begin();
    
}

void loop() {
    webServer.handleClient();
   }
