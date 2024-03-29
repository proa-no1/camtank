#include <kernel.h>
#include "kernel_cfg.h"
#include "app.h"

#include "mbed.h"
#include "HTTPServer.h"
#include "mbed_rpc.h"
#include "SDFileSystem.h"
#include "i2c_setting.h"
#include "TextLCD_SB1602E.h"

#include "GR_PEACH_Camera.h"
GR_PEACH_Camera camera;

#include "app_config.h"

#if (NETWORK_TYPE == 0)
  #include "EthernetInterface.h"
  EthernetInterface network;
#elif (NETWORK_TYPE == 1)
  #include "GR_PEACH_WlanBP3595.h"
  GR_PEACH_WlanBP3595 network;
  DigitalOut usb1en(P3_8);
#else
  #error NETWORK_TYPE error
#endif /* NETWORK_TYPE */
#if (SD_SPICH == 2)
SDFileSystem sdfs(P8_5, P8_6, P8_3, P8_4, "sdroot"); // mosi miso sclk cs name
#elif (SD_SPICH == 1)
SDFileSystem sdfs(P4_6, P4_7, P4_4, P4_5, "sdroot"); // mosi miso sclk cs name
#endif

static char i2c_setting_str_buf[I2C_SETTING_STR_BUF_SIZE];

static void TerminalWrite(Arguments* arg, Reply* r) {
    if ((arg != NULL) && (r != NULL)) {
        for (int i = 0; i < arg->argc; i++) {
            if (arg->argv[i] != NULL) {
                printf("%s", arg->argv[i]);
            }
        }
        printf("\n");
        r->putData<const char*>("ok");
    }
}

static void SetI2CfromWeb(Arguments* arg, Reply* r) {
    int result = 0;

    if (arg != NULL) {
        if (arg->argc >= 2) {
            if ((arg->argv[0] != NULL) && (arg->argv[1] != NULL)) {
                sprintf(i2c_setting_str_buf, "%s,%s", arg->argv[0], arg->argv[1]);
                result = 1;
            }
        } else if (arg->argc == 1) {
            if (arg->argv[0] != NULL) {
                sprintf(i2c_setting_str_buf, "%s", arg->argv[0]);
                result = 1;
            }
        } else {
            /* Do nothing */
        }
        /* command analysis and execute */
        if (result != 0) {
            if (i2c_setting_exe(i2c_setting_str_buf) != false) {
                r->putData<const char*>(i2c_setting_str_buf);
            }
        }
    }
}

DigitalOut dir_left(D8);
DigitalOut dir_right(D7);
PwmOut pwm_left(P5_0);      //TIOC0
PwmOut pwm_right(P8_14);    //TIOC2
PwmOut pwm_pan(P5_3);       //TIOC3
PwmOut pwm_tilt(P3_8);      //TIOC4
static double steer = 0, speed = 0;

void TankSpeed(Arguments* arg, Reply* r) {
	if((arg != NULL)
	&& (arg->argc == 1)) {

		sscanf(arg->argv[0], "%lf", &speed);

		if(speed < 0) {
			speed *= -1;
			dir_left = 1;
			dir_right = 1;
		}else{
			dir_left = 0;
			dir_right = 0;
		}
		pwm_left.write(speed * (1 + steer));
		pwm_right.write(speed * (1 - steer));
	}
	return;
}

void TankSteer(Arguments* arg, Reply* r) {
	if((arg != NULL)
	&& (arg->argc == 1)) {
		sscanf(arg->argv[0], "%lf", &steer);

		pwm_left.write(speed * (1 + steer));
		pwm_right.write(speed * (1 - steer));
	}
	return;
}

void CamPan(Arguments* arg, Reply* r) {
	if((arg != NULL)
	&& (arg->argc == 1)) {
		double val;

		sscanf(arg->argv[0], "%lf", &val);
		pwm_pan.write(0.08 - val * 0.04);
	}
	return;
}

void CamTilt(Arguments* arg, Reply* r) {
	if((arg != NULL)
	&& (arg->argc == 1)) {
		double val;

		sscanf(arg->argv[0], "%lf", &val);
		pwm_tilt.write(0.08 + val * 0.04);
	}
	return;
}

static char printlcd[2][9];

void SaveJpeg(Arguments* arg, Reply* r) {
	static int count = 0;
	int size;
	const char *p_data;
	char filename[128];
	FILE * fp;

	sprintf(filename, "/sdroot/DCIM/100PEACH/CAM%05d.jpg", count);

	strcpy(printlcd[0], "still");
	sprintf(printlcd[1],"CAM%05d", count);
	psnd_dtq(DTQID_CHARLCD, (intptr_t)1|2);

	size = snapshot_req(&p_data);
	fp = fopen(filename, "w");
	if(fp){
		fwrite(p_data, size, 1, fp);
		fclose(fp);
		printf("save_jpeg[%s}\r\n", filename);
	}
	else
	{
		printf("save_jpeg fopen error\r\n");
	}
	count++;
	return;
}

void TimeLapse(Arguments* arg, Reply* r) {
	wup_tsk(TASKID_TIMELAPSE);
	return;
}

void task_main(intptr_t exinf) {

	DigitalOut led_yellow(D13);
	led_yellow = 1;

	dly_tsk(200);
    printf("********* PROGRAM START ***********\r\n");

	pwm_left.period_us(50);
	pwm_left.write(0.0f);
	pwm_right.period_us(50);
	pwm_right.write(0.0f);

	pwm_pan.period_ms(20);
	pwm_pan.write(0.075f);
	dly_tsk(200);
	pwm_tilt.period_ms(20);
	pwm_tilt.write(0.075f);
	dly_tsk(200);

    /* Please enable this line when performing the setting from the Terminal side. */
//    Thread thread(SetI2CfromTerm, NULL, osPriorityBelowNormal, DEFAULT_STACK_SIZE);

//    camera_start();     //Camera Start
    camera.start();     //Camera Start

    RPC::add_rpc_class<RpcDigitalOut>();
    RPC::construct<RpcDigitalOut, PinName, const char*>(LED1, "led1");
    RPC::construct<RpcDigitalOut, PinName, const char*>(LED2, "led2");
    RPC::construct<RpcDigitalOut, PinName, const char*>(LED3, "led3");
    RPCFunction rpcFunc(TerminalWrite, "TerminalWrite");
    RPCFunction rpcSetI2C(SetI2CfromWeb, "SetI2CfromWeb");

	RPCFunction rpcTankSteer(TankSteer, "TankSteer");
	RPCFunction rpcTankSpeed(TankSpeed, "TankSpeed");
	RPCFunction rpcCamPan(CamPan, "CamPan");
	RPCFunction rpcCamTilt(CamTilt, "CamTilt");
	RPCFunction rpcSaveJpeg(SaveJpeg, "SaveJpeg");
	RPCFunction rpcTimeLapse(TimeLapse, "TimeLapse");

#if (NETWORK_TYPE == 1)
    //Audio Camera Shield USB1 enable for WlanBP3595
    usb1en = 1;        //Outputs high level
	dly_tsk(5);
    usb1en = 0;        //Outputs low level
	dly_tsk(5);
#endif

    printf("Network Setting up...\r\n");
#if (USE_DHCP == 1)
	while (network.init() != 0) {                             //for DHCP Server
#else
	while (network.init(IP_ADDRESS, SUBNET_MASK, DEFAULT_GATEWAY) != 0) { //for Static IP Address (IPAddress, NetMasks, Gateway)
#endif
        printf("Network Initialize Error \r\n");

		strcpy(printlcd[0], "Net Init");
		strcpy(printlcd[1], "error ");
		psnd_dtq(DTQID_CHARLCD, (intptr_t)(1|2));
    }
#if (NETWORK_TYPE == 0)
	while (network.connect() != 0) {
#else
	while (network.connect(WLAN_SSID, WLAN_PSK, WLAN_SECURITY) != 0) {
#endif
        printf("Network Connect Error \r\n");

		strcpy(printlcd[0], "Connect");
		strcpy(printlcd[1], "error ");
		psnd_dtq(DTQID_CHARLCD, (intptr_t)(1|2));
    }
    printf("MAC Address is %s\r\n", network.getMACAddress());
    printf("IP Address is %s\r\n", network.getIPAddress());
    printf("NetMask is %s\r\n", network.getNetworkMask());
    printf("Gateway Address is %s\r\n", network.getGateway());
    printf("Network Setup OK\r\n");

	led_yellow = 0;
	sprintf(printlcd[0], "%s", &(network.getIPAddress()[0]));
	sprintf(printlcd[1], "%s", &(network.getIPAddress()[8]));
	psnd_dtq(DTQID_CHARLCD, (intptr_t)(1|2));

    SnapshotHandler::attach_req(&snapshot_req);
    HTTPServerAddHandler<SnapshotHandler>("/camera"); //Camera
    FSHandler::mount("/sdroot", "/");

	mkdir("/sdroot/DCIM", 0777);
	mkdir("/sdroot/DCIM/100PEACH", 0777);

    HTTPServerAddHandler<FSHandler>("/");
    HTTPServerAddHandler<RPCHandler>("/rpc");
    HTTPServerStart(80);
}

void task_timelapse(intptr_t exinf)
{
	int n;
	int size;
	const char *p_data;
	char filename[128];
	FILE * fp;

	while(1)
	{
		slp_tsk();


		for(n = 0; n < 20; n++)
		{
			size = snapshot_req((const char**)&p_data);

			sprintf(filename, "/sdroot/DCIM/100PEACH/LAP%05d.jpg", n);
			fp = fopen(filename, "w");
			if(fp){
				fwrite(p_data, size, 1, fp);
				fclose(fp);
				printf("fwrite %s\r\n", filename);

				strcpy(printlcd[0], "timelaps");
				sprintf(printlcd[1],"%d/20", n);
				psnd_dtq(DTQID_CHARLCD, (intptr_t)1|2);

				dly_tsk(1000);
			}
			else
			{
				printf("time_lapse fopen error\r\n");
				break;
			}
		}
	}
}

void task_charlcd(intptr_t exinf)
{
	I2C					i2c( P1_3, P1_2 );	// sda, scl
	TextLCD_SB1602E		charlcd( &i2c );

	charlcd.printf(0, "%-8s\r", "CamTank");
	charlcd.printf(1, "%8s\r", "start");

	while(1)
	{
		unsigned int data;
		rcv_dtq(DTQID_CHARLCD, (intptr_t*)&data);

		if (data & 1)
		{
			charlcd.printf(0, "%-8s\r", printlcd[0]);
		}

		if (data & 2)
		{
			charlcd.printf(1, "%8s\r", printlcd[1]);
		}
	}
}
