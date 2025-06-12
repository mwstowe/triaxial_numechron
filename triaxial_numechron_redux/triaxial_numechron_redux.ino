/*****************************************************/
/* Main custom section                               */
/*****************************************************/

/*
 * This sketch requires the following libraries :
 * - ESPAsyncWebServer (by ESP32Async (fomerly me-no-dev), which in turn install AsyncTcp
 *   (select Install All)
 *
 * These libraries may be installed trough the Arduino IDE "Tools / Manage libraries" command
 * entering the library name in the left search box and hitting Install on the found library
 *
 * Before loading the sketch, the minimum things to modify depends on the behavior expected from the clock :
 *
 * + Smartconfig mode : clock synchronises with Internet
 *   Wifi credential (SSID, password) are setup initially using Expressif ESPTouch application
 *   from a phone device
 *   -> indicate appropriate timezone string in TZSTRING, between "" (default setting is for France)
 *   THIS IS THE DEFAULT MODE
 *
 * This sketch is specifically for keeping the Triaxial Numechron syncronized with NTP.  As with other
 * mechanical clocks that synch in this way, the biggest challenge isn't keeping accurate time, it's
 * not knowing what's on its own display.
 * 
 * So how this works is by synchronizing the movement of the minutes to the NTP service, which should 
 * keep the clock perfectly accurate.  However, the hours and tens will need to be set the first time
 * by hand, and then the minutes rolled forward to match the current time.  This should only need to 
 * be done once...   and then the hour digit adjusted for daylight savings time.
 *
 * (I've toyed with the idea of having the clock auto-adjust, and it is, of course, possible, to roll
 * forward the clock by 1 hour or 11 hours at the appropriate time.  This would take roughly an hour
 * to go "back" one hour, and probably isn't worth it, but I haven't entirely discarded the idea.)
 *
 * Once powered up, the clock will advance the minutes by one (this is remnant of the adjustment of
 * the minute within the window by pressing "reset," which is still possible.)  After that, it will
 * need a WiFi signal and will synchronize with NTP.  At that point, you'll need to adjust the dials
 * using the following procedure:
 * 
 * Set the hour dial to the current hour.  If the tens digit is displaying a number less then the current
 * time, then set to the current hour minus one.
 * 
 * Roll the tens digit forward to the current time's tens digit.  If the minute window is displaying a
 * number less than the current time, then set to the current tens minus one.
 *
 * Log in to http://tn.local and first adjust the minute digit to fit within the window.  (By design, you
 * can only go backward a little, be careful with this anyway.)  Then use the +1m and +5m buttons to roll
 * the minutes forward until the time is correct.
 *
 * Setting-up mDns :
 *
 * In order to permit access to the clock without knowledge of its IP address,
 * the sketch implements the mDNS service
 * The clock WEB page should be available at http://tn.local
 * (name may be changed by modifying SNAME below)
 */

/*
 * Define explicit application name (Web pages title, private Wifi AP in SA mode)
 */
#define ANAME "Triaxial-Numechron"

/*
 * Define name on network (short name for web access : default : http://hc.local )
 */
#define SNAME "tn"

/*
 * TO BE MODIFIED according to your geographic location and daylight saving
 * You may look at the provided "timezones-navarsystems.xlsx"
 * (data from https://github.com/nayarsystems/posix_tz_db)
 * Otherwise, various sites may help (ex: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv)
 * Or search for "POSIX Timezone string", or <specific location> timezone string
 *
 * If not interested by automatic daylight saving switch you may just
 * remove or comment this line (add // before #define)
 */
#define TZSTRING   "PST8PDT;M3.2.0;M11.1.0"

/*
 * For people interested in the semantic of this:
 * CET  zone name
 * -1 	hour offset of zone name relative to UTC (Coordinated Universal Time)
 * EST 	daylight savings zone name (DST)
 * -2 	hour offset of DST zone name relative to UTC
 * M3.5.0/02 	Start of DST - March (Month 3), the fifth(5) Sunday(0), 2.00am
 * M10.5.0/03 	End of DST - October (Month 10), the fifth(5) Sunday(0), 3.00am
 */

/*
 * TO BE MODIFIED (uncomment) for using standard WiFi
 * In this case, at least one credential shall be defined below and TZSTRING accurate
 */
#define WIFI_MULTI


/*
 * Control precedence of options (don't modify)
 * If WIFI_MULTI is not defined (this is the default),
 * WIFI smart mode is used
 */
#ifndef WIFI_MULTI
/*
 * DO NOT MODIFY
 * Default to WIFI_SMART when WIFI_MULTI is not defined
 * Wifi smart config permits configuration of Wifi SSID and password initially
 * using the Expressif ESPTouch application on a phone device
 * HOWTO use this application in detail may be found on Internet
 */
#define WIFI_SMART
#endif

#ifdef WIFI_MULTI

/*
 * If Standard WIFI used, need to define at least one SSID + Password
 * NB_SSID shall be updated to indicate how many are defined
 * NB_SSID shall not be greater than MAX_SSID
 */

#define MAX_SSID  5  // Do not modify

#define NB_SSID   1  // Update according to the number of credentials 

// Specify up to MAX_SSID credential here
// Dont't forget to indicate the number of credential in NB_SSID
#include "../config.h"           // Include WiFi credentials from separate file; you can comment out 
#define WIFI_SSID2 ""
#define WIFI_PASS2 ""
#define WIFI_SSID3 ""
#define WIFI_PASS3 ""
#define WIFI_SSID4 ""
#define WIFI_PASS4 ""
#define WIFI_SSID5 ""
#define WIFI_PASS5 ""

#endif // WIFI_MULTI

/*****************************************************/
/* Other (less important) custom options             */
/*****************************************************/


#ifdef WIFI_SMART
/* 
 * Number of WiFi connection attempts in Smart Mode 
 * before entering smartconfig again
 */
#define SMART_ATTEMPT 30

/*
 * For testing purpose
 */
//#define TST_SMART
#endif

/*
* Steps for moving one minute
*/
#define STEPS_PER_ROTATION  1536

/*****************************************************/
/* End of custom section                             */
/*****************************************************/

/*****************************************************/
/* Required headers                                  */
/*****************************************************/

#include <Arduino.h>

#include "WiFi.h"
#include "WiFiMulti.h"

#include "time.h"
#include <esp_sntp.h> 
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h> // for ESP32Async (formerly me-no-dev) library
#ifdef USE_TELNET
#include <TelnetStream2.h>
#endif

#ifdef USE_OTA
#include <Update.h>
#define U_PART U_SPIFFS
#endif

#include <ESPmDNS.h>

#include <stdio.h>

Stream * Logger = &Serial;

/*****************************************************/
/* Wifi / Time Management                            */
/*****************************************************/

/*
 * Time synchonisation state
 */
// Blocking clock and scheduling until NTP sync completed
static int tms = -1;

#ifdef WIFI_MULTI

WiFiMulti wifiMulti;

// WiFi connect timeout per AP. Increase when connecting takes longer.
const uint32_t connectTimeoutMs = 10000;

#define WIFI_DELAY  3000

struct {
  char * w_ssid;
  char * w_pwd;
} wfi[MAX_SSID] = {
  { WIFI_SSID1, WIFI_PASS1 },
  { WIFI_SSID2, WIFI_PASS2 },
  { WIFI_SSID3, WIFI_PASS3 },
  { WIFI_SSID4, WIFI_PASS4 },
  { WIFI_SSID5, WIFI_PASS5 },
};
#endif // WIFI_MULTI

static void setTimezone ( const char * tzval )
{
  setenv("TZ",tzval,1);
  tzset();
}

#ifdef TZSTRING
/*
 * Setting time according to defined Timezone
 * Daylight saving time MANAGED in this case
 */ 
int setLocalTime(String et, String em)
{
  struct timeval tv;

  setTimezone("GMT-0");

  tv.tv_sec  = (int)atol(et.c_str());
  tv.tv_usec = (int)atol(em.c_str());

  if ( settimeofday(&tv, NULL) )
  {
    Logger->println("Cannot set time");
    return 0;
  }

  setTimezone(TZSTRING);

  return 1;

}

#else // TZSTRING

/*
 * Setting time independently of Timezone (set it to GMT)
 * Daylight saving time NOT MANAGED in this case
 */ 
int setLocalTime(String dt, String ti, String em)
{
  struct timeval tv;
  struct tm      loctm;

  setTimezone("GMT-0");

  sscanf(dt.c_str(), "%d/%d/%d",
         &loctm.tm_mday,
         &loctm.tm_mon,
         &loctm.tm_year);

  sscanf(ti.c_str(), "%d:%d:%d",
         &loctm.tm_hour,
         &loctm.tm_min,
         &loctm.tm_sec);

  Logger->printf("%02d/%02d/%04d %02d:%02d:%02d\n",
                 loctm.tm_mday, loctm.tm_mon, loctm.tm_year,
                 loctm.tm_hour, loctm.tm_min, loctm.tm_sec);

  loctm.tm_year -= 1900;

  tv.tv_sec  = (int) mktime(&loctm);
  tv.tv_usec = (int) atol(em.c_str());

  if ( settimeofday(&tv, NULL) )
  {
    Logger->println("Cannot set time");
    return 0;
  }

  return 1;
}

#endif // TZSTRING

/*
 * WIFI Smartconfig or standard used
 * Time synchronised using NTP ; Timezone in TZSTRING needs to be valid ... but otherwise, doesn't matter
 */

// NTP settings
#define NTP_SERVER "pool.ntp.org"
#define NTP_DELAY  10000

int printLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  Logger->print("NTP Time : ");
  Logger->printf("%02d:%02d:%02d\n",
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  Logger->print("DST : ");
  Logger->println(timeinfo.tm_isdst);

  return 1;
}

void time_sync_notification(struct timeval *tv)
{
  Logger->println("Time has been synchronised");
  printLocalTime();
  tms = 1;
}

void setNTP(void)
{
  int elapsTm;

#ifdef WIFI_MULTI
  wifiMulti.run(connectTimeoutMs);
#endif

  sntp_set_time_sync_notification_cb(time_sync_notification);

  elapsTm = sntp_get_sync_interval();
  Logger->printf("NTP sync interval: %d ms (%d min)\n",
                 elapsTm, elapsTm / 60000);

  configTzTime(TZSTRING, 0, NTP_SERVER);
}

/*
 * Setup initial time (before any time synchronisation)
 * Doing this avoids loosing time in getLocalTime()
 */
void set_init_time ( void )
{
#ifdef TZSTRING
  setTimezone(TZSTRING);
  setLocalTime("1704067200","0");
#else
  setTimezone("GMT-0");
  setLocalTime("01/01/2024","00:00:00","0");
#endif
}


void wifiSetup()
{
 int i;

#ifdef WIFI_SMART

  Logger->println("WIFI SmartConfig mode");

  //define hostname
  WiFi.setHostname(SNAME);

  WiFi.mode(WIFI_STA);

  WiFi.begin();

#ifndef TST_SMART
  Logger->println("Connecting to WiFi");
  for (i = 0; (i <= SMART_ATTEMPT) && (WiFi.status() != WL_CONNECTED) ; i++)
  {
    Logger->print(".");
    delay(1000);
  }
#endif

  if (i) Logger->println("");
  if ( WiFi.status() == WL_CONNECTED )
  {
    Logger->println("WiFi connected using previous SmartConfig credentials");
  }
  else
  {
    while ( WiFi.status() != WL_CONNECTED )
    {
      WiFi.mode(WIFI_AP_STA);
      WiFi.beginSmartConfig();

      //Wait for SmartConfig packet from mobile
      Logger->println("Waiting for SmartConfig");

      i = 0;
      while (!WiFi.smartConfigDone())
      {
        i++;
        if (i % 100)
          Logger->print(".");
        else
        {
          // No response, restart Smartconfig       
          Logger->println(".");
          WiFi.stopSmartConfig();
          WiFi.beginSmartConfig();
        }
        delay(1000);
      }

      WiFi.stopSmartConfig();

      if (i) Logger->println("");
      Logger->println("SmartConfig received");

      //Wait for WiFi to connect to AP
      Logger->println("Connecting to WiFi");
      for (i = 0; (i <= SMART_ATTEMPT) && (WiFi.status() != WL_CONNECTED) ; i++)
      {
        delay(1000);
        Logger->print(".");
      }
      if (i) Logger->println("");
    }
    Logger->println("WiFi Connected");
  }

#endif // WIFI_SMART

#ifdef WIFI_MULTI

  int done = 0;
  int elapsTm;

  Logger->println("WIFI Multi credential mode");

  //define hostname
  WiFi.setHostname(SNAME);

  WiFi.mode(WIFI_STA);

  for (i = 0; i<NB_SSID ; i++)
    wifiMulti.addAP(wfi[i%NB_SSID].w_ssid, wfi[i%NB_SSID].w_pwd);    

  while ( (wifiMulti.run() != WL_CONNECTED) );

#endif // WIFI_MULTI

  Logger->print("SSID: ");
  Logger->println(WiFi.SSID());
  Logger->print("IP Address: ");
  Logger->println(WiFi.localIP());


  // Use mdns for local host name resolution
  if ( !MDNS.begin(SNAME) )
    Logger->println("Error setting up MDNS responder!");
  else
    Logger->printf("MDNS responder set to '%s.local'\n", SNAME);

  set_init_time();
}

/*****************************************************/
/* Clock Management                                  */
/*****************************************************/

// flag to delay minute advance
int hold_update = 0;

// Variables for asynchronous rotation
bool rotation_pending = false;
int steps_to_rotate = 0;

// ports used to control the stepper motor
// if your motor rotate to the opposite direction, 
// change the order
int port[4] = {12,4,0,2};

// wait for a single step of stepper
int delaytime = 3;

// sequence of stepper motor control
int seq[4][4] = {
  {  LOW,  LOW, HIGH,  LOW},
  {  LOW,  LOW,  LOW, HIGH},
  { HIGH,  LOW,  LOW,  LOW},
  {  LOW, HIGH,  LOW,  LOW}
};

// Previous clock position (modified for adjusting offset / fine tunning)
static long prev_pos = 0;

void rotate(int step) {
  static int phase = 0;
  int i, j;
  int delta = (step > 0) ? 1 : 3;
  int dt = 20;

  step = (step > 0) ? step : -step;
  for(j = 0; j < step; j++) {
    phase = (phase + delta) % 4;
    for(i = 0; i < 4; i++) {
      digitalWrite(port[i], seq[phase][i]);
    }
    delay(dt);
    if(dt > delaytime) dt--;
  }
  // power cut
  for(i = 0; i < 4; i++) {
    digitalWrite(port[i], LOW);
  }
}

void loop() {
  static long prev_min = 0, prev_pos = -1;
  long min;
  static long pos;
  struct tm tmtime; 

  // Handle pending rotation requests from web interface
  if (rotation_pending) {
    rotate(steps_to_rotate);
    rotation_pending = false;
    hold_update = 0;  // Release the hold after rotation completes
    return;  // Skip the rest of the loop after handling rotation
  }

  // Block until synchronized
  if (tms != 1) {
    return;
  }
  
  if (!getLocalTime(&tmtime)) return;
  min = tmtime.tm_min%10;

  if(prev_min == min) {
    return;
  }

  if (hold_update == 1) { // clock is being adjusted
    return;
  }

  if(min == 0) {
    min = 10;
  }

  // since we moved a minute, skip first minute
  if(prev_pos == -1) {  
    prev_pos = STEPS_PER_ROTATION * min;
  }
  
  Logger->print("Minute:Previous minute ");
  Logger->printf("%02d:%02d\n",
                min, prev_min);
  prev_min = min;
  pos = (STEPS_PER_ROTATION * min);
  rotate(-10); // for approach run
  rotate(10); // approach run without heavy load
  Logger->print("Position:Previous position ");
  Logger->printf("%02d:%02d\n",
                pos, prev_pos);

  if(pos - prev_pos > 0) {
    rotate(pos - prev_pos);
  }
  if(min == 10) {
    prev_pos = 0;
    prev_min = 0;
  } else {  
    prev_pos = pos;
  }
}

/*****************************************************/
/* WEB server management                             */
/*****************************************************/

AsyncWebServer server(80);

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Page not found");
}

void web_svr_setup()
{
 // cvt_time(StopTime, &stop_hour, &stop_min);
 // cvt_time(StartTime, &start_hour, &start_min);

 const char mainpage_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Adjustment Page - Triaxial Numechron Clock</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700;800&display=swap" rel="stylesheet">
    <style>
        body {
            font-family: 'Inter', sans-serif;
        }
    </style>
<script type="importmap">
{
  "imports": {
    "react": "https://esm.sh/react@^19.1.0",
    "react-dom/": "https://esm.sh/react-dom@^19.1.0/",
    "react/": "https://esm.sh/react@^19.1.0/"
  }
}
</script>
</head>
<body>
    <div class="min-h-screen flex flex-col items-center justify-center bg-gradient-to-br from-slate-900 to-slate-800 text-gray-200 p-4 selection:bg-sky-500 selection:text-white">

        <!-- Main Title -->
        <h1 class="text-5xl md:text-6xl font-extrabold text-transparent bg-clip-text bg-gradient-to-r from-sky-400 to-cyan-300 mb-6 text-center">
            Triaxial Numechron Clock
        </h1>

        <!-- Explanatory Text -->
        <p class="text-lg text-gray-400 text-center max-w-2xl mb-10 leading-relaxed px-4">
            To adjust the clock display: set hours and tens by hand. If the tens need to display a lesser value than what's shown, roll the hours back by one, then roll the tens forward. After that, adjust the minutes and alignment using the controls below.
        </p>

        <div class="w-full max-w-3xl space-y-10">
            <!-- Adjust Time Section -->
            <div class="p-6 sm:p-8 bg-slate-800/50 rounded-xl shadow-2xl backdrop-blur-md border border-slate-700">
                <h2 class="text-3xl font-bold text-center text-sky-400 mb-8">Adjust Time</h2>
                <div class="grid grid-cols-2 sm:grid-cols-4 gap-4">
                    <a href="/plus1m" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+1 min</a>
                    <a href="/plus3m" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+3 min</a>
                    <a href="/plus5m" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+5 min</a>
                    <a href="/plus7m" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+7 min</a>
                </div>
            </div>

            <!-- Adjust Alignment Section -->
            <div class="p-6 sm:p-8 bg-slate-800/50 rounded-xl shadow-2xl backdrop-blur-md border border-slate-700">
                <h2 class="text-3xl font-bold text-center text-sky-400 mb-8">Adjust Alignment</h2>
                <div class="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-6 gap-4">
                    <a href="/nudge_minus_5_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-red-600 hover:bg-red-700">-5%</a>
                    <a href="/nudge_minus_1_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-red-600 hover:bg-red-700">-1%</a>
                    <a href="/nudge_plus_1_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+1%</a>
                    <a href="/nudge_plus_5_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+5%</a>
                    <a href="/nudge_plus_10_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+10%</a>
                    <a href="/nudge_plus_50_percent" class="no-underline flex items-center justify-center font-semibold py-3 px-4 sm:px-6 rounded-lg shadow-lg hover:shadow-xl transition-all duration-300 ease-in-out transform hover:-translate-y-0.5 focus:outline-none focus:ring-4 focus:ring-opacity-50 text-white bg-sky-600 hover:bg-sky-500 focus:ring-sky-400">+50%</a>
                </div>
            </div>
        </div>

        <footer class="mt-12 text-center text-gray-500 text-sm">
            <p>&copy; 2024 Numechron Adjustments Inc. All rights reserved.</p>
            <p>This is a simulated interface.</p>
        </footer>
    </div>
</body>
</html>
 )rawliteral";

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [mainpage_html](AsyncWebServerRequest *request){
    request->send(200, "text/html", mainpage_html );
  });

  server.on("/plus1m", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 1 MIN\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION;
    request->redirect("/");
  });

  server.on("/plus3m", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 3 MIN\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION*3;
    request->redirect("/");
  });

  server.on("/plus5m", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 5 MIN\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION*5;
    request->redirect("/");
  });

  server.on("/plus7m", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 7 MIN\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION*7;
    request->redirect("/");
  });

  server.on("/nudge_minus_5_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("DECR 5%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = -STEPS_PER_ROTATION/20;
    request->redirect("/");
  });

  server.on("/nudge_minus_1_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("DECR 1%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = -STEPS_PER_ROTATION/100;
    request->redirect("/");
  });

  server.on("/nudge_plus_5_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 5%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION/20;
    request->redirect("/");
  });

  server.on("/nudge_plus_10_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 10%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION/10;
    request->redirect("/");
  });


  server.on("/nudge_plus_1_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 1%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION/100;
    request->redirect("/");
  });

  server.on("/nudge_plus_50_percent", HTTP_GET, [mainpage_html] (AsyncWebServerRequest *request)
  {
    Logger->printf("INCR 50%\n");
    hold_update = 1;
    rotation_pending = true;
    steps_to_rotate = STEPS_PER_ROTATION/2;
    request->redirect("/");
  });

  server.onNotFound(notFound);
  server.begin();
}

/*****************************************************/
/* Entry points                                      */
/*****************************************************/

void setup()
{
  Logger = &Serial;

  Serial.begin(115200);

  Logger->println("");
  Logger->println("Start Triaxial Numechron");

  pinMode(port[0], OUTPUT);
  pinMode(port[1], OUTPUT);
  pinMode(port[2], OUTPUT);
  pinMode(port[3], OUTPUT);
  rotate(-10); // for approach run
  rotate(10); // approach run without heavy load
  rotate(STEPS_PER_ROTATION);

  wifiSetup();

  web_svr_setup();

  setNTP();

  Logger->println("Setup complete");
}


