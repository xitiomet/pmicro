#include "Arduino.h"
#include "PlaceboMicro.h"

int freeRam()
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}


// Constructor and such I require the stream that the object will be talking on the 4 digit address
// and the name/transport of the device, the last two fields are just for human readability
PlaceboDevice::PlaceboDevice(Stream *hwSerial, char address[5], char *name, char *transport)
{
    strcpy(my_address, address);
    num_ctrls = 0;
    controls = (PlaceboControl**) malloc(50 * sizeof(PlaceboControl*));
    my_name = name;
    my_transport = transport;
    broadcast_handler = NULL;
    pSerial = hwSerial;
}

PlaceboDevice::~PlaceboDevice() {}


// This will add a control to the device it should be passed by ref
void PlaceboDevice::addControl(class PlaceboControl *control)
{
    control->addDevice(this);
    controls[num_ctrls] = control;
    num_ctrls++;
}


// When a device pairs with the server it should describe its capabilities
void PlaceboDevice::describe()
{
    pSerial->print("*D=");
    pSerial->print(my_address);
    pSerial->print(",");
    pSerial->print(my_name);
    pSerial->print(",");
    pSerial->println(my_transport);
    for(int i = 0; i < num_ctrls; i++)
        controls[i]->describe(my_address, pSerial);
    pSerial->print("*F=");
    pSerial->print(my_address);
    pSerial->print(",");
    pSerial->println(num_ctrls);
}


// When a broadcast comes in from the server, the device should handle it
// right now the only broadcast is TIME, so Time.h should be implemented to
// handle this.
void PlaceboDevice::setBroadcastFunction(changecallback f)
{
    broadcast_handler = f;
}


// Return a pointer to a device's control, this should be done by passing its
// field id, a pointer to that control will be returned.
PlaceboControl* PlaceboDevice::findControlById(char id)
{
    for(int i = 0; i < num_ctrls; i++)
    {
        if (controls[i]->isId(id))
            return controls[i];
    }
    return NULL;
}

// This function is super dangerous and currently should only be called by
// check address. It does not clear its own memory..
char* PlaceboDevice::readAddress()
{
   char *address = (char*) malloc(sizeof(char) * 5);
   for (int q = 0; q <=3; q++)
   {
     int a_byte = pSerial->read();
     if (a_byte > -1)
     {
       address[q] = (char) a_byte;
     } else {
       address[q] = ' ';
     }
   }
   address[4] = '\0';
   return address;
}

// This function checks the incoming device address on a transmission from the
// server. If the device address matches this device it returns true.
bool PlaceboDevice::checkAddress()
{
    bool is_me = false;
    char* address = readAddress();
    if (strcmp(address, my_address) == 0)
    {
        free(address);
        return true;
    } else {
        free(address);
        return is_me;
    }
}

// Push a value for a field out the stream
void PlaceboDevice::pushValue(char *field, char *value)
{
    delay(200);
    pSerial->print('$');
    pSerial->print(my_address);
    pSerial->print(',');
    pSerial->print(field[0]);
    pSerial->println(value);
}

// What to do on incoming broadcasts.
void PlaceboDevice::handleBroadcast()
{
    int bcf = pSerial->read();
    if (bcf == (int) 'T')
    {
        unsigned long pctime = 0;
        char c;
        while(pSerial->available() && c != ':')
        {
          c = pSerial->read();
          if( c >= '0' && c <= '9')
          {
            pctime = (10 * pctime) + (c - '0') ;
          }
        }
        int checksum = 0;
        while(pSerial->available() && c != '\r')
        {
          c = pSerial->read();
          if( c >= '0' && c <= '9')
          {
            checksum = (10 * checksum) + (c - '0');
          }
        }
        if (pctime % 999 == checksum)
        {
          time = pctime;
        }
        if (broadcast_handler != NULL)
            broadcast_handler();
    }
}

// return a poitner to the current device time.
unsigned long * PlaceboDevice::getTime()
{
    return &time;
}

// Handle incoming field queries from the server.
void PlaceboDevice::handleQuery()
{
    int jb = pSerial->read();
    if (jb == (int) ',')
    {
        int fld = pSerial->read();
        if (fld == '*')
        {
            describe();
        } else {
            PlaceboControl *control = findControlById(fld);
            if (control != NULL)
            {
                pushValue(control->getFieldId(), control->getValue());
            }
        }
    }
}

// handle incoming data pushes from the server.
void PlaceboDevice::handlePush()
{
    int jb = pSerial->read();
    if (jb == (int) ',')
    {
        int fld = pSerial->read();
        PlaceboControl *control = findControlById(fld);
        if (control != NULL)
        {
            int i = 0;
            int bufSize = 32;
            char *tmp = (char*) malloc(bufSize * sizeof(char));
            while(pSerial->available())
            {
                tmp[i] = (char) pSerial->read();
                if (i == bufSize)
                {
                    bufSize *= 2;
                    tmp = (char*) realloc(tmp, (bufSize + 2) * sizeof(char));
                }
                i++;
            }
            tmp[i] = '\0';
            if (control->getMode() == PLB_INPUT || control->getMode() == PLB_INPUT_POLL)
            {
                control->setValue(tmp);
            } else {
                control->pushValue();
            }
            delay(50);
            free(tmp);
        }
    }
}

// This should be put in the main loop on the arduino sketch. This will check
// for packets and manage them accordingly, this will also trigger callbacks.
void PlaceboDevice::readPackets()
{
    while(pSerial->available() > 1)
    {
        int inByte = pSerial->read();
        if (inByte >= 0)
        {
            if (inByte == '~')
            {
                handleBroadcast();
            } else if (inByte == '?') {
                if (checkAddress())
                    handleQuery();
            } else if (inByte == '@') {
                if (checkAddress())
                    handlePush();
            }
        }
    }
}

//Check to see if this control matches a field id.
bool PlaceboControl::isId(char id)
{
    if (my_field_id == id)
        return true;
    else
        return false;
}

char* PlaceboControl::getFieldId()
{
    return &my_field_id;
}

// this is called by device.describe, each field will describe itself in the
// description transmission.
void PlaceboControl::describe(char *dev_address, Stream *hs)
{
    delay(10);
    if (my_mode == PLB_INPUT || my_mode == PLB_INPUT_POLL)
        hs->print("*I=");
    else if (my_mode == PLB_OUTPUT || my_mode == PLB_OUTPUT_POLL)
        hs->print("*O=");
    hs->print(dev_address);
    hs->print(",");
    hs->print(my_field_id);
    hs->print(",");
    hs->print(my_field_name);
    hs->print(",");
    if (my_field_type == PLB_BIT)
        hs->print("BIT");
    else if (my_field_type == PLB_INT)
        hs->print("INT");
    else if (my_field_type == PLB_DEC)
        hs->print("DEC");
    else if (my_field_type == PLB_UTS)
        hs->print("UTS");
    else if (my_field_type == PLB_VAR)
        hs->print("VAR");
    else if (my_field_type == PLB_TXT)
        hs->print("TXT");
    if (my_mode == PLB_INPUT_POLL || my_mode == PLB_OUTPUT_POLL)
    {
        hs->println(",POLL");
    } else {
        hs->println("");
    }
}

// PlaceboControl constructor
PlaceboControl::PlaceboControl(char field_id, char *field_name, int type, int mode)
{
    my_field_id = field_id;
    my_field_name = field_name;
    my_field_type = type;
    my_value = NULL;
    my_mode = mode;
    event_handler = NULL;
    poll_handler = NULL;
    my_devices = (PlaceboDevice**) malloc(4 * sizeof(PlaceboDevice*));
    dev_count = 0;
}

PlaceboControl::~PlaceboControl() {}

// This function should never be called outside the addControl function in
// PlaceboDevice, damn you C++ for not having protected functions!
void PlaceboControl::addDevice(PlaceboDevice *dev)
{
    my_devices[dev_count] = dev;
    dev_count++;
}

// push the updated field to all devices it is attached to.
void PlaceboControl::pushValue()
{
    for(int i = 0; i < dev_count; i++)
        my_devices[i]->pushValue(&my_field_id, getValue());
}

char* PlaceboControl::getValue()
{
    if (poll_handler != NULL)
    {
        poll_handler();
    }
    return my_value;
}

// Basic type abstraction..
bool PlaceboControl::getBool()
{
    if (strcmp(getValue(), "1") == 0)
        return true;
    else
        return false;

}
// Basic type abstraction..
int PlaceboControl::getInt()
{
    return atoi(getValue());
}

// Basic type abstraction..
unsigned long PlaceboControl::getUnsignedLong()
{
    return strtoul(getValue(), NULL, 0);
}

// Is this an input or output control
int PlaceboControl::getMode()
{
    return my_mode;
}

// This is to set a trigger for when the field is updated from the server, this
// should call a function that handles GPIO changes.
void PlaceboControl::setChangeFunction(changecallback f)
{
    event_handler = f;
}

// This is to set a trigger for when the field is polled, this gives the
// arduino sketch a chance to modify the value before its returned to the server
void PlaceboControl::setPollFunction(pollcallback f)
{
    poll_handler = f;
}

// Basic type abstraction..
void PlaceboControl::setBool(bool val)
{
    if (val)
        setValue("1");
    else
        setValue("0");
}

// Flip a boolean
void PlaceboControl::toggleBool()
{
    setBool(!getBool());
}

// Basic type abstraction..
void PlaceboControl::setInt(int val)
{
    char *i = (char*) malloc(10 * sizeof(char));
    itoa(val,i,10);
    setValue(i);
    free(i);
}

// Basic type abstraction..
void PlaceboControl::setUnsignedLong(unsigned long val)
{
    char *i = (char*) malloc(10 * sizeof(char));
    ultoa(val,i,10);
    setValue(i);
    free(i);
}

// Basic type abstraction..
void PlaceboControl::setFloat(float val)
{
    char *i = (char*) malloc(10 * sizeof(char));
    dtostrf(val, 4, 2, i);
    setValue(i);
    free(i);
}


// setValue is called by all other set commands, The base storage for all fields
// is a char* because the types are kinda "FAKE"
void PlaceboControl::setValue(char *val)
{
    int len = strlen(val);
    free(my_value);
    my_value = (char *) malloc((len+1) * sizeof(char));
    strcpy(my_value,val);
    if (my_mode != PLB_INPUT_POLL && my_mode != PLB_OUTPUT_POLL)
        pushValue();
    delay(50);
    if (event_handler != NULL)
    {
        event_handler();
    }
}


