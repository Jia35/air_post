// 引入linkit 7697的Timer、wifi函式庫及DHT函式庫
#include "LTimer.h"
#include "LWiFi.h"
#include "DHT.h"

#define START_1 0x42
#define START_2 0x4d
#define DATA_LENGTH_H        0
#define DATA_LENGTH_L        1
#define PM1_0_CF1_H          2
#define PM1_0_CF1_L          3
#define PM2_5_CF1_H          4
#define PM2_5_CF1_L          5
#define PM10_CF1_H           6
#define PM10_CF1_L           7
#define PM1_0_ATMOSPHERE_H   8
#define PM1_0_ATMOSPHERE_L   9
#define PM2_5_ATMOSPHERE_H   10
#define PM2_5_ATMOSPHERE_L   11
#define PM10_ATMOSPHERE_H    12
#define PM10_ATMOSPHERE_L    13
#define UM0_3_H              14
#define UM0_3_L              15
#define UM0_5_H              16
#define UM0_5_L              17
#define UM1_0_H              18
#define UM1_0_L              19
#define UM2_5_H              20
#define UM2_5_L              21
#define UM5_0_H              22
#define UM5_0_L              23
#define UM10_H               24
#define UM10_L               25
#define VERSION              26
#define ERROR_CODE           27
#define CHECKSUM             29

#define DHT_pin 2
#define DHT_type DHT22

// PMSA003 RX_pin = 6; TX_pin = 7;
const int TGS2602_pin = 14;
const int MQ9_pin = 16;
const int MQ135_pin = 17;
const int Btn_pin = 6, Led_pin = 7;
const float RL_voc = 4.7, RO_voc = 60;
int btnState = 0;

char ssid[] = "ES715_TP_2.4G";
char pass[] = "********";
// nodejs伺服器ip
String host = "140.125.32.146";
// nodejs伺服器port
uint16_t port = 11715;
// POST到的url
String postUrl = "/upload_air/";

// 使用POST輸入JSON格式的字串
String postJson = "";

// 連線網路狀態
int status = WL_IDLE_STATUS;
// 初始化網路客戶端類別物件
WiFiClient client;

LTimer timer0(LTIMER_0);

unsigned char chrRecv;
byte bytCount = 0;
int voc_adc = 0;
float h=0, t=0, voc=0;
float h_voc, t_voc;
float h_arr[5], t_arr[5], voc_arr[5];
float h_pick=0, t_pick=0, voc_pick=0;
unsigned int PM1_ATMO=0, PM25_ATMO=0, PM10_ATMO=0, mq135=0, mq9=0;
unsigned int PM1_ATMO_arr[5], PM25_ATMO_arr[5], PM10_ATMO_arr[5], mq135_arr[5], mq9_arr[5];
unsigned int PM1_ATMO_pick=0, PM25_ATMO_pick=0, PM10_ATMO_pick=0, mq135_pick=0, mq9_pick=0;
int flagNum = 0;
bool isDHT = true, DHTstatus = true;

DHT dht(DHT_pin, DHT_type);

void setup()
{
    Serial.begin(9600);
    while (!Serial);

    // 試圖連線到wifi裝置
    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        status = WiFi.begin(ssid, pass);
    }
    Serial.println("Connected to wifi");
    printWifiStatus();
    
    pinMode(Btn_pin, INPUT);
	pinMode(Led_pin, OUTPUT);
    // 等待使用者按下按鈕 或 開機後超過兩分鐘
	Serial.println("Waiting for the start button to press.");
	while(true) {
		btnState = digitalRead(Btn_pin);
		if(millis() > 120000 || btnState)
			break;
		delay(100);
	}
	digitalWrite(LED_BUILTIN, HIGH);
	delay(500);
	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
	digitalWrite(LED_BUILTIN, HIGH);
	delay(500);
	digitalWrite(LED_BUILTIN, LOW);
	delay(200);
    
    dht.begin();
    // 判斷DHT是否正常運作
	h = dht.readHumidity();
    t = dht.readTemperature();
    if (isnan(t) || isnan(h))
        Serial.println("Please check the DHT pins.");
	// 等待DHT回復正常運作
    while(true) {
		h = dht.readHumidity();
	    t = dht.readTemperature();
        if (!isnan(t) || !isnan(h))
	        break;
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(600);
	}

	Serial.println("Start collecting data.");
    Serial1.begin(9600);
    delay(3000);
    getPMData();
    timer0.begin();
    timer0.start(10000, LTIMER_REPEAT_MODE, _getAirData, NULL);
}

void loop()
{
    // 如果伺服器有回應(response)，則印出資料
    while (client.available()) {
        char c = client.read();
        Serial.write(c);
    }
    
	if(flagNum == 6) {
        dataSelect();
        // 使用POST請求資料到伺服器
        postData();
        flagNum = 0;
    }

	if(isDHT==true) {
	    h = dht.readHumidity();
	    t = dht.readTemperature();
		isDHT = false;
        DHTstatus = true;
	    if (isnan(t) || isnan(h)) {
	        Serial.println("Failed to read from DHT");
	        h=0, t=0;
            DHTstatus = false;
	    }
    }
	delay(100);
	    
}

// callback function for timer0
void _getAirData(void *usr_data)
{
    if(flagNum < 5) {
        // 取得各項感測器數據資料
    	isDHT=true;
        getPMData();
        voc_adc = analogRead(TGS2602_pin);
        voc = adcToSrr_voc(voc_adc);
        mq9 = analogRead(MQ9_pin);
        mq135 = analogRead(MQ135_pin);
        Serial.println();
        Serial.println("--------------------");
        Serial.print("Humidity: ");Serial.println(h);
        Serial.print("Temperature: ");Serial.println(t);
        Serial.print("PM1_ATMO: ");Serial.println(PM1_ATMO);
        Serial.print("PM25_ATMO: ");Serial.println(PM25_ATMO);
        Serial.print("PM10_ATMO: ");Serial.println(PM10_ATMO);
        Serial.print("TGS2602_SRR: ");Serial.println(voc);
        Serial.print("MQ9: ");Serial.println(mq9);
        Serial.print("MQ135: ");Serial.println(mq135);

        // 各項數據資料存入各自陣列
        h_arr[flagNum] = h;
        t_arr[flagNum] = t;
        PM1_ATMO_arr[flagNum] = PM1_ATMO;
        PM25_ATMO_arr[flagNum] = PM25_ATMO;
        PM10_ATMO_arr[flagNum] = PM10_ATMO;
        voc_arr[flagNum] = voc;
        mq9_arr[flagNum] = mq9;
        mq135_arr[flagNum] = mq135;
    }
    flagNum++;
}

/* 排序挑選準備上傳之資料 */
void dataSelect()
{
    arrSort_f(h_arr, 5);
    arrSort_f(t_arr, 5);
    arrSort(PM1_ATMO_arr, 5);
    arrSort(PM25_ATMO_arr, 5);
    arrSort(PM10_ATMO_arr, 5);
    arrSort_f(voc_arr, 5);
    arrSort(mq9_arr, 5);
    arrSort(mq135_arr, 5);

    // 挑選方法採用中位數
    h_pick = h_arr[2];
    t_pick = t_arr[2];
    PM1_ATMO_pick = PM1_ATMO_arr[2];
    PM25_ATMO_pick = PM25_ATMO_arr[2];
    PM10_ATMO_pick = PM10_ATMO_arr[2];
    voc_pick = voc_arr[2];
    mq9_pick = mq9_arr[2];
    mq135_pick = mq135_arr[2];
}

/* 陣列排序 - 正整數 */
void arrSort(unsigned int *arr, byte aSize)
{
    unsigned int tempData;
    aSize-=1;
    for(int i=0; i<aSize; i++) {
        for(int j=0; j<(aSize-i); j++) {
            if(arr[j] > arr[j+1]) {
                tempData = arr[j+1];
                arr[j+1] = arr[j];
                arr[j] = tempData;
            }
        }
    }
}

/* (目前沒使用) 陣列挑選 - 正整數，採用去掉前後一位剩餘平均 */
unsigned int arrPick(unsigned int *arr, byte aSize)
{
    unsigned int tempData = 0;
    for(int i=1; i<aSize-1; i++) {
        tempData += arr[i];
    }
    return tempData/(aSize-2);
}

/* 陣列排序 - 浮點數 */
void arrSort_f(float *arr, byte aSize)
{
    float tempData;
    aSize-=1;
    for(int i=0; i<aSize; i++) {
        for(int j=0; j<(aSize-i); j++) {
            if(arr[j] > arr[j+1]) {
                tempData = arr[j+1];
                arr[j+1] = arr[j];
                arr[j] = tempData;
            }
        }
    }
}

/* (目前沒使用) 陣列挑選 - 浮點數，採用去掉前後一位剩餘平均 */
float arrPick_f(float *arr, byte aSize)
{
    float tempData = 0;
    for(int i=1; i<aSize-1; i++) {
        tempData += arr[i];
    }
    return tempData/(aSize-2);
}

/* 輸出wifi狀態 */
void printWifiStatus()
{
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
    Serial.println();
}

// 使用POST請求資料到伺服器
void postData()
{
    Serial.println("\nStarting connection to server...");
    // 如果有連線，則開始傳出http的請求
    if (client.connect(host.c_str(), port)) {
        // 傳出的Json格式字串
        postJson = "{ \"H\":" + (String)h_pick + ",\"T\":" + (String)t_pick +
					",\"PM1_ATMO\":" + (String)PM1_ATMO_pick + ",\"PM25_ATMO\":" + 
                    (String)PM25_ATMO_pick + ",\"PM10_ATMO\":" + (String)PM10_ATMO_pick +
                    ",\"VOC\":" + (String)voc_pick + 
                    ",\"MQ9\":" + (String)mq9_pick + ",\"MQ135\":" + (String)mq135_pick + "}";

        Serial.println("connected to server (POST)");

        // 開始http請求
        // headers
        client.println("POST " + postUrl + " HTTP/1.1");
        client.println("Host: " + host + ":" + (String)port);
        client.println("Content-Type: application/json; charset=utf-8");
        client.println("Content-Length: " + (String)postJson.length());
        //client.println("Connection: keep-alive");
        //client.println("Accept: */*");
        client.println();

        // body
        client.println(postJson);
        client.println();

        delay(10);
    }
}

/* TGS2602 類比值轉SRR 及溫溼度修正 */
float adcToSrr_voc(int temp_voc)
{
    if(DHTstatus) {
        h_voc = h;
        t_voc = t;
    }
    float temp_ssr = ((8192-2*(float)temp_voc)*RL_voc)/((float)temp_voc*RO_voc);
    float h40_dir, h65_dir, h85_dir;
    float dir_arr[3] = {abs(h_voc-40), abs(h_voc-65), abs(h_voc-85)};
    int index = 0, intMin = 100;
    float tgs2602_reg[3][3] = {
        {0.0214, -0.3626, 1.59},
        {0.0229, -0.3851, 1.688},
        {0.0286, -0.4354, 1.834}
    };
    float srr_correct = 0;

    for(int i=0; i<3; i++) {
        if(dir_arr[i] < intMin) {
            intMin=dir_arr[i];
            index = i;
        }
    }
    srr_correct = tgs2602_reg[index][0]*(t_voc/10)*(t_voc/10) +
					tgs2602_reg[index][1]*(t_voc/10) + tgs2602_reg[index][2];
    return temp_ssr/srr_correct;
}

/* PMSA003 取得pm1.0 pm2.5 pm10濃度 */
void getPMData()
{
    unsigned char chrData[30];

    if (Serial1.available()) {
        chrRecv = Serial1.read();

        if (chrRecv == START_1 && bytCount == 0)  bytCount = 1;
        if (chrRecv == START_2 && bytCount == 1)  bytCount = 2;

        if (bytCount == 2) {
            bytCount = 0;

            for(int i = 0; i < 30; i++) {
                chrData[i] = Serial1.read();
            }
        // 空氣中主要汙物為等效顆粒進行密度換算，適用於普通室內外大氣環境
        PM1_ATMO = analysisPmData(chrData, PM1_0_ATMOSPHERE_H, PM1_0_ATMOSPHERE_L);
        PM25_ATMO = analysisPmData(chrData, PM2_5_ATMOSPHERE_H, PM2_5_ATMOSPHERE_L);
        PM10_ATMO = analysisPmData(chrData, PM10_ATMOSPHERE_H, PM10_ATMOSPHERE_L);
      }

   }
}

unsigned int analysisPmData(unsigned char chrSrc[], byte bytHigh, byte bytLow)
{
   return (chrSrc[bytHigh] << 8) + chrSrc[bytLow];
}
