/******************************************************************************
*
*                     RICOH THETA S Remote Control Software 
*                             Single Button Edition
*                                      for 
*                     Switch Science ESP-WROOM-02 Dev.board
* 
* Author  : Katsuya Yamamoto
* Version : see `sThetaRemoteVersion' value
* 
* It shows the minimum configuration of hardware below
*  - ESP-WROOM-02 Dev.board : https://www.switch-science.com/catalog/2500/
* Optional hardware 
*  - FET LED Driver         : https://www.switch-science.com/catalog/2397/
* 
*    Pin connection
*    ESP-WROOM-02 Pin            FET LED Driver Pin
*         3V3    ------------------    VCC
*         IO4    ------------------    LED1
*         IO5    ------------------    LED2
*         GND    ------------------    GND
*
******************************************************************************/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>  // Add JSON Library  https://github.com/bblanchon/ArduinoJson

const char sThetaRemoteVersion[] = "v01.02";    //Last Update 2015-11-22 : It was modified in order just in case.
//In the old version THETA S search mode, Once in the vicinity of the remote controler has more than 21 THETA, buffer overflow occurs.

//--- ESP-WROOM-02 Hardwear ---
const int buttonPin = 0;
const int led1Pin = 4;
const int led2Pin = 5;

//--- Wi-Fi Connect ---
#define THETA_SSID_TOP_ADR    0
#define THETA_SSID_BYTE_LEN   19
#define THETA_PASS_OFFSET     7
#define THETA_PASS_BYTE_LEN   8

char ssid[THETA_SSID_BYTE_LEN+1] = "THETAXS00000000.OSC";   //This initial value is not used.
char password[THETA_PASS_BYTE_LEN+1] = "00000000";          //This initial value is not used.

#define WIFI_CONNECT_THETA    0
#define WIFI_SCAN_THETA       1
int iConnectOrScan = WIFI_CONNECT_THETA;

#define NEAR_RSSI_THRESHOLD   -45   //[db]

//--- HTTP  ---
// Use WiFiClient class to create TCP connections
WiFiClient client;

const char sHost[] = "192.168.1.1";
const int iHttpPort = 80;

#define   HTTP_TIMEOUT_DISABLE  0 // never times out during transfer. 
#define   HTTP_TIMEOUT_NORMAL   1
#define   HTTP_TIMEOUT_STATE    2
#define   HTTP_TIMEOUT_STARTCAP 5
#define   HTTP_TIMEOUT_STOPCAP  70
#define   HTTP_TIMEOUT_CMDSTAT  2

//--- THETA API URLs ---
const char  sUrlInfo[]        = "/osc/info" ;
const char  sUrlState[]       = "/osc/state" ;
const char  sUrlChkForUp[]    = "/osc/checkForUpdates" ;
const char  sUrlCmdExe[]      = "/osc/commands/execute" ;
const char  sUrlCmdStat[]     = "/osc/commands/status" ;

//--- Internal Info  ---
#define   TAKE_PIC_STAT_DONE  0
#define   TAKE_PIC_STAT_BUSY  1

#define   INT_EXP_OFF         0
#define   INT_EXP_ON          1

#define   PUSH_CMD_CNT_INTEXP 3

#define   INT_EXP_STAT_STOP   0
#define   INT_EXP_STAT_RUN    1

#define   MOVE_STAT_STOP      0
#define   MOVE_STAT_REC       1

#define   STATUS_CHK_CYCLE    20
#define   CHK_BUSY_END        0     //BUSY
#define   CHK_CAPMODE         0     //NO BUSY
#define   CHK_MOVESTAT        10    //NO BUSY

int     iRelease          = 0;
int     iTakePicStat      = TAKE_PIC_STAT_DONE;
int     iIntExpOnOff      = INT_EXP_OFF;          //For expansion
int     iIntExpStat       = INT_EXP_STAT_STOP;
int     iMoveStat         = MOVE_STAT_STOP;
int     iStatChkCnt       = 0;
String  strTakePicLastId  = "0";
String  strSSID           = "SID_0000";
String  strCaptureMode    = "";

#define   LED_BLINK_CYCLE     10
int     iLed1BlinkStat    = 0;
int     iLed1BlinkCycleCnt= LED_BLINK_CYCLE;


//===============================
// User define function prototype
//===============================
void    INT_ReleaseSw(void);

void    BlinkLed1(void);

int     ConnectTHETA(void);
int     SearchAndEnterTHETA(void);
int     CheckThetaSsid( const char* pcSsid );
void    SaveThetaSsid(char* pcSsid);
void    ReadThetaSsid(char* pcSsid);
void    SetThetaSsidToPassword(char* pcSsid, char* pcPass);

String  SimpleHttpProtocol(const char* sPostGet, char* sUrl, char* sHost, int iPort, String strData, unsigned int uiTimeoutSec);

int     ThetaAPI_Get_Info(void);
int     ThetaAPI_Post_State(void);
int     ThetaAPI_Post_startSession(void);
int     ThetaAPI_Post_takePicture(void);
int     ThetaAPI_Post__startCapture(void);
int     ThetaAPI_Post__stopCapture(void);
int     ThetaAPI_Post_getOptions(void);
int     ThetaAPI_Post_commnads_status(void);


//===============================
// setup
//===============================
void setup() {
  // initialize serial and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  Serial.println("");
  Serial.println("");
  Serial.println("-----------------------------------------");
  Serial.println("  RICOH THETA S Remote Control Software  ");
  Serial.println("          Single Button Edition          ");
  Serial.println("                   for                   ");
  Serial.println("  Switch Science ESP-WROOM-02 Dev.board  ");
  Serial.println("             FW Ver : " + String(sThetaRemoteVersion) );
  Serial.println("-----------------------------------------");
  Serial.println("");
  
  // set the digital pin as output:
  pinMode(led1Pin, OUTPUT);
  digitalWrite(led1Pin, LOW);
  pinMode(led2Pin, OUTPUT);
  digitalWrite(led2Pin, LOW);
  
  // initialize the pushbutton pin as an input:
  pinMode(buttonPin, INPUT);
  attachInterrupt( buttonPin, INT_ReleaseSw, FALLING);
  
  // Define EEPROM SIZE
  EEPROM.begin(512);    //Max 4096Byte

  //Read TEHTA SSID from EEPROM & check
  char SsidBuff[THETA_SSID_BYTE_LEN+1];
  ReadThetaSsid(SsidBuff);  
  if ( CheckThetaSsid( (const char*)SsidBuff ) == 1 ) {
    memcpy( ssid, SsidBuff, THETA_SSID_BYTE_LEN );
    SetThetaSsidToPassword(SsidBuff, password);
    Serial.println("");
    Serial.print("EEPROM SSID=");
    Serial.println(ssid);
    Serial.print("       PASS=");
    Serial.println(password);
  } else {
    iConnectOrScan = WIFI_SCAN_THETA;
  }
  
}

//===============================
// main loop
//===============================
void loop() {
  
  if ( WiFi.status() != WL_CONNECTED ) {
    //LED2
    digitalWrite(led2Pin, HIGH);
    
    if (iConnectOrScan == WIFI_CONNECT_THETA) {
      //LED1
      digitalWrite(led1Pin, LOW);
      
      iConnectOrScan = ConnectTHETA();
      if ( iConnectOrScan == WIFI_CONNECT_THETA ) {
        ThetaAPI_Get_Info();
        ThetaAPI_Post_State();
        if ( strSSID.equals("SID_0000") ) {
          ThetaAPI_Post_startSession();
        }
        iRelease = 0;
      }
    } else {
      //LED1
      digitalWrite(led1Pin, HIGH);
      
      if( SearchAndEnterTHETA() == 1 ) {
        ReadThetaSsid(ssid);  
        SetThetaSsidToPassword(ssid, password);
        iConnectOrScan = WIFI_CONNECT_THETA;
      } else {
        iConnectOrScan = WIFI_SCAN_THETA;
      }
    }
  } else {
    //LED2
    digitalWrite(led2Pin, LOW);
    
    if ( iRelease == 1 ) {
      //LED1
      digitalWrite(led1Pin, HIGH);
      
      //ThetaAPI_Post_getOptions();             //A reaction becomes dull, so it's eliminated.
      
      if ( strCaptureMode.equals("image") )  {
        if ( iIntExpOnOff == INT_EXP_OFF ) {
          ThetaAPI_Post_takePicture();
        } else {
          if ( iIntExpStat == INT_EXP_STAT_STOP ) {
            ThetaAPI_Post__startCapture();
            iIntExpStat = INT_EXP_STAT_RUN;
          } else {
            ThetaAPI_Post__stopCapture();
            iIntExpStat = INT_EXP_STAT_STOP;
            iIntExpOnOff = INT_EXP_OFF;         //When expanding, it's eliminated.
          }
        }
      } else if ( strCaptureMode.equals("_video") ) {
        ThetaAPI_Post_State();
        if ( iMoveStat == MOVE_STAT_STOP ) {
          iMoveStat = MOVE_STAT_REC;
          ThetaAPI_Post__startCapture();
        } else {
          iMoveStat = MOVE_STAT_STOP;
          ThetaAPI_Post__stopCapture();
        }
      } else {
        Serial.println("captureMode failed : [" + strCaptureMode + "]");
      }
      
      delay(1000);                              //To avoid release of the fast cycle.
      iRelease = 0;
    } else {
      //LED1
      if ( (iTakePicStat == TAKE_PIC_STAT_BUSY) || (iMoveStat == MOVE_STAT_REC) || (iIntExpStat == INT_EXP_STAT_RUN) ) {
        digitalWrite(led1Pin, HIGH);
      } else {
        if ( iIntExpOnOff == INT_EXP_ON ) {
          BlinkLed1();
        } else {
          digitalWrite(led1Pin, LOW);
        }
      }
      
      iStatChkCnt++;
      if (iStatChkCnt >= STATUS_CHK_CYCLE){
        iStatChkCnt=0;
      }
      if (iTakePicStat == TAKE_PIC_STAT_BUSY) {
        if ( iStatChkCnt == CHK_BUSY_END ) {
          ThetaAPI_Post_commnads_status();
        } else {
          delay(10);
        }
      } else {
        if ( iStatChkCnt == CHK_CAPMODE ) {
          ThetaAPI_Post_getOptions();           //When doing just before release, it's unnecessary.
        } else if ( iStatChkCnt == CHK_MOVESTAT ) {
          ThetaAPI_Post_State();
        } else {
          delay(10);
        }
      }
    }
  }
}

//===============================
// User Define function
//===============================
//-------------------------------------------
// Interrupt handler
//-------------------------------------------
void INT_ReleaseSw(void)
{
  iRelease =1;
}

//-------------------------------------------
// LED1 Blink Control
//-------------------------------------------
void    BlinkLed1(void)
{
  iLed1BlinkCycleCnt++;
  if (iLed1BlinkCycleCnt >= LED_BLINK_CYCLE ) {
    iLed1BlinkCycleCnt=0;
    
    if ( iLed1BlinkStat == 0 ) {
      iLed1BlinkStat = 1;
      digitalWrite(led1Pin, HIGH);
    } else {
      iLed1BlinkStat = 0;
      digitalWrite(led1Pin, LOW);
    }
  }
  return;
}

//-------------------------------------------
// Wi-Fi Connect functions
//-------------------------------------------
int ConnectTHETA(void)
{
  int iRet = WIFI_CONNECT_THETA;
  int iButtonState = 0;
  int iButtonCnt = 0;
  int iPushCnt = 0;

  Serial.println("");
  Serial.println("WiFi disconnected");
  iRelease = 0;

  iButtonCnt=0;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    iButtonState = digitalRead(buttonPin);
    if ( iButtonState == LOW ) {
      iButtonCnt++;
      if ( iButtonCnt >= 10 ) {
        iRet=WIFI_SCAN_THETA;
        break;
      }
    } else {
      iButtonCnt=0;
    }
    if ( iRelease == 1 ) {
       iRelease = 0;
       iPushCnt++;
    }
    delay(500);
  }

  if ( iRet != WIFI_SCAN_THETA ) {
    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    Serial.println("");
    if ( iPushCnt >= PUSH_CMD_CNT_INTEXP ) {
      iIntExpOnOff = INT_EXP_ON;
      Serial.println("1 Period Interval Exp ON ! : PushCnt=" + String(iPushCnt));
    } else {
      iIntExpOnOff = INT_EXP_OFF;
      Serial.println("1 Period Interval Exp OFF  : PushCnt=" + String(iPushCnt));
    }
    Serial.println("");
  }
  
  return iRet;
}
//----------------
#define SEARCH_MAX_NUM 20
int SearchAndEnterTHETA(void)
{
  int iRet = 0;
  int iThetaCnt=0;
  int aiSsidPosList[SEARCH_MAX_NUM];
  char ssidbuf[256];
  
  Serial.println("");
  Serial.println("Search THETA");
  
  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(") ");
      Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");

      if( WiFi.RSSI(i) >= NEAR_RSSI_THRESHOLD ) {
        WiFi.SSID(i).toCharArray(ssidbuf, sizeof(ssidbuf));
        if ( CheckThetaSsid(ssidbuf) == 1 ) {
          if ( iThetaCnt < SEARCH_MAX_NUM ) {
            aiSsidPosList[iThetaCnt]=i;
            iThetaCnt++;
          }
        }
      }
      delay(10);
    }
    
    if (iThetaCnt>0) {
      iRet = 1;
      
      //Select Max RSSI THETA
      int iRssiMaxPos=0;
      for (int i = 0; i <iThetaCnt; i++) {
        if( WiFi.RSSI(aiSsidPosList[iRssiMaxPos]) < WiFi.RSSI(aiSsidPosList[i]) ) {
          iRssiMaxPos = i;
        }
      }
      //Enter THETA SSID to EEPROM
      WiFi.SSID(aiSsidPosList[iRssiMaxPos]).toCharArray(ssidbuf, sizeof(ssidbuf));
      SaveThetaSsid(ssidbuf);
      Serial.println("");
      Serial.println("--- Detected TEHTA ---");
      Serial.print("SSID=");
      Serial.print(WiFi.SSID(aiSsidPosList[iRssiMaxPos]));
      Serial.print(", RSSI=");
      Serial.print(WiFi.RSSI(aiSsidPosList[iRssiMaxPos]));
      Serial.println("[db]");
    }
  }
  Serial.println("");
  
  return iRet;
}
//----------------
int CheckThetaSsid( const char* pcSsid )
{
  int iRet=1;

  String strSsid = String(pcSsid);
  if ( strSsid.length() == THETA_SSID_BYTE_LEN ) {
    if ( ( strSsid.startsWith("THETAXS") == true ) &&
         ( strSsid.endsWith(".OSC") == true )      ){
      //Serial.print("pass=");
      for (int j=THETA_PASS_OFFSET; j<(THETA_PASS_OFFSET+THETA_PASS_BYTE_LEN); j++) {
        //Serial.print( *( pcSsid + j ) );
        if (  (*( pcSsid + j )<0x30) || (0x39<*( pcSsid + j ))  ) {
          iRet=0;
        }
      }
      Serial.println("");
    } else {
      iRet = 0;
    }
  } else {
    iRet = 0;
  }

  return iRet;  
}
//----------------
void SaveThetaSsid(char* pcSsid)
{
  for (int iCnt=0; iCnt<THETA_SSID_BYTE_LEN; iCnt++) {
     EEPROM.write( (THETA_SSID_TOP_ADR + iCnt), *(pcSsid+iCnt) );
  }
  EEPROM.commit();
  delay(100);
  
  return ;
}
//----------------
void ReadThetaSsid(char* pcSsid)
{
  for (int iCnt=0; iCnt<THETA_SSID_BYTE_LEN; iCnt++) {
    //Read EEPROM
    *(pcSsid+iCnt) = EEPROM.read(THETA_SSID_TOP_ADR + iCnt);
  }
  *(pcSsid+THETA_SSID_BYTE_LEN) = 0x00;

  return ;
}
//----------------
void SetThetaSsidToPassword(char* pcSsid, char* pcPass)
{
  for (int iCnt=0; iCnt<THETA_PASS_BYTE_LEN; iCnt++) {
    //Read EEPROM
    *(pcPass+iCnt) = *(pcSsid+THETA_PASS_OFFSET+iCnt);
  }
  *(pcPass+THETA_PASS_BYTE_LEN) = 0x00;
  
  return ;
}

//-------------------------------------------
// HTTP protocol functions
//-------------------------------------------
String SimpleHttpProtocol(const char* sPostGet, char* sUrl, char* sHost, int iPort, String strData, unsigned int uiTimeoutSec)
{
  unsigned long ulStartMs;
  unsigned long ulCurMs;
  unsigned long ulElapsedMs;
  unsigned long ulTimeoutMs;
  int           iDelimiter=0;
  int           iCheckLen=0;
  int           iResponse=0;
  String        strJson = String("") ;

  ulTimeoutMs = (unsigned long)uiTimeoutSec * 1000;
  client.flush();   //clear response buffer

  //Use WiFiClient class to create TCP connections
  if (!client.connect(sHost, iHttpPort)) {
    Serial.println("connection failed");

    delay(1000);
    WiFi.disconnect();
    return strJson;
  }
  
  //Send Request
  client.print(String(sPostGet) + " " + sUrl + " HTTP/1.1\r\n" +
               "Host: " + String(sHost) + ":" + String(iHttpPort)  + "\r\n" + 
               "Content-Type: application/json;charset=utf-8\r\n" + 
               "Accept: application/json\r\n" + 
               "Content-Length: " + String(strData.length()) + "\r\n" + 
               "Connection: close\r\n" + 
               "\r\n");
  if ( strData.length() != 0 ) {
    client.print(strData + "\r\n");
  }
  
  //Read Response
  ulStartMs = millis();
  while(1){
    delay(10);

    String line;
    while(client.available()){
      iResponse=1;
      #if 0  //-----------------------------------------------------
      line = client.readStringUntil('\r'); //This function is a defect.
      #else  //-----------------------------------------------------
      //Serial.println("res size=" + String(client.available()));
      char sBuf[client.available()];
      int iPos=0;
      while (client.available()) {
        char c = client.read();
        if (  c == '\r' ) {
          break;
        } else {
          sBuf[iPos] = c;
          iPos++;
        }
      }
      sBuf[iPos] = 0x00;
      line = String(sBuf);
      #endif //-----------------------------------------------------
      line.trim() ;
      //Serial.println("[" + line + "]");
      
      if (iDelimiter==1) {
        strJson = line;
      }
      if (line.length() == 0) {
        iDelimiter = 1;
      }
      if (line.indexOf("Content-Length:") >=0 ) {
        line.replace("Content-Length:","");
        if ( line.length() < 11 ) {
          char sBuff[11];
          line.toCharArray(sBuff, line.length()+1);
          iCheckLen = atoi(sBuff);
        } else {
          iCheckLen = -1;
        }
        //Serial.println("len=[" + String(iCheckLen) + "]");
      }
    }
    
    ulCurMs = millis();
    if ( ulCurMs > ulStartMs ) {
      ulElapsedMs = ulCurMs - ulStartMs;
    } else {
      ulElapsedMs = (4294967295 - ulStartMs) + ulCurMs + 1 ;
    }
    if ( (ulTimeoutMs!=0) && (ulElapsedMs>ulTimeoutMs) ) {
      break;
    }
    if ( (iResponse==1) && (strJson.length()!=0) ) {
      break;
    }
  }
  
  //Check Response
  if ( iCheckLen != strJson.length() ) {
    Serial.println("receive error : JSON[" + strJson + "], Content-Length:" + String(iCheckLen));
    strJson = String("");
  }
  //Serial.println( "time=" + String(ulElapsedMs) + "[ms]");

  return strJson;
}

//-------------------------------------------
// THETA API functions
//-------------------------------------------
int ThetaAPI_Get_Info(void)
{
  int iRet=0;
  
  String strSendData = String("");
  String strJson = SimpleHttpProtocol("GET", (char*)sUrlInfo, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_NORMAL );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<350> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Get_Info() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sModel = root["model"];
      const char* sSN    = root["serialNumber"];
      const char* sFwVer = root["firmwareVersion"];
      Serial.println("ThetaAPI_Get_Info() : Model[" + String(sModel) + "], S/N[" + String(sSN) + "], FW Ver[" + String(sFwVer) + "]");
      iRet=1;
    }
  }
  return iRet;
}
//----------------
int ThetaAPI_Post_State(void)
{
  int iRet=0;
  
  String strSendData = String("");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlState, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_STATE );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<350> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post_State() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sessionId       = root["state"]["sessionId"];
      const char* batteryLevel    = root["state"]["batteryLevel"];
      const char* _captureStatus  = root["state"]["_captureStatus"];
      const char* _recordedTime   = root["state"]["_recordedTime"];
      const char* _recordableTime = root["state"]["_recordableTime"];
      Serial.println("ThetaAPI_Post_State() : sessionId[" + String(sessionId) + "], batteryLevel[" + String(batteryLevel) +
                      "], _captureStatus[" + String(_captureStatus) + 
                      "], _recordedTime[" + String(_recordedTime) + "], _recordableTime[" + String(_recordableTime) + "]");
      
      strSSID = String(sessionId);
      
      String strCaptureStatus  = String(_captureStatus);
      String strRecordedTime   = String(_recordedTime);
      String strRecordableTime = String(_recordableTime);
      
      if ( strCaptureStatus.equals("idle") ) {
        iMoveStat = MOVE_STAT_STOP;
        iIntExpStat = INT_EXP_STAT_STOP;
      } else {
        if ( strRecordedTime.equals("0") && strRecordableTime.equals("0") ) {
          iMoveStat = MOVE_STAT_STOP;
          iIntExpOnOff= INT_EXP_ON;
          iIntExpStat = INT_EXP_STAT_RUN ;
        } else {
          iMoveStat = MOVE_STAT_REC;
          iIntExpStat = INT_EXP_STAT_STOP;
        }
      }
      iRet=1;
    }
  }
  return iRet;
}
//----------------
int ThetaAPI_Post_startSession(void)
{
  int iRet=0;
  
  String strSendData = String("{\"name\": \"camera.startSession\",  \"parameters\": {\"timeout\": 180}}");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdExe, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_NORMAL );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post_startSession() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sSessionId  = root["results"]["sessionId"];
      strSSID = String(sSessionId);
      Serial.println("ThetaAPI_Post_startSession() : sessionId[" + strSSID + "]" );
      iRet=1;
    }
  }
  return iRet;
}
//----------------
int ThetaAPI_Post_takePicture(void)
{
  int iRet=0;
  
  iTakePicStat = TAKE_PIC_STAT_BUSY;
  
  String strSendData = String("{\"name\": \"camera.takePicture\", \"parameters\": { \"sessionId\": \"" + strSSID + "\" } }");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdExe, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_NORMAL );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post_takePicture() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      Serial.print("ThetaAPI_Post_takePicture() : state[" + strState + "], " );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        Serial.println("Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else {  //inProgress
        const char* sId = root["id"];
        strTakePicLastId = String(sId);
        Serial.println("id[" + strTakePicLastId + "]");
        iRet=1;
      }
    }
  }
  return iRet;
}
//----------------
int ThetaAPI_Post__startCapture(void)
{
  int iRet=0;

  String strSendData = String("{\"name\": \"camera._startCapture\", \"parameters\": { \"sessionId\": \"" + strSSID + "\" } }");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdExe, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_STARTCAP );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post__startCapture() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      Serial.print("ThetaAPI_Post__startCapture() : state[" + strState + "]" );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        Serial.println(", Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else {  //done
        Serial.println("");
        iRet=1;
      }
    }
  }
  
  return iRet;
}
//----------------
int ThetaAPI_Post__stopCapture(void)
{
  int iRet=0;
  
  String strSendData = String("{\"name\": \"camera._stopCapture\", \"parameters\": { \"sessionId\": \"" + strSSID + "\" } }");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdExe, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_STOPCAP );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post__stopCapture() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      Serial.print("ThetaAPI_Post__stopCapture() : state[" + strState + "]" );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        Serial.println(", Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else {  //done
        Serial.println("");
        iRet=1;
      }
    }
  }
  
  return iRet;
}
//----------------
int     ThetaAPI_Post_getOptions(void)
{
  int iRet=0;
  
  String strSendData = String("{\"name\": \"camera.getOptions\", \"parameters\": { \"sessionId\": \"" + strSSID + "\", \"optionNames\": [\"captureMode\"] } }");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdExe, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_NORMAL );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );

  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post_getOptions() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      Serial.print("ThetaAPI_Post_getOptions() : state[" + strState + "], " );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        Serial.println("Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else {  //done
        const char* sCaptureMode = root["results"]["options"]["captureMode"];
        strCaptureMode = String(sCaptureMode);
        Serial.println("captureMod[" +strCaptureMode + "]");
        iRet=1;
      }
    }
  }
  
  return iRet;
}
//----------------
int     ThetaAPI_Post_commnads_status(void)
{
  int iRet=0;
  
  String strSendData = String("{\"id\":\"" + strTakePicLastId + "\"}");
  String strJson = SimpleHttpProtocol("POST", (char*)sUrlCmdStat, (char*)sHost, iHttpPort, strSendData, HTTP_TIMEOUT_CMDSTAT );
  //Serial.println("JSON:[" + strJson + "], len=" + strJson.length() );

  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      Serial.println("ThetaAPI_Post_commnads_status() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      Serial.print("ThetaAPI_Post_commnads_status() : state[" + strState + "]" );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        Serial.println(", Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else if ( strState.equals("done") ) {
        const char* sFileUri = root["results"]["fileUri"];
        Serial.println(", fileUri[" + String(sFileUri) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;        
        iRet=1;
      } else {  // inProgress
        const char* sId = root["id"];
        Serial.println(", id[" + String(sId) + "]");
        iRet=1;
      }
    }
  }
  
  return iRet;
}
