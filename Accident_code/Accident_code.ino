#include <Servo.h>
#include <TinyGPS.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SparkFun_APDS9960.h>

// Pin and hardware setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
TinyGPS gps;
SoftwareSerial gps_port(3, 4);
Servo myservo;
char sz[32];
int pos = 0;
int fr = 6; // Fire sensor
int buzzer = 9; // Buzzer
#define rly 10
#define led 11
#define rb 13
#define alc A3
#define vib 7
#define APDS9960_INT 2
float latitude, longitude;
const String EMERGENCY_PHONE = "+91*********";//Place your mobilr number with country code
int val = 1;
SparkFun_APDS9960 apds = SparkFun_APDS9960();
int isr_flag = 0;

void setup()
{
  // Pin modes
  pinMode(buzzer, OUTPUT);
  pinMode(rly, OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(rb, INPUT_PULLUP);
  pinMode(alc, INPUT);
  pinMode(fr, INPUT);
  pinMode(vib, INPUT);
  pinMode(APDS9960_INT, INPUT);
  myservo.attach(8);
  digitalWrite(buzzer, LOW);
  digitalWrite(rly, LOW);

  // LCD initialization
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(1, 0);
  lcd.print("Hello NANO!");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Accident System");
  lcd.setCursor(0, 1);
  lcd.print(" ON- Screen Test");
  delay(3000);
  lcd.clear();

  // Servo initialization
  myservo.write(90);
  myservo.detach();
  // Serial and GPS initialization
  Serial.begin(9600);
  gps_port.begin(9600);
  delay(2000);
  init_GSM();
  delay(3000);

  // Gesture sensor initialization
  guester_ina();
  delay(3000);
  // Alcohol detection (run only once at the beginning)
  if (analogRead(alc) >= 320)
  {
    myservo.attach(8);
    handleEvent("Alcohol lvl High", Alchol_sms);
    myservo.write(0);
    delay(1000);
    myservo.detach();
  }
  else {
    myservo.attach(8);
    myservo.write(0);
    delay(1000);
    myservo.detach();
    lcd.print("Engine ON");
    delay(1000);
    lcd.clear();
  }
}

void loop()
{
  Serial.println("Entering loop"); // Debugging statement
  checkSensorsAndHandleEvents();
}

void checkSensorsAndHandleEvents()
{
  encode_gps();

  if (digitalRead(vib) == LOW && val == 1)
  {
    handleEvent("Crash detected", Accident_sms);
  }
  else if (digitalRead(fr) == LOW && val == 1)
  {
    handleEvent("Fire detected", Fire_sms);
  }
  else if (isr_flag == 1 && val == 1)
  {
    detachInterrupt(digitalPinToInterrupt(APDS9960_INT));
    handleGesture();
    isr_flag = 0;
    attachInterrupt(digitalPinToInterrupt(APDS9960_INT), interruptRoutine, FALLING);
  }

  check_reset(); // Check if reset button is pressed
  delay(1000);
}

// Handle sensor events
void handleEvent(const char* message, void (*alertFunction)())
{
  lcd.setCursor(1, 0);
  lcd.print(message);
  delay(3000);
  lcd.clear();
  alertFunction();
  make_call();
  val = 0; // Set val to prevent further detection
}

// Gesture sensor setup
void guester_ina()
{
  lcd.print(F("GestureTest"));
  delay(1000);
  lcd.clear();

  // Initialize interrupt service routine
  attachInterrupt(digitalPinToInterrupt(APDS9960_INT), interruptRoutine, FALLING);

  // Initialize APDS-9960
  if (apds.init())
  {
    Serial.println("APDS-9960 Initialization complete");
    lcd.print(F("Initialization complete"));
    delay(1000);
    lcd.clear();
  }
  else
  {
    Serial.println("APDS-9960 init error!");
    lcd.print(F("APDS-9960 init error!"));
    delay(1000);
    lcd.clear();
  }

  // Start gesture sensor
  if (apds.enableGestureSensor(true))
  {
    Serial.println("Gesture sensor running");
    lcd.print(F("Gesture running"));
    delay(1000);
    lcd.clear();
  }
  else
  {
    Serial.println("Gesture sensor error!");
    lcd.print(F("Gesture error!"));
    delay(1000);
    lcd.clear();
  }
}

// Fire alert SMS
void Fire_sms()
{
  digitalWrite(buzzer, HIGH);
  send_sms("Car Fire Alert at ");
  digitalWrite(buzzer, LOW);
}

void interruptRoutine()
{
  isr_flag = 1;
}

// Gesture actions
void handleGesture()
{
  if (apds.isGestureAvailable())
  {
    switch (apds.readGesture())
    {
      case DIR_UP:
        lcd.print("UP");
        Serial.println(F("Gesture UP detected"));

        break;
      case DIR_DOWN:
        lcd.print("medical help");
        Serial.println(F("Gesture DOWN detected"));
        Heart_sms();
        make_call();
        break;
      case DIR_LEFT:
        lcd.print("medical help");
        Serial.println(F("Gesture LEFT detected"));
        Heart_sms();
        make_call();
        break;
      case DIR_RIGHT:
        lcd.print("RIGHT");
        Serial.println(F("Gesture RIGHT detected"));
        break;
      case DIR_FAR:
      case DIR_NEAR:
      default:
        lcd.print("medical help");
        Serial.println(F("Unknown gesture or medical help"));
        Heart_sms();
        make_call();
        break;
    }
    delay(1000);
    lcd.clear();
  }
}

// Check if reset button is pressed
void check_reset()
{
  Serial.println("Checking reset button"); // Debugging statement
  if (digitalRead(rb) == LOW)
  {
    Serial.println("Reset button pressed"); // Debugging statement
    lcd.print("Resetting...");
    delay(1000);
    lcd.clear();
    val = 1; // Reset the val to enable detection again
  }
  delay(1000);
}

// Accident alert SMS
void Accident_sms()
{
  digitalWrite(buzzer, HIGH);
  send_sms("Car Accident Alert at ");
  digitalWrite(buzzer, LOW);
}

// Alcohol alert SMS
void Alchol_sms()
{
  digitalWrite(buzzer, HIGH);
  send_sms("Alcohol Alert at ");
  digitalWrite(rly, HIGH);
  digitalWrite(buzzer, LOW);
}

// Medical alert SMS
void Heart_sms()
{
  digitalWrite(buzzer, HIGH);
  send_sms("Medical Alert at ");
  lcd.clear();
  digitalWrite(buzzer, LOW);
}

// Send SMS with GPS location
void send_sms(const char* alert_message)
{
  digitalWrite(led, HIGH);
  Serial.println(F("Sending SMS..."));
  lcd.print("Sending SMS...");
  Serial.println("AT");
  delay(2000);
  Serial.println("AT+CMGF=1"); // Set SMS to text mode
  delay(2000);
  Serial.print("AT+CMGS=\"");
  Serial.print(EMERGENCY_PHONE);
  Serial.println("\"");
  delay(2000);
  Serial.print(alert_message);
  Serial.print("http://www.google.com/maps/place/");
  delay(2000);
  Serial.print(latitude, 6);
  Serial.print(",");
  Serial.println(longitude, 6);
  delay(1000);
  Serial.print(sz);
  Serial.print(",");
  Serial.write(26); // ASCII code for Ctrl+Z to send the SMS
  delay(2000);
  Serial.println(F("SMS Sent"));
  lcd.clear();
  digitalWrite(led, LOW);
}

// Make emergency call
void make_call()
{
  digitalWrite(led, HIGH);
  lcd.print("calling....");
  Serial.print("ATD");
  Serial.print(EMERGENCY_PHONE);
  Serial.print(";");
  delay(30000); // 20 sec delay
  Serial.println("ATH");
  delay(5000); // 1 sec delay
  lcd.clear();
  digitalWrite(led, LOW);
}

// Encode GPS data
void encode_gps()
{
  for (unsigned long start = millis(); millis() - start < 1000;)
  {
    while (gps_port.available())
    {
      char a = gps_port.read();
      if (gps.encode(a))
      {
        getgps(gps);
        break; // Break out of the loop once GPS data is encoded
      }
    }
  }
}

void getgps(TinyGPS &gps)
{
  gps.f_get_position(&latitude, &longitude);
  print_date(gps);
}

// Initialize GSM
void init_GSM()
{
  Serial.println("Initializing GSM...");
  Serial.println("AT");
  delay(2000);
  Serial.println("AT+CMGF=1");
  delay(2000);
  Serial.println("GSM Initialized");
}

// Print date
static void print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age != TinyGPS::GPS_INVALID_AGE)
  {
    sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d ",
            month, day, year, hour, minute, second);
  }
}
