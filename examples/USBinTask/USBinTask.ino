/*
 * Mega + expansion RAM + funky status LED,
 *
 * IMPORTANT! PLEASE USE Arduino 1.0.5 or better!
 * Older versions HAVE MAJOR BUGS AND WILL NOT WORK AT ALL!
 * Use of gcc-avr and lib-c that is newer than the Arduino version is even better.
 *
 * NOTE: FAT on DVD-RAM or DVD+RW *WILL* quickly destroy it!
 *
 * DVD-RAM formatting note:
 *      2236704 sectors total (LBA)
 *      mformat -S 3 -t 1013 -h 69 -n 32 -F -M 2048 -c 1 -m 0xf8 z:
 *      ZERO sectors wasted
 *
 * DVD+RW formatting note:
 *       2295104 sectors total (LBA)
 *       mformat -S 3 -t 1526 -h 47 -n 32 -F -M 2048 -c 1 -m 0xf8 z:
 *       ZERO sectors wasted
 *
 * This program tests, xmemUSB core and xmemUSBFS library module, as well as
 * a reconfigured LDC panel (optional, not needed).
 * It also tests many parts of xmem2.
 *
 */

// These are needed for auto-config and Arduino
#include <Wire.h>
#include <SPI.h>
//

#ifndef USE_MULTIPLE_APP_API
#define USE_MULTIPLE_APP_API 16
#endif
// Set this to 0 if you don't own the lcd panel or have not modified it to use SPI.
// Since LCD does not safely support teensy, define as 0
#define _USE_LCD 0
#define _USE_FS 1

#include <xmem.h>
#include <RTClib.h>
#include <usbhub.h>
#include <masstorage.h>
#include <stdio.h>
#include <xmemUSB.h>


#if _USE_FS
#include <xmemUSBFS.h>
#include <Storage.h>
#endif

#if _USE_LCD

#include <ColorLCDShield.h>

#define CLOCK_RADIUS 50  // radius of clock face
#define CLOCK_CENTER 55  // If you adjust the radius, you'll probably want to adjust this
#define H_LENGTH  30  // length of hour hand
#define M_LENGTH  40  // length of minute hand
#define S_LENGTH  48  // length of second hand

#define BACKGROUND  BLACK  // room for growth, adjust the background color according to daylight
#define C_COLOR  0x822  // This is the color of the clock face, and digital clock
#define H_COLOR  RED  // hour hand color
#define M_COLOR  GREEN  // minute hand color
#define S_COLOR  BLUE  // second hand color

static int16_t hours, minutes, seconds, ampm;
static uint32_t lastmil;
static int buttonPins[3];
LCDShield lcd;
#endif

#if _USE_FS
////////////////////////////////////////////////////////////////////////////////
//
// Test file system
//
////////////////////////////////////////////////////////////////////////////////

void show_dir(DIRINFO *de) {
        int res;
        uint64_t fre;
        uint32_t numlo;
        uint32_t numhi;
        int fd = opendir("/");
        if(fd > 0) {
                printf_P(PSTR("Directory of '/'\r\n"));
                do {
                        res = readdir(fd, de);

                        if(!res) {
                                DateTime tstamp(de->fdate, de->ftime);
                                if(!(de->fattrib & AM_VOL)) {
                                        if(de->fattrib & AM_DIR) {
                                                printf_P(PSTR("d"));
                                        } else printf_P(PSTR("-"));

                                        if(de->fattrib & AM_RDO) {
                                                printf_P(PSTR("r-"));
                                        } else printf_P(PSTR("rw"));

                                        if(de->fattrib & AM_HID) {
                                                printf_P(PSTR("h"));
                                        } else printf_P(PSTR("-"));

                                        if(de->fattrib & AM_SYS) {
                                                printf_P(PSTR("s"));
                                        } else printf_P(PSTR("-"));


                                        if(de->fattrib & AM_ARC) {
                                                printf_P(PSTR("a"));
                                        } else printf_P(PSTR("-"));



                                        printf_P(PSTR(" %12lu"), de->fsize);
                                        printf_P(PSTR(" %.4u-%.2u-%.2u"), tstamp.year(), tstamp.month(), tstamp.day());
                                        printf_P(PSTR(" %.2u:%.2u:%.2u"), tstamp.hour(), tstamp.minute(), tstamp.second());
                                        printf_P(PSTR(" %s"), de->fname);
                                        if(de->lfname[0] != 0) {
                                                printf_P(PSTR(" (%s)"), de->lfname);
                                        }
                                        printf_P(PSTR("\r\n"));
                                }
                        }

                } while(!res);
                closedir(fd);

                fre = fs_getfree("/");
                numlo = fre % 1000000000llu;
                numhi = fre / 1000000000llu;
                if(numhi) printf("%lu", numhi);
                printf("%lu bytes available on disk.\r\n", numlo);
        }

}

void test_main(void) {
        uint32_t start;
        uint32_t end;
        uint64_t fre;
        uint32_t wt;
        uint32_t rt;
        char fancy[4];
        int fd;
        uint8_t fdc;
        uint8_t *ptr;
        uint8_t slots = 0;
        uint8_t spin = 0;
        uint8_t this_task = xmem::getcurrentBank();
        DIRINFO *de = (DIRINFO *)malloc(sizeof (DIRINFO));
        uint8_t *data = (uint8_t *)malloc(128);
        int res;

        fancy[0] = '-';
        fancy[1] = '\\';
        fancy[2] = '|';
        fancy[3] = '/';

        //char *current_path;
        //current_path = (char *)malloc(2);
        //uint8_t *ptr = (uint8_t *)current_path;
        //*ptr = '/';
        //ptr++;
        //*ptr = 0x00;
        printf_P(PSTR("Maximum Volume mount count :%i\r\n"), _VOLUMES);
        printf_P(PSTR("\r\nTesting task started. PID=%i"), this_task);
        for(;;) {
                fdc = _VOLUMES;
                uint8_t last = _VOLUMES;
                printf_P(PSTR("\r\nWaiting for '/' to mount..."));
                while(fdc == _VOLUMES) {
                        slots = fs_mountcount();
                        if(slots != last) {
                                last = slots;
                                if(slots != 0) {
                                        printf_P(PSTR(" \r\n"), ptr);
                                        fdc = fs_ready("/");
                                        for(uint8_t x = 0; x < _VOLUMES; x++) {
                                                ptr = (uint8_t *)fs_mount_lbl(x);
                                                if(ptr != NULL) {
                                                        printf_P(PSTR("'%s' is mounted\r\n"), ptr);
                                                        free(ptr);
                                                }
                                        }
                                        if(fdc == _VOLUMES) printf_P(PSTR("\r\nWaiting for '/' to mount..."));
                                }
                        }
                        printf_P(PSTR(" \b%c\b"), fancy[spin]);
                        spin = (1 + spin) % 4;
                        xmem::Sleep(100);
                }
                printf_P(PSTR(" \b"));
                fre = fs_getfree("/");
                if(fre > 2097152) {
                        //show_dir(de);
                        printf_P(PSTR("Removing '/HeLlO.tXt' file... "));
                        res = unlink("/hello.txt");
                        printf_P(PSTR("completed with %i\r\n"), res);
                        //show_dir(de);
                        printf_P(PSTR("\r\nStarting Write test...\r\n"));
                        fd = open("/HeLlO.tXt", O_WRONLY | O_CREAT);
                        if(fd > 0) {
                                printf_P(PSTR("File opened OK, fd = %i\r\n"), fd);
                                //char tst[] = "                                        \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
                                //for(int i = 0; i < 26; i++) {
                                //        write(fd, tst, strlen(tst));
                                //}
                                char hi[] = "]-[ello \\/\\/orld!\r\n";
                                res = write(fd, hi, strlen(hi));
                                printf_P(PSTR("Wrote %i bytes, "), res);
                                res = close(fd);
                                printf_P(PSTR("File closed result = %i.\r\n"), res);
                        } else {
                                printf_P(PSTR("xError %d (%u)\r\n"), fd, fs_err[this_task]);
                        }
                        //show_dir(de);
                        delay(1000);
                        printf_P(PSTR("\r\nStarting Read test...\r\n"));
                        fd = open("/hElLo.TxT", O_RDONLY);
                        if(fd > 0) {
                                res = 1;
                                printf_P(PSTR("File opened OK, fd = %i, displaying contents...\r\n"), fd);

                                while(res > 0) {
                                        res = read(fd, data, 128);
                                        for(int i = 0; i < res; i++) {
                                                if(data[i] == '\n') Serial.write('\r');
                                                if(data[i] != '\r') Serial.write(data[i]);
                                        }
                                }
                                printf_P(PSTR("\r\nRead completed, last read result = %i (%i), "), res, fs_err[this_task]);
                                res = close(fd);
                                printf_P(PSTR("file close result = %i.\r\n"), res);
                                //show_dir(de);
                                printf_P(PSTR("Testing rename\r\n"));
                                unlink("/newtest.txt");
                                res = rename("/HeLlO.tXt", "/newtest.txt");
                                printf_P(PSTR("file rename result = %i.\r\n"), res);
                        } else {
                                printf_P(PSTR("File not found.\r\n"));
                        }
                        //show_dir(de);
                        printf_P(PSTR("\r\nRemoving '/1MB.bin' file... "));
                        res = unlink("/1MB.bin");
                        printf_P(PSTR("completed with %i\r\n"), res);
                        //show_dir(de);
                        printf_P(PSTR("1MB write timing test "));

                        //for (int i = 0; i < 128; i++) data[i] = i & 0xff;
                        fd = open("/1MB.bin", O_WRONLY | O_CREAT);
                        if(fd > 0) {
                                int i = 0;
                                xmem::Sleep(500);
                                start = millis();
                                for(; i < 8192; i++) {
                                        res = write(fd, data, 128);
                                        if(fs_err[this_task]) break;
                                }
                                printf_P(PSTR(" %i writes, (%i), "), i, fs_err[this_task]);
                                res = close(fd);
                                end = millis();
                                wt = end - start;
                                printf_P(PSTR("(%i), "), fs_err[this_task]);
                                printf_P(PSTR(" %lu ms (%lu sec)\r\n"), wt, (500 + wt) / 1000UL);
                        }
                        printf_P(PSTR("completed with %i\r\n"), fs_err[this_task]);

                        //show_dir(de);
                        printf_P(PSTR("1MB read timing test "));

                        fd = open("/1MB.bin", O_RDONLY);
                        if(fd > 0) {
                                xmem::Sleep(500);
                                start = millis();
                                res = 1;
                                int i = 0;
                                while(res > 0) {
                                        res = read(fd, data, 128);
                                        i++;
                                }
                                printf_P(PSTR("%i reads, (%i), "), i, fs_err[this_task]);
                                end = millis();
                                res = close(fd);
                                rt = end - start;
                                printf_P(PSTR(" %lu ms (%lu sec)\r\n"), rt, (500 + rt) / 1000UL);
                        }
                        printf_P(PSTR("completed with %i\r\n"), fs_err[this_task]);
                } else {
                        printf_P(PSTR("Not enough free space or write protected."));
                }

                show_dir(de);

                printf_P(PSTR("\r\nFlushing caches..."));
                fs_sync(); // IMPORTANT! Sync all caches to all medias!
                printf_P(PSTR("\r\nRemove and insert media..."));
                while(fs_ready("/") != _VOLUMES) {
                        printf_P(PSTR(" \b%c\b"), fancy[spin]);
                        spin = (1 + spin) % 4;
                        xmem::Sleep(100);
                }
                printf_P(PSTR(" \r\n\r\n"));
        }
}
#endif
#if _USE_LCD

////////////////////////////////////////////////////////////////////////////////
//
// Test that LCD panel can coexist on the same SPI bus.
// Test that I2C is locked across tasks.
//
////////////////////////////////////////////////////////////////////////////////

/*
  displayDigitalTime() takes in values for hours, minutes, seconds
  and am/pm. It'll print the time, in digital format, on the
  bottom of the screen.
 */
void displayDigitalTime() {
        char timeChar[12];
        if(!ampm) {
                sprintf_P(timeChar, PSTR("%.2d:%.2d:%.2d AM"), hours, minutes, seconds);
        } else {
                sprintf_P(timeChar, PSTR("%.2d:%.2d:%.2d PM"), hours, minutes, seconds);
        }
        /* Print the time on the clock */
        lcd.setStr(timeChar, CLOCK_CENTER + CLOCK_RADIUS + 4, 22, C_COLOR, BACKGROUND);
}

/*
  drawClock() simply draws the outer circle of the clock, and '12',
  '3', '6', and '9'. Room for growth here, if you want to customize
  your clock. Maybe add dashe marks, or even all 12 digits.
 */
void drawClock() {

        /* Print 12, 3, 6, 9, a lot of arbitrary values are used here
           for the coordinates. Just used trial and error to get them
           into a nice position. */
        lcd.setStr("12", CLOCK_CENTER - CLOCK_RADIUS, 66 - 9, C_COLOR, BACKGROUND);
        lcd.setStr("3", CLOCK_CENTER - 9, 66 + CLOCK_RADIUS - 12, C_COLOR, BACKGROUND);
        lcd.setStr("6", CLOCK_CENTER + CLOCK_RADIUS - 18, 66 - 4, C_COLOR, BACKGROUND);
        lcd.setStr("9", CLOCK_CENTER - 9, 66 - CLOCK_RADIUS + 4, C_COLOR, BACKGROUND);
}

/*
  displayAnalogTime() draws the three clock hands in their proper
  position. Room for growth here, I'd like to make the clock hands
  arrow shaped, or at least thicker and more visible.
 */

void displayAnalogTime() {
        static int16_t ohx, ohy, omx, omy, osx, osy;
        int16_t hx, hy, mx, my, sx, sy;

        float sf = radians(((seconds * 6) + 270) % 360);
        float mf = radians(((minutes * 6) + 270) % 360);
        float hf = radians(((hours * 30) + 270) % 360);

        sx = (int16_t)(round(sin(sf) * S_LENGTH));
        sy = (int16_t)(round(cos(sf) * S_LENGTH));
        mx = (int16_t)(round(sin(mf) * M_LENGTH));
        my = (int16_t)(round(cos(mf) * M_LENGTH));
        hx = (int16_t)(round(sin(hf) * H_LENGTH));
        hy = (int16_t)(round(cos(hf) * H_LENGTH));

        lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + osx, 66 + osy, BACKGROUND); // Erase second hand
        osx = sx;
        osy = sy;
        if(((ohx ^ hx) | (ohy ^ hy))) {
                lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + ohx, 66 + ohy, BACKGROUND); // Erase hour hand
                ohx = hx;
                ohy = hy;
        }
        if(((omx ^ mx) | (omy ^ my))) {
                lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + omx, 66 + omy, BACKGROUND); // Erase minute hand
                omx = mx;
                omy = my;
        }

        drawClock();
        lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + osx, 66 + osy, S_COLOR); // Draw second hand
        lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + omx, 66 + omy, M_COLOR); // Draw minute hand
        lcd.setLine(CLOCK_CENTER, 66, CLOCK_CENTER + ohx, 66 + ohy, H_COLOR); // Draw hour hand

}

void updateClock() {
        displayAnalogTime();
        displayDigitalTime();
}

/*
  setTime uses on-shield switches S1, S2, and S3 to set the time
  pressing S3 will exit the function. S1 increases hours, S2
  increases seconds.
 */
void setTime() {
        /* Reset the clock to midnight */
        seconds = 0;
        minutes = 0;
        hours = 12;
        ampm = 0;

        /* Draw the clock, so we can see the new time */
        updateClock();
        while(!digitalRead(buttonPins[2])) xmem::Yield();

        /* We'll run around this loop until S3 is pressed again */
        while(digitalRead(buttonPins[2])) {
                /* If S1 is pressed, we'll update the hours */
                if(!digitalRead(buttonPins[0])) {
                        hours++; // Increase hours by 1
                        if(hours == 12)
                                ampm ^= 1; // Flip am/pm if it's 12 o'clock
                        if(hours >= 13)
                                hours = 1; // Set hours to 1 if it's 13. 12-hour clock.

                        /* and update the clock, so we can see it */
                        updateClock();
                        delay(250);
                }
                if(!digitalRead(buttonPins[1])) {
                        minutes++; // Increase minutes by 1
                        if(minutes >= 60)
                                minutes = 0; // If minutes is 60, set it back to 0

                        /* and update the clock, so we can see it */
                        updateClock();
                        delay(100);
                }
        }
        /* Once S3 is pressed, we'll exit, but not until it's released */
        while(!digitalRead(buttonPins[2])) xmem::Yield();

        DateTime o = RTCnow();
        if(!ampm && hours == 12) hours = 0;
        if(ampm) hours += 12;
        DateTime q = DateTime(o.year(), o.month(), o.day(), hours, minutes, seconds);
        RTCset(q);
        DateTime t = RTCnow();
        hours = t.hour();
        minutes = t.minute();
        seconds = t.second();
        if(hours > 11) ampm = 1;
        else ampm = 0;
        if(hours > 11) hours -= 12;
        if(hours == 0) hours = 12;
        updateClock();
}

void LCD_clock(void) {
        xmem::Sleep(200);
        buttonPins[0] = A0;
        buttonPins[1] = A1;
        buttonPins[2] = A2;
        /* Set up the button pins as inputs, set pull-up resistor */
        for(int i = 0; i < 3; i++) {
                pinMode(buttonPins[i], INPUT);
                digitalWrite(buttonPins[i], HIGH);
        }
        /* Initialize the LCD, set the contrast, clear the screen */
        lcd.init(false);
        lcd.contrast(0x80);
        lcd.clear(BACKGROUND);

        /* Draw the circle */
        lcd.setCircle(CLOCK_CENTER, 66, CLOCK_RADIUS, C_COLOR);
        DateTime t = RTCnow();
        hours = t.hour();
        minutes = t.minute();
        seconds = t.second();
        if(hours > 11) ampm = 1;
        else ampm = 0;
        if(hours > 11) hours -= 12;
        if(hours == 0) hours = 12;
        updateClock();
        lastmil = 0L;

        // loop() ;-)
        for(;;) {
                xmem::Yield();
                uint32_t mils = millis();
                uint32_t delta = mils - lastmil;
                if(delta < 1000) {
                        if(!digitalRead(buttonPins[2])) {
                                setTime(); // If S3 was pressed, go set the time
                                lastmil = millis();
                        }
                } else {
                        lastmil = millis();
                        DateTime v = RTCnow();
                        hours = v.hour();
                        minutes = v.minute();
                        seconds = v.second();
                        if(hours > 11) ampm = 1;
                        else ampm = 0;
                        if(hours > 11) hours -= 12;
                        if(hours == 0) hours = 12;
                        updateClock();
                }

        }
}

#endif



////////////////////////////////////////////////////////////////////////////////
//
// Setup and loop
//
////////////////////////////////////////////////////////////////////////////////


USB Usb;
USBHub Hub(&Usb);
static uint8_t brightness;
static int8_t brightness_modification_value;

void setup() {
        brightness = 0;
        brightness_modification_value = 1;

        // declare LED pin to be an output:
        pinMode(LED_BUILTIN, OUTPUT);
        analogWrite(LED_BUILTIN, 0);

        USB_Module_Calls Calls[2];
        {
                uint8_t c = 0;
#if _USE_FS
                Calls[c] = GenericFileSystem::USB_Module_Call;
                c++;
#endif
                Calls[c] = NULL;
        }

        USB_Setup(Calls);

#ifdef USING_MAKEFILE
        printf_P(PSTR("\r\n\r\nYou used make, excellent!\r\n\r\n"));
#else
        printf_P(PSTR("\r\n\r\nWARNING: You did not use make, using the USB HOST Serial ONLY.\r\n\r\n"));
#endif
        //
        // Note! Tasks do NOT actually start here. They start after the return to main()!
        //
        // The value t1 is thrown out. You can save it to a global if you need to.
        // This demo does not need to.
        //
#if _USE_FS
        uint8_t t1 = xmem::SetupTask(test_main);
        xmem::StartTask(t1);
#endif
#if _USE_LCD
        uint8_t t2 = xmem::SetupTask(LCD_clock);
        xmem::StartTask(t2);
#endif
}

// This is the main task (task 0). Not using it to do anything except service serial.
// YOU SHOULD ALWAYS Yield() in loop() at least once for maximal performance if it is not used!
// For trivial stuff (LED blinker) sprinkled Yield() works fantastic, giving other tasks more MCU time.
// Note that the LED speed can actually indicate load on the uC... slower == higher load.

void loop() {
        xmem::Yield();
        analogWrite(LED_BUILTIN, brightness);
        xmem::Yield();
        brightness += brightness_modification_value;
        xmem::Yield();
        if(brightness == 0 || brightness == 255) brightness_modification_value = -brightness_modification_value;
        xmem::Yield();
}

