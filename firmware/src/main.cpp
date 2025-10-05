// Adapted main.cpp based on nikthefix TRulerNick example
#include "AXS15231B.h"
#include <TFT_eSPI.h>
#include <Wire.h>
#include "fontM.h"
#include "fontH.h"
#include "fontS.h"
#include "fontT.h"
#include "pins_config.h"

#include <Arduino.h>
#include "vault.h"

// Forward declarations
void show_password_overlay(const String &pw);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);

bool debug_swap_state = false; // toggle to test sprite byte order at runtime

uint8_t ALS_ADDRESS = 0x3B;
uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x8};
int tx=0;
int ty=0;
int cx=-1;
int cy=-1;
int xpos[4]={4,48,92,136};
int ypos[4]={4,48,92,136};
String btns[4][4]={{"7","8","9","/"},{"4","5","6","*"},{"1","2","3","-"},{"0",".","=","+"}};
String num="";
int deb=0;
int operation=0;
float numBuf=0;
bool touch_held=false;
#define time_out_reset 30000
uint16_t touch_timeout=0;
uint16_t cnt=0;
// PIN-entry state
bool in_pin_mode = false;
String pin_buffer = "";

// colors
unsigned short col1=0x39C7;
unsigned short col2=0x2945;
unsigned short col3=TFT_ORANGE;
unsigned short col4=TFT_SILVER;
unsigned short cls[4][4]={{col1,col1,col1,col2},{col1,col1,col1,col2},{col1,col1,col1,col2},{col1,col2,col2,col2}};
unsigned short tcls[4][4]={{col4,col4,col4,col3},{col4,col4,col4,col3},{col4,col4,col4,col3},{col4,col3,col3,col3}};

void draw()
{
  sprite.fillSprite(TFT_BLACK);
  sprite.loadFont(fontT);
  sprite.setTextDatum(0);
  sprite.fillRoundRect(190,48,460,106,2,col2);
  sprite.setTextColor(TFT_ORANGE,TFT_BLACK);
  sprite.drawString("CLEAR",576,8);
  sprite.fillRect(556,34,80,6,TFT_BLUE);
  sprite.setTextDatum(4);

  for(int i=0;i<4;i++)
    for(int j=0;j<4;j++){
      sprite.setTextColor(tcls[i][j],cls[i][j]);
      sprite.fillRoundRect(xpos[j],ypos[i],40,40,4,cls[i][j]);
      sprite.drawString(btns[i][j],xpos[j]+20,ypos[i]+20,4);
      if(cx==j && cy==i) sprite.fillCircle(xpos[j]+8,ypos[i]+8,4,TFT_RED);
    }
  sprite.unloadFont();

  sprite.setTextDatum(0);
  sprite.loadFont(fontT);
  sprite.setTextColor(TFT_SILVER,TFT_BLACK);
 // sprite.drawString("T-DISPLAY S3 LONG",190,8);
  sprite.unloadFont();

  sprite.loadFont(fontH);
  sprite.setTextColor(TFT_SILVER,col2);
  bool lastDot=false;
  if(num.length()>0 && num.charAt(num.length()-1)=='.') lastDot=true;
  if(num!="" && lastDot==0){
    int nn=num.toInt();
    float nl=num.toFloat()*1000;
    if((nl-(nn*1000))==0) sprite.drawString(String(nn),210,58);
    else sprite.drawString(num,210,58);
    sprite.unloadFont();
  }
  if(lastDot) sprite.drawString(num,210,58);

  //sprite.loadFont(fontS);
  sprite.setTextColor(col3,TFT_BLACK);
 // sprite.drawString("NikTheFix ",190,161);
  sprite.setTextColor(0x8410,TFT_BLACK);
  //sprite.drawString("VOLOS PROJECTS ",494,161);
 // sprite.pushImage(604,158,30,20,yt);

  // Push the full sprite to the LCD in smaller horizontal stripes to avoid
  // long blocking SPI transfers that can trigger the watchdog on some boards.
  const int STRIPE_H = 40; // stripe height in pixels (tuneable)
  uint16_t *bmp = (uint16_t *)sprite.getPointer();
  for (int sy = 0; sy < 180; sy += STRIPE_H) {
    int h = STRIPE_H;
    if (sy + h > 180) h = 180 - sy;
    // data pointer offset: each row is 640 pixels
    uint16_t *stripe_ptr = bmp + (sy * 640);
    lcd_PushColors_rotated_90(0, sy, 640, h, stripe_ptr);
    // small yield to allow watchdog and RTOS to run
    delay(1);
  }
}



void draw_pin_ui()
{
  sprite.fillSprite(TFT_BLACK);
  sprite.loadFont(fontT);
  sprite.setTextColor(TFT_SILVER, TFT_BLACK);
  sprite.drawString("Enter PIN", 40, 20);
  sprite.unloadFont();

  sprite.loadFont(fontM);
  sprite.drawString(String(pin_buffer), 40, 80);
  sprite.unloadFont();

  // draw on screen
  lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t*)sprite.getPointer());
}

void getTouch()
{
    uint8_t buff[20] = {0};
    Wire.beginTransmission(ALS_ADDRESS);
    Wire.write(read_touchpad_cmd, 8);
    Wire.endTransmission();
  Wire.requestFrom((uint8_t)ALS_ADDRESS, (uint8_t)8);
    while (!Wire.available());
    Wire.readBytes(buff, 8);

    int pointX=-1;
    int pointY=-1;
    int type = 0;

    type = AXS_GET_GESTURE_TYPE(buff);
    pointX = AXS_GET_POINT_X(buff,0);
    pointY = AXS_GET_POINT_Y(buff,0);

        if(pointX > 640) pointX = 640;
        if(pointY > 180) pointY = 180;

        
        tx=map(pointX,627,10,0,640);
        ty=map(pointY,180,0,0,180);
        
        if(tx>180 && tx<590) return;  //mask invalid touch area
        if(ty>50 && tx>590) return;   //mask invalid tough area

        for(int i=0;i<4;i++)
        {if(tx>xpos[i] && tx<xpos[i]+44)
        cx=i;
        if(ty>ypos[i] && ty<ypos[i]+44)
        cy=i;}

        if(tx>=590 && tx<=640 && ty>=0 && ty<=50)
        {num=""; numBuf=0; operation=0; cx=-1; cy=-1;}

    if (cx>=0 && cx<4 && cy>=0 && cy<4 ) {
        String cs=btns[cy][cx];
        
        if(cs=="1" || cs=="2" || cs=="3" || cs=="4" || cs=="5" || cs=="6" || cs=="7" || cs=="8" || cs=="9" || cs=="0")
        {num=num+cs; if(num.length()>7) {num=""; numBuf=0; operation=0; cx=-1; cy=-1;}}

        if(cs==".")
        {
          bool finded=0;
          for(int i=0;i<num.length();i++)
          if(num.charAt(i)=='.')
          finded=true;

          if(!finded)
          num=num+cs;
        }

        if(cs=="+")
        {operation=1; numBuf=num.toFloat();
        num="";
        }   

        if(cs=="-")
        {operation=2; numBuf=num.toFloat();
        num="";
        } 

         if(cs=="*")
        {operation=3; numBuf=num.toFloat();
        num="";
        } 

         if(cs=="/")
        {operation=4; numBuf=num.toFloat();
        num="";
        } 

        if(cs=="=")
        {
          
        if(operation==1) 
        {numBuf=numBuf+num.toFloat();
        num=String(numBuf);}

        if(operation==2) 
        {numBuf=numBuf-num.toFloat();
        num=String(numBuf);}

        if(operation==3) 
        {numBuf=numBuf*num.toFloat();
        num=String(numBuf);}

        if(operation==4) 
        {numBuf=numBuf/num.toFloat();
        num=String(numBuf);}
        
        }        
    }     
}


void setup() {
    pinMode(TOUCH_INT, INPUT_PULLUP);
  Serial.begin(115200);
  Serial.println("startup: Serial initialized");
    sprite.createSprite(640, 180);    // full screen landscape sprite in psram
    sprite.setSwapBytes(1);

    // comment out if using variable brightness
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);


    // if using esp32 3.0.0-alpha3 then you can use the following 2 lines to adjust the backlight brightness
    //ledcAttach(TFT_BL, 10000, 8);// pin, pwm freq, resolution in bits
    //ledcWrite(TFT_BL, 30);// pin, pulse width (a.k.a brightness 0-255)

    

    //ini touch screen 
    pinMode(TOUCH_RES, OUTPUT);
    digitalWrite(TOUCH_RES, HIGH);delay(2);
    digitalWrite(TOUCH_RES, LOW);delay(10);
    digitalWrite(TOUCH_RES, HIGH);delay(2);
    Wire.begin(TOUCH_IICSDA, TOUCH_IICSCL);

    //init display 
    axs15231_init();
  Serial.println("axs15231_init done");
    // init vault and defaults
    vault_init();
  Serial.println("vault_init done");
    if (!vault_has_pin()) {
      vault_set_pin("1234");
      vault_store_password("default", "345678");
    }
    draw();
    
}

void loop() {
  if(digitalRead(TOUCH_INT)==LOW) {
    if(touch_held==false){ getTouch(); draw(); }
    touch_held=true;
    touch_timeout=0;
  }
  // runtime debug: if user taps very top-left corner (0..40, 0..40) toggle swap
  if (digitalRead(TOUCH_INT)==LOW) {
    // getTouch will set tx/ty; check small corner
    if (tx >= 0 && tx <= 40 && ty >= 0 && ty <= 40) {
      debug_swap_state = !debug_swap_state;
      sprite.setSwapBytes(debug_swap_state ? 1 : 0);
      Serial.print("Toggled swapBytes -> "); Serial.println(debug_swap_state ? 1 : 0);
      //draw_test_pattern();
      delay(500);
    }
  }
  // if user tapped CLEAR area start PIN mode
  if (tx>=590 && tx<=640 && ty>=0 && ty<=50 && !in_pin_mode) {
    in_pin_mode = true;
    pin_buffer = "";
    draw_pin_ui();
    delay(200);
  }

  // PIN mode handling: use calculator keypad to enter PIN and '=' to submit
  if (in_pin_mode && cx>=0 && cy>=0) {
    String cs = btns[cy][cx];
    if (cs=="=") {
      if (vault_check_pin(pin_buffer)) {
        Serial.println("PIN OK - typing password");
        String pw;
        if (vault_retrieve_password("default", pw)) {
          show_password_overlay(pw);
        } else {
          Serial.println("vault_retrieve_password failed");
        }
      } else {
        Serial.println("PIN FAIL");
      }
      in_pin_mode = false;
      // consume this touch so we don't loop on the same press
      cx = -1; cy = -1;
      draw();
      delay(300);
    } else if (cs==".") {
      // ignore
    } else {
      pin_buffer += cs;
      // consume this touch so we don't append the same digit repeatedly
      cx = -1; cy = -1;
      draw_pin_ui();
      delay(200);
    }
  }
  touch_timeout++;
  if(touch_timeout >= time_out_reset) { touch_held=false; touch_timeout=time_out_reset; }
}

void show_password_overlay(const String &pw)
{
  // simple overlay that shows the password for 3 seconds
  sprite.fillSprite(TFT_BLACK);
  sprite.loadFont(fontT);
  sprite.setTextColor(TFT_SILVER, TFT_BLACK);
  sprite.drawString("Password:", 40, 20);
  sprite.unloadFont();

  sprite.loadFont(fontM);
  sprite.setTextColor(TFT_ORANGE, TFT_BLACK);
  sprite.drawString(pw, 40, 80);
  sprite.unloadFont();

  lcd_PushColors_rotated_90(0, 0, 640, 180, (uint16_t*)sprite.getPointer());
  delay(3000);
  draw();
}

// forward declaration so loop() can call it before its definition
void show_password_overlay(const String &pw);