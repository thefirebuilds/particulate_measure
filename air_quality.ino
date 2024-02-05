#include <ESP8266WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// Network SSID
const char* ssid = ""; //might not be my real SSID
const char* password = ""; //probably not my actual password

#include <SoftwareSerial.h>
SoftwareSerial pmsSerial(2, 3);

const char* host1 = "mywebsite.com"; //probably not my actual site
const char* host2 = "mywebsite.com";
const int httpsPort1 = 443;
const int httpsPort2 = 443;
String motion;

// Use WiFiClientSecure class to create TLS connection
void sendDataUrl(String strPpm25) {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  client.setInsecure();
  Serial.print("connecting to ");
  Serial.println(host1);

  //Serial.printf("Using fingerprint '%s'\n", fingerprint);
  //client.setFingerprint(fingerprint);

  if (!client.connect(host1, httpsPort1)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/aqi.php?strAqi=" + strPpm25;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host1 + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
  Serial.println("Sending GET request...");
  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed - ");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
  client.stop();
}
///HTTPS

// Set up your InfluxDB details here

// InfluxDB v2 server url (Use: InfluxDB UI -> Load Data -> Client Libraries)
#define INFLUXDB_URL ""

// InfluxDB v2 server or cloud API authentication token (Use: InfluxDB UI -> Data -> Tokens -> <select token>)
#define INFLUXDB_TOKEN ""

// InfluxDB v2 organization id (Use: InfluxDB UI -> User -> About -> Common Ids )
#define INFLUXDB_ORG ""

// InfluxDB v2 bucket name (Use: InfluxDB UI -> Data -> Buckets)
#define INFLUXDB_BUCKET "Particulate"

// Set timezone string according to <https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html>
#define TZ_INFO "UTC-6"


// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Create your Data Point here
Point sensor("Air_Quality");

void setup() {
  // our debugging output
  Serial.begin(115200);
 
  // sensor baud rate is 9600
  pmsSerial.begin(9600);

  ///Wifi
  Serial.begin(115200);
  delay(10);
 
  // Connect WiFi
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.hostname("Wemos1");
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
 
  // Print the IP address
  Serial.print("IP address: ");
  Serial.print(WiFi.localIP());
  //Wifi
timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

 // Check server connection
    if (client.validateConnection()) {
      Serial.print("Connected to InfluxDB: ");
      Serial.println(client.getServerUrl());
    } else {
      Serial.print("InfluxDB connection failed: ");
      Serial.println(client.getLastErrorMessage());
    }
}

struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};
 
struct pms5003data data;
int eventCount = 0; //counts the loop, which iterates every second
int ppm25 = 0;
int avgPpm25 = 0;
//600 seconds = 10mins - I want to calc an average every 10 mins
int secsToPost = 600; //number of seconds to wait - also the divisor for avg
String urlVars; //will hold the GET values sent to my website

void loop() {
  if (readPMSdata(&pmsSerial)) {
    // reading data was successful!
    Serial.println();
    Serial.println("---------------------------------------");
    Serial.println("Concentration Units (standard)");
    Serial.print("PM 1.0: "); Serial.print(data.pm10_standard);
    Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_standard);
    Serial.print("\t\tPM 10: "); Serial.println(data.pm100_standard);
    Serial.println("---------------------------------------");
    Serial.println("Concentration Units (environmental)");
    Serial.print("PM 1.0: "); Serial.print(data.pm10_env);
    Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_env);
    Serial.print("\t\tPM 10: "); Serial.println(data.pm100_env);
    Serial.println("---------------------------------------");
    Serial.print("Particles > 0.3um / 0.1L air:"); Serial.println(data.particles_03um);
    Serial.print("Particles > 0.5um / 0.1L air:"); Serial.println(data.particles_05um);
    Serial.print("Particles > 1.0um / 0.1L air:"); Serial.println(data.particles_10um);
    Serial.print("Particles > 2.5um / 0.1L air:"); Serial.println(data.particles_25um);
    Serial.print("Particles > 5.0um / 0.1L air:"); Serial.println(data.particles_50um);
    Serial.print("Particles > 10.0 um / 0.1L air:"); Serial.println(data.particles_100um);
    eventCount++;
    Serial.println("-------------------" + String(eventCount) + "--------------------");

    //write to influxx
    sensor.clearFields();
    sensor.addField("03um particulate", data.particles_03um);
    sensor.addField("05um particulate", data.particles_05um);
    sensor.addField("10um particulate", data.particles_10um);
    sensor.addField("25um particulate", data.particles_25um);
    sensor.addField("50um particulate", data.particles_50um);
    sensor.addField("100um particulate", data.particles_100um);

    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());
      
    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    //Wait 10s
    Serial.println("Wait 10s");
    delay(10000);
    }
    //end write to influx
    ppm25 = ppm25 + data.pm25_standard; //Standard - pick one Stnd or Envir
    //ppm25 = ppm25 + data.pm25_env; //Environmental
    if (eventCount == secsToPost) {
      avgPpm25 = ppm25 / secsToPost;
      urlVars = String(avgPpm25) + "|" + String(data.pm10_standard) + "|" + String(data.pm25_standard) + "|" + String(data.pm100_standard);
      urlVars += "|" + String(data.pm10_env) + "|" + String(data.pm25_env) + "|" + String(data.pm100_env);
      Serial.println("Average: " + String(avgPpm25) + "|" + eventCount); 
      sendDataUrl(urlVars); ///send to URL every 10min (600 seconds)
      avgPpm25 = 0;
      ppm25 = 0;
      eventCount = 0;
      Serial.println("---------------+++++-------------------");

  }
}
 
boolean readPMSdata(Stream *s) {
  if (! s->available()) {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != 0x42) {
    s->read();
    return false;
  }
 
  // Now read all 32 bytes
  if (s->available() < 32) {
    return false;
  }
    
  uint8_t buffer[32];    
  uint16_t sum = 0;
  s->readBytes(buffer, 32);
 
  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }
 
  /* debugging
  for (uint8_t i=2; i<32; i++) {
    Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(", ");
  }
  Serial.println();
  */
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }
 
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);
 
  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    return false;
  }
  // success!
  return true;
}
