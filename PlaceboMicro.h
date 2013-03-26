#ifndef PlaceboMicro_h
#define PlaceboMicro_h
#include "Arduino.h"

#define PLB_BIT (1)
#define PLB_BOOL (1)
#define PLB_INT (2)
#define PLB_DEC (3)
#define PLB_UTS (4)
#define PLB_VAR (5)
#define PLB_TXT (6)

#define PLB_INPUT (101)
#define PLB_OUTPUT (102)
#define PLB_INPUT_POLL (103)
#define PLB_OUTPUT_POLL (104)

typedef void (*changecallback)();
typedef void (*pollcallback)();

int freeRam();

class PlaceboControl;

class PlaceboDevice
{
private:
    PlaceboControl **controls;
    Stream *pSerial;
    int num_ctrls;
    unsigned long time;
public:
    changecallback broadcast_handler;
    char my_address[5];
    char *my_name;
    char *my_transport;
    PlaceboDevice(Stream *hwSerial, char address[5], char *name, char *transport);
    ~PlaceboDevice();
    char *readAddress();
    bool checkAddress();
    void describe();
    void setBroadcastFunction(changecallback f);
    void addControl(class PlaceboControl *control);
    void pushValue(char *field, char *value);
    void handleBroadcast();
    unsigned long *getTime();
    void handleQuery();
    void handlePush();
    void readPackets();
    PlaceboControl* findControlById(char id);
};

class PlaceboControl
{
private:
    char my_field_id;
    char *my_field_name;
    int my_field_type;
    int my_mode;
    char *my_value;
    PlaceboDevice **my_devices;
    int dev_count;
public:
    PlaceboControl(char field_id, char *field_name, int type, int mode);
    ~PlaceboControl();
    changecallback event_handler;
    pollcallback poll_handler;
    void pushValue();
    void addDevice(PlaceboDevice *dev);
    void describe(char *dev_address, Stream *hs);
    bool isId(char id);
    int getMode();
    char *getFieldId();
    char *getValue();
    bool getBool();
    int getInt();
    unsigned long getUnsignedLong();

    void setBool(bool val);
    void toggleBool();

    void setValue(char *val);
    void setUnsignedLong(unsigned long val);
    void setInt(int val);
    void setFloat(float val);

    void setChangeFunction(changecallback f);
    void setPollFunction(pollcallback f);
};
#endif
