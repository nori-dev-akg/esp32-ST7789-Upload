#include <WiFi.h>
#include <JPEGDecoder.h>
#include <Adafruit_GFX.h>   // Core graphics library by Adafruit
#include <Arduino_ST7789.h> // Hardware-specific library for ST7789 (with or without CS pin)
#include <SPI.h>

#define TFT_DC 17   // ?
#define TFT_RST 16  // LCDのリセット用（SPI関係ない）
#define TFT_CS -1   // blank SS
#define TFT_MOSI 23 // SDA for hardware SPI data pin (all of available pins)
#define TFT_SCLK 18 // SCL for hardware SPI sclk pin (all of available pins)

const char AP_DEMO_HTTP_200_IMAGE[] = "HTTP/1.1 200 OK\r\nPragma: public\r\nCache-Control: max-age=1\r\nExpires: Thu, 26 Dec 2016 23:59:59 GMT\r\nContent-Type: image/";

typedef enum
{
    UPL_AP_STAT_MAIN = 1,           // GET /
    UPL_AP_STAT_LED_HIGH,           // GET /H
    UPL_AP_STAT_LED_LOW,            // GET /L
    UPL_AP_STAT_GET_IMAGE,          // GET /logo.bmp
    UPL_AP_STAT_GET_FAVICON,        // GET /favicon.ico
    UPL_AP_STAT_POST_UPLOAD,        // POST /upload
    UPL_AP_STAT_POST_START_BOUNDRY, // POST /upload boundry
    UPL_AP_STAT_POST_GET_BOUNDRY,   // POST /upload boundry
    UPL_AP_STAT_POST_START_IMAGE,   // POST /upload image
    UPL_AP_STAT_POST_GET_IMAGE,     // POST /upload image
} UPL_AP_STAT_t;

#define WIFI_SSID "(YOUR WIFI_SSID)"
#define WIFI_PASSWORD "(SSID PASSWORD)"

Arduino_ST7789 tft = Arduino_ST7789(TFT_DC, TFT_RST, TFT_CS); //for display with CS pin
#define LED_PIN 4

WiFiServer server(80);

#define MAX_IMAGE_SIZE 65535
#define MAX_BUF_SIZE 1024
//#define IMAGE_DEBUG

int value = 0;
char boundbuf[MAX_BUF_SIZE];
int boundpos = 0;
char imagetypebuf[MAX_BUF_SIZE];
int imagetypepos = 0;
char imagebuf[MAX_IMAGE_SIZE];
int imagepos = 0;

void setup()
{
    bool ret;

    Serial.begin(115200);

    // We start by connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.println("WiFi Connecting.");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(WiFi.localIP());

    server.begin();

    tft.init(240, 240); // initialize a ST7789 chip, 240x240 pixels
    tft.fillScreen(BLACK);
    tft.setRotation(3);
}

void printUploadForm(WiFiClient client)
{
    Serial.println("printUploadForm");
    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
    client.println("<html>");
    client.println("<body>");
    client.println();
    client.println("<form action=\"upload\" method=\"post\" enctype=\"multipart/form-data\">");
    client.println("Select image to upload:");
    client.println("<input type=\"file\" name=\"fileToUpload\" id=\"fileToUpload\">");
    client.println("<input type=\"submit\" value=\"Upload Image\" name=\"submit\">");
    client.println("</form>");
    client.println();
    client.println("</body>");
    client.println("</html>");

    client.println();
}

void printImage(WiFiClient client)
{
    Serial.println("printImage");
    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    client.print(AP_DEMO_HTTP_200_IMAGE);
    client.print(imagetypebuf);
    client.print("\r\n\r\n");
#ifdef IMAGE_DEBUG
    Serial.print(AP_DEMO_HTTP_200_PNG);
#endif
    for (int i = 0; i < imagepos; i++)
    {
        client.write(imagebuf[i]);
#ifdef IMAGE_DEBUG
        Serial.write(imagebuf[i]);
#endif
    }
    drawArrayJpeg((uint8_t *)imagebuf, imagepos, 0, 0);
}

void printThanks(WiFiClient client)
{
    Serial.println("printThanks");
    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
    // and a content-type so the client knows what's coming, then a blank line:
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println();
    client.println("<html>");
    client.println("<body>");
    client.println();
    client.println("Thank You");
    client.println("<a id=\"logo\" href=\"/\"><img src=\"logo.bmp\" alt=\"logo\" border=\"0\"></a>");
    client.println();
    client.println("</body>");
    client.println("</html>");
    // the content of the HTTP response follows the header:
    //client.print("Click <a href=\"/H\">here</a> turn the LED on pin 5 on<br>");
    //client.print("Click <a href=\"/L\">here</a> turn the LED on pin 5 off<br>");

    // The HTTP response ends with another blank line:
    client.println();
}

void loop()
{
    int cnt;
    bool newconn = false;
    int stat;
    WiFiClient client = server.available(); // listen for incoming clients

    if (client)
    { // if you get a client,
        stat = 0;
        boundpos = 0;
        Serial.println("new client"); // print a message out the serial port
        String currentLine = "";      // make a String to hold incoming data from the client
        while (client.connected())
        { // loop while the client's connected
            cnt = client.available();
            if (cnt)
            { // if there's bytes to read from the client,
#ifdef IMAGE_DEBUG
                if (newconn == false)
                {
                    Serial.println(cnt);
                    newconn = true;
                }
#endif
                char c = client.read(); // read a byte, then
#ifndef IMAGE_DEBUG
                if (stat != UPL_AP_STAT_POST_GET_IMAGE)
                {
#endif
                    Serial.write(c); // print it out the serial monitor
#ifndef IMAGE_DEBUG
                }
#endif

                if (stat == UPL_AP_STAT_POST_GET_IMAGE)
                {
                    if (imagepos < MAX_IMAGE_SIZE)
                    {
                        imagebuf[imagepos] = c;
                        imagepos++;
                    }
                }
                if (c == '\n')
                { // if the byte is a newline character
#ifdef IMAGE_DEBUG
                    Serial.print("stat is equal=");
                    Serial.println(stat);
#endif
                    if (stat == UPL_AP_STAT_POST_START_BOUNDRY)
                    {
                        boundbuf[boundpos] = '\0';
                        boundpos++;
#ifdef IMAGE_DEBUG
                        Serial.println("&&&&&&&&&&&&&&&&&");
                        Serial.println(boundbuf);
                        Serial.println("&&&&&&&&&&&&&&&&&");
#endif
                        stat = UPL_AP_STAT_POST_UPLOAD;
                        Serial.println("stats=UPL_AP_STAT_POST_UPLOAD");
                    }
                    if (stat == UPL_AP_STAT_POST_START_IMAGE && currentLine.length() == 0)
                    {
                        imagetypebuf[imagetypepos] = '\0';
                        imagetypepos++;
#ifdef IMAGE_DEBUG
                        Serial.println("&&&&&&&&&&&&&&&&&");
                        Serial.println(imagetypebuf);
                        Serial.println("&&&&&&&&&&&&&&&&&");
#endif
                        imagepos = 0;
                        stat = UPL_AP_STAT_POST_GET_IMAGE;
                        Serial.println("stats=UPL_AP_STAT_POST_GET_IMAGE");
                    }
                    // if you got a newline, then clear currentLine:
                    currentLine = "";
                    newconn = false;
                }
                else if (c != '\r')
                {                     // if you got anything else but a carriage return character,
                    currentLine += c; // add it to the end of the currentLine
                    if (stat == UPL_AP_STAT_POST_START_BOUNDRY)
                    {
                        if (boundpos < MAX_BUF_SIZE)
                        {
                            boundbuf[boundpos] = c;
                            boundpos++;
                        }
                    }
                    if (stat == UPL_AP_STAT_POST_START_IMAGE)
                    {
                        if (imagetypepos < MAX_BUF_SIZE)
                        {
                            imagetypebuf[imagetypepos] = c;
                            imagetypepos++;
                        }
                    }
                }

                // Check to see if the client request was "GET / "
                if (currentLine.endsWith("GET / "))
                {
                    stat = UPL_AP_STAT_MAIN;
                    Serial.println("stats=UPL_AP_STAT_MAIN");
                }
                if (currentLine.endsWith("GET /logo.bmp "))
                {
                    stat = UPL_AP_STAT_GET_IMAGE;
                    Serial.println("stats=UPL_AP_STAT_GET_IMAGE");
                }
                if (currentLine.endsWith("POST /upload "))
                {
                    stat = UPL_AP_STAT_POST_UPLOAD;
                    Serial.println("stats=UPL_AP_STAT_POST_UPLOAD");
                }
                if (stat == UPL_AP_STAT_POST_UPLOAD && currentLine.endsWith("Content-Type: multipart/form-data; boundary="))
                {
                    stat = UPL_AP_STAT_POST_START_BOUNDRY;
                    Serial.println("stats=UPL_AP_STAT_POST_START_BOUNDRY");
                }
                if (stat == UPL_AP_STAT_POST_UPLOAD && currentLine.endsWith("Content-Type: image/"))
                {
                    stat = UPL_AP_STAT_POST_START_IMAGE;
                    Serial.println("stats=UPL_AP_STAT_POST_START_IMAGE");
                }
                if (stat == UPL_AP_STAT_POST_UPLOAD && boundpos > 0 && currentLine.endsWith(boundbuf))
                {
                    Serial.println("found boundry");
                }
                if (stat == UPL_AP_STAT_POST_GET_IMAGE && boundpos > 0 && currentLine.endsWith(boundbuf))
                {
                    Serial.println("found image boundry");
                    Serial.println(imagepos);
                    stat = UPL_AP_STAT_POST_UPLOAD;
                    imagepos = imagepos - boundpos - 3;
#ifdef IMAGE_DEBUG
                    Serial.println(imagepos);
                    for (int i = 0; i < imagepos; i++)
                    {
                        Serial.write(imagebuf[i]);
                    }
#endif
                    Serial.println("stats=UPL_AP_STAT_POST_UPLOAD");
                }
            }
            else
            {
                if (stat == UPL_AP_STAT_MAIN)
                {
                    printUploadForm(client);
                    break;
                }
                if (stat == UPL_AP_STAT_POST_UPLOAD)
                {
                    printThanks(client);
                    break;
                }
                if (stat == UPL_AP_STAT_GET_IMAGE)
                {
                    printImage(client);
                    break;
                }

                Serial.println("stat unknown");
                delay(1000);
                break;
            }
        }
        // close the connection:
        client.stop();
        Serial.println("client disonnected");
    }

    delay(100);
}

/*====================================================================================
  This sketch contains support functions to render the Jpeg images.

  Created by Bodmer 15th Jan 2017
  ==================================================================================*/

// Return the minimum of two values a and b
#define minimum(a, b) (((a) < (b)) ? (a) : (b))

//====================================================================================
//   This function opens the Filing System Jpeg image file and primes the decoder
//====================================================================================
void drawArrayJpeg(uint8_t *buff_array, uint32_t buf_size, int xpos, int ypos)
{

    Serial.println("=====================================");
    Serial.println("Drawing Array ");
    Serial.println("=====================================");

    boolean decoded = JpegDec.decodeArray(buff_array, buf_size);
    if (decoded)
    {
        // print information about the image to the serial port
        jpegInfo();

        // render the image onto the screen at given coordinates
        jpegRender(xpos, ypos);
    }
    else
    {
        Serial.println("Jpeg file format not supported!");
    }
}

//====================================================================================
//   Decode and paint onto the TFT screen
//====================================================================================
void jpegRender(int xpos, int ypos)
{

    // retrieve infomration about the image
    uint16_t *pImg;
    uint16_t mcu_w = JpegDec.MCUWidth;
    uint16_t mcu_h = JpegDec.MCUHeight;
    uint32_t max_x = JpegDec.width;
    uint32_t max_y = JpegDec.height;

    // Jpeg images are draw as a set of image block (tiles) called Minimum Coding Units (MCUs)
    // Typically these MCUs are 16x16 pixel blocks
    // Determine the width and height of the right and bottom edge image blocks
    uint32_t min_w = minimum(mcu_w, max_x % mcu_w);
    uint32_t min_h = minimum(mcu_h, max_y % mcu_h);

    // save the current image block size
    uint32_t win_w = mcu_w;
    uint32_t win_h = mcu_h;

    // record the current time so we can measure how long it takes to draw an image
    uint32_t drawTime = millis();

    // save the coordinate of the right and bottom edges to assist image cropping
    // to the screen size
    max_x += xpos;
    max_y += ypos;

    // read each MCU block until there are no more
    while (JpegDec.read())
    {

        // save a pointer to the image block
        pImg = JpegDec.pImage;

        // calculate where the image block should be drawn on the screen
        int mcu_x = JpegDec.MCUx * mcu_w + xpos;
        int mcu_y = JpegDec.MCUy * mcu_h + ypos;

        // check if the image block size needs to be changed for the right edge
        if (mcu_x + mcu_w <= max_x)
            win_w = mcu_w;
        else
            win_w = min_w;

        // check if the image block size needs to be changed for the bottom edge
        if (mcu_y + mcu_h <= max_y)
            win_h = mcu_h;
        else
            win_h = min_h;

        // copy pixels into a contiguous block
        if (win_w != mcu_w)
        {
            for (int h = 1; h < win_h - 1; h++)
            {
                memcpy(pImg + h * win_w, pImg + (h + 1) * mcu_w, win_w << 1);
            }
        }

        // draw image MCU block only if it will fit on the screen
        if ((mcu_x + win_w) <= tft.width() && (mcu_y + win_h) <= tft.height())
        {
            tft.drawRGBBitmap(mcu_x, mcu_y, pImg, win_w, win_h);
        }

        // Stop drawing blocks if the bottom of the screen has been reached,
        // the abort function will close the file
        else if ((mcu_y + win_h) >= tft.height())
            JpegDec.abort();
    }

    // calculate how long it took to draw the image
    drawTime = millis() - drawTime;

    // print the results to the serial port
    Serial.print("Total render time was    : ");
    Serial.print(drawTime);
    Serial.println(" ms");
    Serial.println("=====================================");
}

//====================================================================================
//   Send time taken to Serial port
//====================================================================================
void jpegInfo()
{
    Serial.println(F("==============="));
    Serial.println(F("JPEG image info"));
    Serial.println(F("==============="));
    Serial.print(F("Width      :"));
    Serial.println(JpegDec.width);
    Serial.print(F("Height     :"));
    Serial.println(JpegDec.height);
    Serial.print(F("Components :"));
    Serial.println(JpegDec.comps);
    Serial.print(F("MCU / row  :"));
    Serial.println(JpegDec.MCUSPerRow);
    Serial.print(F("MCU / col  :"));
    Serial.println(JpegDec.MCUSPerCol);
    Serial.print(F("Scan type  :"));
    Serial.println(JpegDec.scanType);
    Serial.print(F("MCU width  :"));
    Serial.println(JpegDec.MCUWidth);
    Serial.print(F("MCU height :"));
    Serial.println(JpegDec.MCUHeight);
    Serial.println(F("==============="));
}

//====================================================================================
//   Open a Jpeg file and dump it to the Serial port as a C array
//====================================================================================
void createArray(const char *filename)
{

    fs::File jpgFile; // File handle reference for SPIFFS
    //  File jpgFile;  // File handle reference For SD library

    if (!(jpgFile = SPIFFS.open(filename, "r")))
    {
        Serial.println(F("JPEG file not found"));
        return;
    }

    uint8_t data;
    byte line_len = 0;
    Serial.println("// Generated by a JPEGDecoder library example sketch:");
    Serial.println("// https://github.com/Bodmer/JPEGDecoder");
    Serial.println("");
    Serial.println("#if defined(__AVR__)");
    Serial.println("  #include <avr/pgmspace.h>");
    Serial.println("#endif");
    Serial.println("");
    Serial.print("const uint8_t ");
    while (*filename != '.')
        Serial.print(*filename++);
    Serial.println("[] PROGMEM = {"); // PROGMEM added for AVR processors, it is ignored by Due

    while (jpgFile.available())
    {

        data = jpgFile.read();
        Serial.print("0x");
        if (abs(data) < 16)
            Serial.print("0");
        Serial.print(data, HEX);
        Serial.print(","); // Add value and comma
        line_len++;
        if (line_len >= 32)
        {
            line_len = 0;
            Serial.println();
        }
    }

    Serial.println("};\r\n");
    // jpgFile.seek( 0, SeekEnd);
    jpgFile.close();
}
//====================================================================================
