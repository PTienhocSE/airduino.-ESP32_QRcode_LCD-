#include <Arduino_JSON.h>
#include <Adafruit_MLX90614.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SoftwareSerial.h>
#include <DS3231.h>
DS3231 _clock;
bool century = false;
bool h12Flag;
bool pmFlag;
byte year;
byte month;
byte date;
byte dOW;
byte hour;
byte minute;
byte second;


SoftwareSerial sim(23,5);
void _readSerial();
int _timeout;
String _buffer;
String number = "0762628832"; //-> change with your number



#define NUMBER_OF_STUDENT   100



const char* ssid = "QR_SYSTEM";
const char* password = "123456789";
WebServer server(80);

String string_time;

void writeString(char add,String data);
String read_String(char add);
void save_data_student();
void handleRoot();

bool have_qr_student  = false ;
float temperature = 0;

struct student
{
  int id;  
  String student_name;
  String student_class;
  float temperature;
  String Stringtime;
};

student studen_get_qr;
student data_student[NUMBER_OF_STUDENT] = {0};

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t * timer = NULL;
volatile SemaphoreHandle_t timerSemaphore;
uint32_t ui32_timer_1_second = false;
uint32_t ui32_counter_timer_1_s = 0;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display

uint32_t counter_test = 0;

int32_t init_json(String input)
{

  JSONVar myObject = JSON.parse(input);

  // JSON.typeof(jsonVar) can be used to get the type of the var
  if (JSON.typeof(myObject) == "undefined") {
    Serial.println("Parsing input failed!");
    return false;
  }
  
  Serial.print("JSON.typeof(myObject) = ");
  Serial.println(JSON.typeof(myObject)); // prints: object

 if (myObject.hasOwnProperty("id")) {
    Serial.print("id: = ");
    studen_get_qr.id = (int) myObject["id"];
    Serial.println(studen_get_qr.id);
  }
  
  if (myObject.hasOwnProperty("name")) {
    Serial.print("name: ");    
    studen_get_qr.student_name = myObject["name"];
    Serial.println(studen_get_qr.student_name);
    //Serial.println((const char*) myObject["name"]);
  }
  if (myObject.hasOwnProperty("class")) {
    Serial.print("class: ");
    studen_get_qr.student_class = myObject["class"];
    Serial.println(studen_get_qr.student_class);
  }
  return true;
}

void init_lcd()
{
  sim.begin(9600);
  lcd.init();                      // initialize the lcd 
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(3,0);
  lcd.print("KHKT TTHUE");
  lcd.setCursor(0,1);
  lcd.print("THPT DANGHUYTRU");
   lcd.setCursor(0,2);
  delay(3000);
  lcd.clear(); 
}
void IRAM_ATTR onTimer(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  ui32_timer_1_second = true;
  ui32_counter_timer_1_s++;
  
  portEXIT_CRITICAL_ISR(&timerMux);
  // Give a semaphore that we can check in the loop
  xSemaphoreGiveFromISR(timerSemaphore, NULL);
  // It is safe to use digitalRead/Write here if you want to toggle an output
}
void toogle_buzzer()
{
  digitalWrite(19,LOW);
  delay(1000);
  digitalWrite(19,HIGH);
  delay(1000);  
  digitalWrite(19,LOW);
  delay(1000);
  digitalWrite(19,HIGH);
  delay(1000);digitalWrite(19,LOW);
  delay(1000);
  digitalWrite(19,HIGH);
  delay(1000);
}
void setup() 
{
  Serial.begin(115200);
  Serial2.begin(115200);
  EEPROM.begin(1024);
  pinMode(19,OUTPUT);
  digitalWrite(19,HIGH);
  while (!Serial);
  Serial.println("Start project");
  
  Serial.print("Setting AP mode");
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP(); //mặc định là 192.168.4.1
  Serial.print("AP IP address: ");
  Serial.println(IP);
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }
  server.on("/", handleRoot);
  server.begin();
  Serial.println("HTTP server started");
  
  init_lcd();
  Serial.println("Adafruit MLX90614 test");
  

  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX sensor. Check wiring.");
    
    lcd.clear();
    while (1)
    {      
      lcd.setCursor(0,0);
      lcd.print("   KHOI DONG");
      lcd.setCursor(0,1);
      lcd.print("  LOI CAM BIEN");
      delay(1000);
    }
  };
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("   KHOI DONG");
  lcd.setCursor(0,1);
  lcd.print("  BINH THUONG");
  delay(2000);
  Serial.print("Emissivity = "); Serial.println(mlx.readEmissivity());
  Serial.println("================================================");

   // Create semaphore to inform us when the timer has fired
  timerSemaphore = xSemaphoreCreateBinary();

  // Use 1st timer of 4 (counted from zero).
  // Set 80 divider for prescaler (see ESP32 Technical Reference Manual for more
  // info).
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer.
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second (value in microseconds).
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, 1000000, true);

  // Start an alarm
  Serial.println("start an alarm");
  timerAlarmEnable(timer);
}

void loop() 
{
  server.handleClient();
  string_time = read_time();
  if (Serial.available())
  {
    //YYMMDDwHHMMSS
    set_time();
  }
  if (Serial2.available()) 
  {
    String data_qr = Serial2.readString();
    Serial.println(data_qr);
    if (init_json(data_qr) == true)
    {    
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(studen_get_qr.student_name);
        lcd.setCursor(0,1);
        lcd.print(studen_get_qr.student_class);
        lcd.setCursor(8,1);
        lcd.print(studen_get_qr.id);
        delay(1000);
        lcd.clear();
        have_qr_student = true;
        lcd.setCursor(0,0);
        lcd.print("    VUI LONG");
        lcd.setCursor(0,1);
        lcd.print("  DO NHIET DO");
        delay(1000);
        //lcd.clear();
        ui32_counter_timer_1_s = 0;
    }
  }

  if  ((have_qr_student == true)&&(ui32_timer_1_second== true))
  {
    ui32_timer_1_second = false;
    temperature = mlx.readObjectTempC();
    temperature = temperature + 4;
    Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempC());
    Serial.print("*C\tObject = "); Serial.print(temperature); Serial.println("*C");
    Serial.print("Ambient = "); Serial.print(mlx.readAmbientTempF());
    Serial.print("*F\tObject = "); Serial.print(mlx.readObjectTempF()); Serial.println("*F"); 
//    lcd.setCursor(0,0);
//    lcd.print("NDo_MT: ");
//    lcd.print(mlx.readAmbientTempC());
//    lcd.setCursor(0,1);
//    lcd.print("NDo_CT: ");
    
    //lcd.print(temperature);
    
    if (temperature > 32)
    {
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("    DANG DO ");
      lcd.setCursor(0,1);
      lcd.print("    NHIET DO");
      float temp = 0;
      for (int i = 0;i<10;i++)
      {
        temp = mlx.readObjectTempC();
        if (temp > temperature)
        {
          temperature = temp;
        }       
        delay(500);
      }
      have_qr_student = false;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("     CAM ON");
      lcd.setCursor(0,1);
      lcd.print("NDo_CT: ");
      lcd.print(temperature);
      if (temperature >  37)
      {
        toogle_buzzer();
      }
      save_data_student();  
      delay(2000);
      lcd.clear();  
      SendMessage();
    }
    else
    {
      if (ui32_counter_timer_1_s > 10)
      {
        have_qr_student = false;
        temperature = 0;    
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("CHUA DO");
        delay(2000);
        lcd.clear();   
        save_data_student();     
      }
      
    }
    
  }
  else if (ui32_timer_1_second == true)
  {
    if (ui32_counter_timer_1_s%2 ==0)
    {
      
      lcd.setCursor(0,0);
      lcd.print("    VUI LONG");
      lcd.setCursor(0,1);
      lcd.print("   QUET MA QR"); 
    }
    else
    {
      lcd.clear();
      }
  }
  
}



void save_data_student()
{
  uint32_t i = 0;
        uint32_t check_full = 0;
        for(i = 0;i<NUMBER_OF_STUDENT;i++)
        {
          if (data_student[i].id == studen_get_qr.id)
          {
            Serial.println("exist student in store");
            data_student[i].temperature = temperature; 
            data_student[i].Stringtime = string_time;
            break;
          }
          else if(data_student[i].id == 0)
          {
            Serial.println("have empty student");
            data_student[i].id = studen_get_qr.id;
            data_student[i].student_name = studen_get_qr.student_name;
            data_student[i].student_class = studen_get_qr.student_class;  
            data_student[i].temperature = temperature;
            data_student[i].Stringtime = string_time; 
            break;          
          }
          else
          {
            
            check_full++;
          }
        }
        if (check_full == NUMBER_OF_STUDENT)
        {
          Serial.println("data full");
          for(i = 0;i<NUMBER_OF_STUDENT;i++)
          {
            data_student[i].id = 0;
              data_student[i].student_name = "";
              data_student[i].student_class = ""; 
              data_student[i].temperature = 0;
              data_student[i].Stringtime = "";
          }          
        }
  } 


void writeString(char add,String data)
{
  int _size = data.length();
  int i;
  for(i=0;i<_size;i++)
  {
    EEPROM.write(add+i,data[i]);
  }
  EEPROM.write(add+_size,'\0');   //Add termination null character for String Data
  EEPROM.commit();
}


String read_String(char add)
{
  int i;
  char data[100]; //Max 100 Bytes
  int len=0;
  unsigned char k;
  k=EEPROM.read(add);
  while(k != '\0' && len<500)   //Read until null character
  {    
    k=EEPROM.read(add+len);
    data[len]=k;
    len++;
  }
  data[len]='\0';
  return String(data);
}

void handleRoot() 
{
  uint32_t i = 0;
  String data_web = "";
  data_web = (String)"STT"+(String)"    " + (String)"Ma HS" +(String)"    " + (String)"Ten HS" +(String)"      "+ (String)"Lop" +(String)"      " + (String)"Nhietdo" +(String)"      " +(String)"Thoi gian" ; 
  data_web = data_web + "\r\n";
  for(i = 0;i<NUMBER_OF_STUDENT;i++)
  {
    if (data_student[i].id != 0)
    {
      data_web = data_web + String(i);
      data_web = data_web +(String)"    ";
      data_web = data_web + String(data_student[i].id);
      data_web = data_web +(String)"    ";
      data_web = data_web + String(data_student[i].student_name);
      data_web = data_web +(String)"                ";
      data_web = data_web + String(data_student[i].student_class);   
      data_web = data_web +(String)"    ";
      data_web = data_web + String(data_student[i].temperature);   
      data_web = data_web +(String)"    ";
      data_web = data_web + String(data_student[i].Stringtime);   
      data_web = data_web + "\r\n";
      Serial.println(data_web);
    }
  }
  server.send(300, "text/plain",data_web);
 

}

void SendMessage()
{
  //Serial.println ("Sending Message");
  sim.println("AT+CMGF=1");    //Sets the GSM Module in Text Mode
  delay(200);
  //Serial.println ("Set SMS Number");
  sim.println("AT+CMGS=\"" + number + "\"\r"); //Mobile phone number to send message
  delay(500);
  String SMS = studen_get_qr.student_name + (String)" : " + String(temperature) ;
  sim.println(SMS);
  delay(100);
  sim.println((char)26);// ASCII code of CTRL+Z
  delay(200);
  _readSerial();
  Serial.println(_buffer);
}
void RecieveMessage()
{
  Serial.println ("sim800L Read an SMS");
  delay (1000);
  sim.println("AT+CNMI=2,2,0,0,0"); // AT Command to receive a live SMS
  delay(1000);
  Serial.write ("Unread Message done");
}
void _readSerial() {
  _timeout = 0;
  while  (!sim.available() && _timeout < 12000  )
  {
    delay(13);
    _timeout++;
  }
  if (sim.available()) {
    _buffer =  sim.readString();
  }
}


String read_time()
{
  String Stringtime = "";
  // send what's going on to the serial monitor.
  
  // Start with the year
  Serial.print("2");
  if (century) {      // Won't need this for 89 years.
    Serial.print("1");
  } else {
    Serial.print("0");
  }
  Serial.print(_clock.getYear(), DEC);
  Serial.print(' ');
  
  // then the month
  Serial.print(_clock.getMonth(century), DEC);
  Serial.print(" ");
  
  // then the date
  Serial.print(_clock.getDate(), DEC);
  Serial.print(" ");
  
  // and the day of the week
  Serial.print(_clock.getDoW(), DEC);
  Serial.print(" ");
  
  // Finally the hour, minute, and second
  Serial.print(_clock.getHour(h12Flag, pmFlag), DEC);
  Serial.print(" ");
  Serial.print(_clock.getMinute(), DEC);
  Serial.print(" ");
  Serial.print(_clock.getSecond(), DEC);
 
  Serial.println();
  Stringtime = String(_clock.getHour(h12Flag, pmFlag))+":"+String(_clock.getMinute())+":"+String(_clock.getSecond());
  Stringtime = Stringtime + "-" + String(_clock.getDate())+"/"+String(_clock.getMonth(century));
  
  Serial.println(Stringtime);
  delay(1000);
  return Stringtime;
  }

  
void getDateStuff(byte& year, byte& month, byte& date, byte& dOW,
                  byte& hour, byte& minute, byte& second) {
    // Call this if you notice something coming in on
    // the serial port. The stuff coming in should be in
    // the order YYMMDDwHHMMSS, with an 'x' at the end.
    boolean gotString = false;
    char inChar;
    byte temp1, temp2;
    char inString[20];
    
    byte j=0;
    while (!gotString) {
        if (Serial.available()) {
            inChar = Serial.read();
            inString[j] = inChar;
            j += 1;
            if (inChar == 'x') {
                gotString = true;
            }
        }
    }
    Serial.println(inString);
    // Read year first
    temp1 = (byte)inString[0] -48;
    temp2 = (byte)inString[1] -48;
    year = temp1*10 + temp2;
    // now month
    temp1 = (byte)inString[2] -48;
    temp2 = (byte)inString[3] -48;
    month = temp1*10 + temp2;
    // now date
    temp1 = (byte)inString[4] -48;
    temp2 = (byte)inString[5] -48;
    date = temp1*10 + temp2;
    // now Day of Week
    dOW = (byte)inString[6] - 48;
    // now hour
    temp1 = (byte)inString[7] -48;
    temp2 = (byte)inString[8] -48;
    hour = temp1*10 + temp2;
    // now minute
    temp1 = (byte)inString[9] -48;
    temp2 = (byte)inString[10] -48;
    minute = temp1*10 + temp2;
    // now second
    temp1 = (byte)inString[11] -48;
    temp2 = (byte)inString[12] -48;
    second = temp1*10 + temp2;
}

void set_time()
{
   getDateStuff(year, month, date, dOW, hour, minute, second);
        
        _clock.setClockMode(false);  // set to 24h
        //set_clockMode(true); // set to 12h
        
        _clock.setYear(year);
        _clock.setMonth(month);
        _clock.setDate(date);
        _clock.setDoW(dOW);
        _clock.setHour(hour);
        _clock.setMinute(minute);
        _clock.setSecond(second);
        
        // Test of alarm functions
        // set A1 to one minute past the time we just set the _clock
        // on current day of week.
        _clock.setA1Time(dOW, hour, minute+1, second, 0x0, true,
                        false, false);
        // set A2 to two minutes past, on current day of month.
        _clock.setA2Time(date, hour, minute+2, 0x0, false, false,
                        false);
        // Turn on both alarms, with external interrupt
        _clock.turnOnAlarm(1);
        _clock.turnOnAlarm(2);
  }
