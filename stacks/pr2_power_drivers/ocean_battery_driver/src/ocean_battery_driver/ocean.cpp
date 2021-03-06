
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>

#include "ocean.h"

#define FILE_LOGGING 0

using namespace willowgarage::ocean;

// Define these consts here to make gcc4 happy
const int ocean::NMEA_MAX;
const int ocean::MAXTAGLEN;
const int ocean::MAXCHANNELS;

const int ocean::INPUT_BUF_SIZE;
const int ocean::OUTPUT_BUF_SIZE;
const unsigned int ocean::MAX_PACKET_LENGTH;
const int ocean::BAD_PACKET;
const int ocean::NO_PACKET;
const int ocean::NMEA_PACKET;

//Battery Registers
//Takend from the Smart Battery Data Specification Revision 1.1, Dec. 11, 1998
//
const struct ocean::regPair ocean::regList[] = {
      {"manufacturerAccess"      ,""      , 0x00},
      {"remainingCapacityAlarm"  ,""      , 0x01},
      {"remainingTimeAlarm"      ,"min"   , 0x02},
      {"batteryMode"             ,""      , 0x03},
      {"atRate"                  ,"mA"    , 0x04},  //could also be mW
      {"atRateTimeToFull"        ,"min"   , 0x05},
      {"atRateTimeToEmpty"       ,"min"   , 0x06},
      {"atRateOK"                ,"bool"  , 0x07},
      {"temperature"             ,"0.1 K" , 0x08}, 
      {"voltage"                 ,"mV"    , 0x09}, 
      {"current"                 ,"mA"    , 0x0a}, 
      {"averageCurrent"          ,"mA"    , 0x0b}, 
      {"maxError"                ,"%"     , 0x0c}, 
      {"relativeStateOfCharge"   ,"%"     , 0x0d},
      {"absoluteStateOfCharge"   ,"%"     , 0x0e}, 
      {"remainingCapacity"       ,"mAh"   , 0x0f},  //could also be mWh
      {"fullChargeCapacity"      ,"mAh"   , 0x10},  //could also be mWh
      {"runTimeToEmpty"          ,"min"   , 0x11}, 
      {"averageTimeToEmpty"      ,"min"   , 0x12}, 
      {"averageTimeToFull"       ,"min"   , 0x13}, 
      {"batteryStatus"           ,""      , 0x16}, 
      {"cycleCount"              ,"cycle" , 0x17}, 
      {"designCapacity"          ,"mAh"   , 0x18}, //could also be mWh
      {"designVoltage"           ,"mV"    , 0x19}, 
      {"specificationInfo"       ,""      , 0x1a}, 
      {"manufactureDate"         ,"DMY"   , 0x1b}, 
      {"serialNumber"            ,"uint"  , 0x1c}, 
      {"manufactureName"         ,"string", 0x20}, 
      {"deviceName"              ,"string", 0x21}, 
      {"deviceChemistry"         ,"string", 0x22}, 
      {"manufactureData"         ,""      , 0x23}
    };
const unsigned ocean::regListLength(sizeof(regList)/ sizeof(struct regPair));
/**
 * Open the required device and establish communications with
 * the OCEAN.
 */

ocean::ocean ( int id,  int debug)
{
  debuglevel = debug;
  server.id = id;
  server.battery.resize(4);
}

ocean::~ocean ()
{
  if (close (inputDevice) != 0)
  {
    fprintf (stderr, "failed closing serial device: %s\n", strerror (errno));
  }
}

void
ocean::setDebugLevel (int level)
{
  debuglevel = level;
}

void
ocean::initialize (const std::string &input_dev)
{
  inputDevice = open (input_dev.c_str(), O_RDWR | O_NOCTTY);
  //printf ("inputDevice %d\n", inputDevice);
  if (inputDevice < 0)
  {
    fprintf (stderr, "failed to open tty device: %s\n", strerror (errno));
  }
  if (isatty (inputDevice) != 1)
  {
    fprintf (stderr, "Device not a tty device.\n");
  }
  report (2, "Device opened dev=%d\n", inputDevice);

  tcgetattr (inputDevice, &ttyset);     //get current port settings

  ttyset.c_cflag &= ~(PARENB | PARODD | CRTSCTS);
  ttyset.c_cflag |= CREAD | CLOCAL;
  ttyset.c_iflag = ttyset.c_oflag = ttyset.c_lflag = (tcflag_t) 0;

  set_speed (19200);

  //memset(batReg, 0, sizeof(batReg));
  //memset(batRegFlag, 0, sizeof(batReg));
  server.lastTimeSystem = server.lastTimeController = 0;
  //memset( server.lastTimeBattery, 0, sizeof(server.lastTimeBattery));
  //memset( batRegTime, 0, sizeof(batRegTime));

#if (FILE_LOGGING > 0)
  char logname[128];
  sprintf ( logname, "/tmp/oceanServer%c.log", input[strlen(input) -1]);
  report (2, "Logging to file: %s\n", logname);

  outputFile = open( logname, (O_WRONLY | O_APPEND ) );

  if(outputFile < 0)
  {
    if(errno == ENOENT )
    {
      outputFile = open( logname, (O_WRONLY | O_CREAT), (S_IRWXU | S_IRWXG) );
    }

    if((outputFile < 0))
    {
      report (2, "Failed to open log file: %d\n", errno);
      close(inputDevice);
      exit(outputFile);
    }
  }
#endif

  if(commTest())  //If the Ocean isn't talking then get it into the NMEA mode
    resetOcean();

}

int ocean::commTest()
{
  int maxTries = 10;
  fd_set rfds;
  struct timeval tv;
  int retval;

  packet_reset();

  int listenCount = 50;
  do
  {
    maxTries = 10;

    while((packetType != NMEA_PACKET) && (maxTries--))
    {
      report(5,"commTest: call packet_get\n");
      int result = NO_PACKET;

      FD_ZERO(&rfds);
      FD_SET(inputDevice, &rfds);
      tv.tv_sec = 2;
      tv.tv_usec = 0;

      retval = select( inputDevice + 1, &rfds, NULL, NULL, &tv);
      if(retval < 0)
        report(0, "select error\n");
      else if(retval > 0)
      {
        {
          report(5, "calling packet_get\n");
          result = packet_get();
        }
      }
      else
      {
        report(2, "select timeout\n");
        listenCount -= 10;
        maxTries = 0;
      }
      usleep(1000);
    }

    if(packetType == NMEA_PACKET)
    {
      report(5,"NMEA packet\n");
      if(nmea_parse() > 0)
      {
        return(0);
      }
    } else
       report(5,"non NMEA packet\n");

    report(6, "listenCount=%d\n", listenCount);
  } while( listenCount-- > 0);

  return(-1);
}

void
ocean::resetOcean()
{
  report(5, "Sending ocean reset string\n");
  string_send(" ");
  usleep(1000);
  string_send("x");
  usleep(1000);
}

int
ocean::run (void)
{
  int status (0);

  packet_get ();
  if (packetType == NMEA_PACKET)
  {
    status = nmea_parse ();
  }
  return status;
}

/**
 * Flush out the serial buffer.
 */
void
ocean::flush (void)
{
  tcflush (inputDevice, TCIOFLUSH);
}


int
ocean::get_speed (void)
{
  speed_t code = cfgetospeed (&ttyset);
  switch (code)
  {
    case B0:
      return (0);
    case B300:
      return (300);
    case B1200:
      return (1200);
    case B2400:
      return (2400);
    case B4800:
      return (4800);
    case B9600:
      return (9600);
    case B19200:
      return (19200);
    case B38400:
      return (38400);
    case B57600:
      return (57600);
    default:
      return (115200);
  }
}

void
ocean::set_speed (int newspeed)
{
  unsigned int rate;
  if (newspeed < 300)
    rate = B0;
  else if (newspeed < 1200)
    rate = B300;
  else if (newspeed < 2400)
    rate = B1200;
  else if (newspeed < 4800)
    rate = B2400;
  else if (newspeed < 9600)
    rate = B4800;
  else if (newspeed < 19200)
    rate = B9600;
  else if (newspeed < 38400)
    rate = B19200;
  else if (newspeed < 57600)
    rate = B38400;
  else if (newspeed < 115200)
    rate = B57600;
  else
    rate = B115200;

  if (cfsetispeed (&ttyset, B0) != 0)
    report (0, "Failed setting input speed\n");
  if (cfsetospeed (&ttyset, rate) != 0)
    report (0, "Failed setting output speed\n");

  ttyset.c_iflag &= ~(PARMRK | INPCK);
  ttyset.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);
  //ttyset.c_cflag |= CS8 | PARENB | PARODD;
  ttyset.c_cflag |= CS8;

  if (tcsetattr (inputDevice, TCSANOW, &ttyset) != 0)
    report (0, "Failed to configure serial device\n");

  flush ();
  report (1, "set_speed speed=%d rate=%x\n", newspeed, rate);

  currentBaudRate = (unsigned int) newspeed;
  packet_reset ();
}

void
ocean::report (int errlevel, const char *fmt, ...)
{
  if (errlevel <= debuglevel)
  {
    char buf[BUFSIZ], buf2[BUFSIZ], *sp;
    va_list ap;

    (void) strcpy (buf, "ocean: ");
    va_start (ap, fmt);
    (void) vsnprintf (buf + strlen (buf), sizeof (buf) - strlen (buf), fmt,
                      ap);
    va_end (ap);

    buf2[0] = '\0';
    for (sp = buf; *sp != '\0'; sp++)
      if (isprint (*sp)
          || (isspace (*sp) && (sp[1] == '\0' || sp[2] == '\0')))
        (void) snprintf (buf2 + strlen (buf2), 2, "%c", *sp);
      else
        (void) snprintf (buf2 + strlen (buf2), 6, "\\x%02x", (unsigned) *sp);

    (void) fputs (buf2, stderr);
  }
}


enum
{
  GROUND_STATE,                 /* we don't know what packet type to expect */
  NMEA_DOLLAR,                  /* we've seen first character of NMEA leader */
  NMEA_PUB_LEAD,                /* seen second character of NMEA G leader */
  NMEA_LEADER_END,              /* seen end char of NMEA leader, in body */
  NMEA_CR,                      /* seen terminating \r of NMEA packet */
  NMEA_RECOGNIZED               /* saw trailing \n of NMEA packet */
};

char *
ocean::gpsd_hexdump (void *binbuf, size_t binbuflen)
{
  static char hexbuf[MAX_PACKET_LENGTH * 2 + 1];
  size_t i;
  size_t len =
    (size_t) ((binbuflen >
               MAX_PACKET_LENGTH) ? MAX_PACKET_LENGTH : binbuflen);
  char *ibuf = (char *) binbuf;
  memset (hexbuf, 0, sizeof (hexbuf));

  for (i = 0; i < len; i++)
  {
    (void) snprintf (hexbuf + (2 * i), 3, "%02x",
                     (unsigned int) (ibuf[i] & 0xff));
  }
  return hexbuf;
}

/**
 * A sentence looks something like this:
 * $GPRMC,224759.85,A,3713.45089,N,12145.99453,W,000.04,161.59,280805,14.95,E,A*22
 * $GPRMC,224759.90,A,3713.45088,N,12145.99453,W,000.07,162.37,280805,14.95,E,A*2F 
 */
void
ocean::nextstate (unsigned char c)
{
  switch (packetState)
  {
    case GROUND_STATE:
      if (c == '$')
      {
        packetState = NMEA_DOLLAR;
      }
      break;

    case NMEA_DOLLAR:
      if (c == 'S')        /* vendor sentence */
        packetState = NMEA_LEADER_END;
      else if (c == 'C')        /* vendor sentence */
        packetState = NMEA_LEADER_END;
      else if (c == 'B')        /* vendor sentence */
        //packetState = NMEA_PUB_LEAD;
        packetState = NMEA_LEADER_END;
      else
        packetState = GROUND_STATE;
      break;

/*
    case NMEA_PUB_LEAD:
      currentBattery = c - '0'; //convert to battery number
      packetState = NMEA_LEADER_END;
      break;
*/

    case NMEA_LEADER_END:
      if (c == '\r')
        packetState = NMEA_CR;
      else if (c == '\n')
        /* not strictly correct, but helps for interpreting logfiles */
        packetState = NMEA_RECOGNIZED;
      else if (c == '$')
        /* faster recovery from missing sentence trailers */
        packetState = NMEA_DOLLAR;
      else if (!isprint (c))
        packetState = GROUND_STATE;
      break;

    case NMEA_CR:
      if (c == '\n')
        packetState = NMEA_RECOGNIZED;
      else
        packetState = GROUND_STATE;
      break;
    case NMEA_RECOGNIZED:
      if (c == '$')
        packetState = NMEA_DOLLAR;
      else
        packetState = GROUND_STATE;
      break;

  }
}

#define STATE_DEBUG

/* packet grab succeeded, move to output buffer */
void
ocean::packet_accept (int packet_type)
{
  size_t packetlen = inbufptr - inbuffer;
  if (packetlen < sizeof (outbuffer))
  {
    memcpy ((void *) outbuffer, (void *) inbuffer, packetlen);
    outbuflen = packetlen;
    outbuffer[packetlen] = '\0';
    packetType = packet_type;
#ifdef STATE_DEBUG
    report (6, "Packet type %d accepted %d = %s\n",
            packet_type, packetlen, gpsd_hexdump (outbuffer, outbuflen));
#endif /* STATE_DEBUG */
  }
  else
  {
    report (1, "Rejected too long packet type %d len %d\n",
            packet_type, packetlen);
  }
}

/* shift the input buffer to discard all data up to current input pointer */
void
ocean::packet_discard ()
{
  size_t discard = inbufptr - inbuffer;
  size_t remaining = inbuflen - discard;
  inbufptr = (unsigned char *) memmove (inbuffer, inbufptr, remaining);
  inbuflen = remaining;
#ifdef STATE_DEBUG
  report (6, "Packet discard of %d, chars remaining is %d = %s\n",
          discard, remaining, gpsd_hexdump (inbuffer, inbuflen));
#endif /* STATE_DEBUG */
}

/* shift the input buffer to discard one character and reread data */
void
ocean::character_discard ()
{
  memmove (inbuffer, inbuffer + 1, (size_t)-- inbuflen);
  inbufptr = inbuffer;
#ifdef STATE_DEBUG
  report (6, "Character discarded, buffer %d chars = %s\n",
          inbuflen, gpsd_hexdump (inbuffer, inbuflen));
#endif /* STATE_DEBUG */
}


/* entry points begin here */

/* get 0-origin big-endian words relative to start of packet buffer */
#define getword(i) (short)(session->inbuffer[2*(i)] | (session->inbuffer[2*(i)+1] << 8))


/* grab a packet; returns ether BAD_PACKET or the length */
ssize_t
ocean::packet_parse (size_t newdata)
{
#ifdef STATE_DEBUG
  report (6, "Read %d chars to buffer offset %d (total %d): %s\n",
          newdata,
          inbuflen, inbuflen + newdata, gpsd_hexdump (inbufptr, newdata));
#endif /* STATE_DEBUG */

  outbuflen = 0;
  inbuflen += newdata;
#if 0
  inbuffer[inbuflen] = '\0';

  report (5, "Input buffer: %s\n", inbuffer);
#endif
  while (inbufptr < (inbuffer + inbuflen))
  {
    unsigned char c = *inbufptr++;
    static const char *state_table[] = {
      "GROUND_STATE",           /* we don't know what packet type to expect */
      "NMEA_DOLLAR",            /* we've seen first character of NMEA leader */
      "NMEA_PUB_LEAD",          /* seen second character of NMEA G leader */
      "NMEA_LEADER_END",        /* seen end char of NMEA leader, in body */
      "NMEA_CR",                /* seen terminating \r of NMEA packet */
      "NMEA_RECOGNIZED"         /* saw trailing \n of NMEA packet */
    };
    nextstate (c);
    report (7, "%08ld: character '%c' [%02x], new state: %s\n",
            char_counter,
            (isprint (c) ? c : '.'), c, state_table[packetState]);
    char_counter++;

    if (packetState == GROUND_STATE)
    {
      character_discard ();
    }
    else if (packetState == NMEA_RECOGNIZED)
    {
      bool checksum_ok = true;
      char csum[3];
      char *trailer = (char *) inbufptr - 5;
      //report (6, "Trailing character=%x\n", trailer[0]);
      if (*trailer == '%')  //Ocean-Server doesn't follow convention and uses a percent sign
      {
        unsigned int n, crc = 0;
        for (n = 1; (char *) inbuffer + n < trailer; n++)
          crc ^= inbuffer[n];
        (void) snprintf (csum, sizeof (csum), "%02X", crc);
        checksum_ok = (toupper (csum[0]) == toupper (trailer[1])
                       && toupper (csum[1]) == toupper (trailer[2]));
        //report (6, "checksum_ok=%d\n", checksum_ok);
      }
      if (checksum_ok)
        packet_accept (NMEA_PACKET);

      packetState = GROUND_STATE;
      packet_discard ();
      break;                    // once we get a packet, get out
    }
  }                             /* while */

  return (ssize_t) newdata;
}

#undef getword

/* grab a packet; returns ether BAD_PACKET or the length */
ssize_t
ocean::packet_get ()
{
  ssize_t newdata;
  newdata =
    read (inputDevice, inbuffer + inbuflen, sizeof (inbuffer) - (inbuflen));
  if ((newdata < 0) && (errno != EAGAIN))
    return BAD_PACKET;
  else if ((newdata == 0) || ((newdata < 0) && (errno == EAGAIN)))
    return NO_PACKET;

#if (FILE_LOGGING > 0)
  write( outputFile, inbuffer, newdata );
#endif

  return packet_parse ((size_t) newdata);
}

/* return the packet machine to the ground state */
void
ocean::packet_reset ()
{
  packetState = GROUND_STATE;
  inbuflen = 0;
  inbufptr = inbuffer;
  packetType = NO_PACKET;
}

/**
 * Functions for parsing NMEA sentences.
 */

/**************************************************************************
 *
 * NMEA sentence handling begins here
 *
 **************************************************************************/
unsigned int ocean::processSystem (int count, char *field[])
{
  report (2, "processSystem message\n");
  /*
   * $S,01,270F,02,00,04,60%21
   *  1  01 Field
   *  2  minutes to discharge in unsigned 16bit Hex
   *  3  02 Field
   *  4  reserved
   *  5  03 Field
   *  6  ASCII text message
   *
   */
  time((time_t*)&server.lastTimeSystem);

  for( int index = 1; index < count; )
  {
    //report (4, "field[1]=%s\n", field[1]);
    int tmp = atoi(field[index]);
    ++index;
    switch(tmp)
    {
      case 1:
        server.timeLeft = (unsigned short)strtol( field[index], 0, 16 );
        report (5, "timeLeft=%x\n", server.timeLeft);
        break;
      case 3:
        server.message.assign(field[index]);
        //sscanf( field[index], "%s", server.message.c_str());
        report (5, "processSystem message=%s\n", server.message.c_str());
        break;
      case 4:
        server.averageCharge = (unsigned short)strtol( field[index], 0, 16 );
        report (5, "averageCharge=%x\n", server.averageCharge);
        break;
      default:
        ;
    }
    ++index;
  }

  return 0;
}

unsigned int ocean::processController (int count, char *field[])
{
  report (2, "processController message\n");
  /*
   * $C1,01,FF,02,00,03,FF,04,00,05,00,06,00,07,00%72
   *  1  01 Field
   *  2  Batteries present mask
   *  3  02 Field
   *  4  Batteries charging mask
   *  5  03 Field
   *  6  Batteries supplying power mask
   *
   *  8  reserved
   *
   *  10 charging mask
   *
   *  12 "Power No Good" status
   *
   *  14 Charge Inhibited mask
   *
   */

  time((time_t*)&server.lastTimeController);

  for( int index = 1; index < count; )
  {
    int tmp = atoi (field[index]);
    //report (5, "field[%d]=%s  field[%d]=%s\n", index, field[index], index+1, field[index+1]);
    int value = strtol(field[index+1], 0, 16);
    report (5, "switch=%d  value=%d\n", tmp, value);

/*
    if ( tmp != fieldCount)
    {
      report (1, "Error processing Controller message\n");
    }
    else
*/
    {
      switch(tmp)
      {
        case 1:
        {
          server.present = value;
        }
        break;
        case 2:
        {
          server.charging = value;
        }
        break;
        case 3:
        {
          server.discharging = value;
        }
        break;
        case 4:
        {
          server.reserved = value;
        }
        break;
        case 5:
        {
          server.powerPresent = value;
        }
        break;
        case 6:
        {
          server.powerNG = value;
        }
        break;
        case 7:
        {
          server.inhibited = value;
        }
        break;
      }
    }
    index += 2;
  }

  return 0;
}

unsigned int ocean::processBattery (int count, char *field[])
{

  report (2, "processBattery %s\n", field[0]);
  //report (5, "currentBattery=%d \n", currentBattery);
  //
  //$B11,02,000A,01,0294,03,0080,08,0B94,09,4115,0A,FFEC,0B,FEC3%3D
  //$B11,0C,000A,0D,0060,0E,0062,0F,1952,10,1A45,11,4B67,12,0519%33
  //$B11,13,FFFF,14,0000,15,41A0,16,00E0,17,0022,18,19C8,19,3840%3C
  //
  // 0   Battery number
  // 1   register number
  // 2   register value
  // ..  repeat

  unsigned int battery = (unsigned int)(field[0][2] - '0');
  --battery;
  report (5, "processBattery count=%d \n", count);
  report (5, "currentBattery=%d \n", battery);

  time((time_t*)&server.battery[battery].lastTimeBattery);
  --count;  //get past sentence type

  unsigned int regNumber;
  unsigned int value;
  unsigned int xx = 1;
  while(count > 0)
  {
    regNumber = (unsigned int)strtoul( field[xx], 0, 16);
    ++xx;
    value = strtoul( field[xx], 0, 16);
    ++xx;
    report (5, "reg[%u]=%x \n", regNumber, value);
    if(regNumber >= server.MAX_BAT_REG)
    {
      report (2, "Register greater than expected: %x  MAX_BAT_REG=%x\n", regNumber, server.MAX_BAT_REG);
    }
    else
    {
      server.battery[battery].batReg[regNumber] = value;
      server.battery[battery].batRegFlag[regNumber] = 0x1;
      time((time_t*)&server.battery[battery].batRegTime[regNumber]);
    }

    count -= 2;
  }
  
  return 0;
}

/**************************************************************************
 *
 * Entry points begin here
 *
 **************************************************************************/

/* parse an NMEA sentence, unpack it into a session structure */
unsigned int
ocean::nmea_parse ()
{
  static struct
  {
    const char *name;
    int funcNum;
  } nmea_phrase[] =
  {
    {
    "S", 1},                    //System
    {
    "C1", 2},                    //Controller
    {
    "B1", 3},                    //Battery
  };
  char buf[NMEA_MAX + 1];
  static char zeroStr[] = "0";

  int count;
  unsigned int retval = 0;
  unsigned int i;
  char *p, *field[NMEA_MAX], *s;

  /* make an editable copy of the sentence */
  strncpy (buf, (const char *) outbuffer, NMEA_MAX);

  /* discard the checksum part */
  for (p = buf; (*p != '*') && (*p >= ' ');)
    ++p;

  *p = 0;
  /* split sentence copy on commas, filling the field array */

  for (count = 0, p = (char *) buf; (p != 0) && (*p != 0);
       p = strchr (p, ','))
  {
    *p = 0;
    ++p;
    if ((*p) != ',')
    {
      field[count] = p;
    }
    else
      field[count] = zeroStr;

    ++count;
  }

  /* dispatch on field zero, the sentence tag */
  for (i = 0; i < (unsigned) (sizeof (nmea_phrase) / sizeof (nmea_phrase[0]));
       ++i)
  {
    s = field[0];
    //if (strlen (nmea_phrase[i].name) == 3)
      //s += 1;                   /* skip talker ID */
    if (strncmp (nmea_phrase[i].name, s, 2) == 0)
    {
      switch (nmea_phrase[i].funcNum)
      {
        case 1:
          processSystem (count, field);
          retval = 1;
          break;
        case 2:
          processController (count, field);
          retval = 2;
          break;
        case 3:
          processBattery (count, field);
          retval = 3;
          break;
        default:
          retval = 0;
          break;
      }

      strncpy (tag, nmea_phrase[i].name, MAXTAGLEN);
      sentenceLength = strlen ((const char *) outbuffer);
      report (5, "Got Packet: type=%s\n", tag);
      break;
    }
  }
  packetType = NO_PACKET;
  return retval;
}

/* add NMEA checksum to a possibly  *-terminated sentence */
void
ocean::nmea_add_checksum (char *sentence)
{
  unsigned char sum = '\0';
  char c, *p = sentence;

  if (*p == '$')
  {
    p++;
  }
  else
  {
    report (1, "Bad NMEA sentence: '%s'\n", sentence);
  }
  while (((c = *p) != '*') && (c != '\0'))
  {
    sum ^= c;
    p++;
  }
  *p++ = '*';
  (void) snprintf (p, 5, "%02X\r\n", (unsigned) sum);
}

/* ship a command to the OCEAN, adding * and correct checksum */
int
ocean::nmea_send (const char *fmt, ...)
{
  int status;
  char buf[BUFSIZ];
  va_list ap;

  va_start (ap, fmt);
  (void) vsnprintf (buf, sizeof (buf) - 5, fmt, ap);
  va_end (ap);
  if (fmt[0] == '$')
  {
    strcat (buf, "*");
    nmea_add_checksum (buf);
  }
  else
    strcat (buf, "\r\n");
  status = (int) write (inputDevice, buf, strlen (buf));
  if (status == (int) strlen (buf))
  {
    report (3, "=> OCEAN: %s\n", buf);
    return status;
  }
  else
  {
    report (3, "=> OCEAN: %s FAILED\n", buf);
    return -1;
  }
}

int
ocean::string_send (const char *fmt, ...)
{
  int status;
  char buf[BUFSIZ];
  va_list ap;

  va_start (ap, fmt);
  (void) vsnprintf (buf, sizeof (buf) - 5, fmt, ap);
  va_end (ap);
  status = (int) write (inputDevice, buf, strlen (buf));
  if (status == (int) strlen (buf))
  {
    report (3, "=> Ocean: %s\n", buf);
    return status;
  }
  else
  {
    report (3, "=> Ocean: %s FAILED\n", buf);
    return -1;
  }
}

