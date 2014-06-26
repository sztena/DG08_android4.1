/* drivers/i2c/chips/mma8452.c - mma8452 compass driver
 *
 * Copyright (C) 2007-2008 HTC Corporation.
 * Author: Hou-Kun Chen <houkun.chen@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/mma8452.h>
#include <mach/gpio.h>
#include <mach/board.h> 
#include "EnDe.h"
#include <linux/timer.h>
#include <linux/cdev.h>



#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


//add by chenxiao for Standby
#ifdef CONFIG_ARCH_RK29
#define POWER_ON_state_PIN	RK29_PIN4_PD0
#define PLAY_Standby_PIN	RK29_PIN4_PD4
#endif

#ifdef CONFIG_ARCH_RK30
#define POWER_ON_state_PIN	RK30_PIN4_PD0
#define PLAY_Standby_PIN	RK30_PIN4_PD4
#endif

#define CK235_SPEED		12 * 1000
#define CK235_Version 300

#if 1
#define mmaprintk(x...) printk(x)
#else
#define mmaprintk(x...)
#endif

#if 0
#define mmaprintkd(x...) printk(x)
#else
#define mmaprintkd(x...)
#endif

#if 0
#define mmaprintkf(x...) printk(x)
#else
#define mmaprintkf(x...)
#endif

static unsigned char ck235_buffer[9];
static unsigned char ck235_APP_buffer[9];

struct SCK235_APP_data{
	unsigned int  Password ;
	unsigned char  ck235_inbuffer[8] ;
	unsigned int  ck235_outbuffer[8] ;
	unsigned char  validate ;
};
struct CK235_data {
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct cdev          cdev;
	struct class         *CK235_class;
};



static char * ck235_modname = "CKIM_mod"; 
static char * ck235_devicename = "ckim"; 
static char * ck235_classname = "CKIM_class";

static int CK235_rx_data(struct i2c_client *client, char *rxData, int length);
static int CK235_tx_data(struct i2c_client *client, char *txData, int length);



static struct i2c_client *this_client;





//#define One_Wire_Data RK29_PIN6_PA0
//#define One_Wire_Data RK29_PIN4_PD1

#define EdbgOutputDebugString printk
//#define	SETGPG11_OUTPUT	(rGPGCON |= (1<<22))
//#define	SETGPG11_OUTPUT2	(rGPGCON &= ~(1<<23))
//#define	SETGPG11_INPUT	(rGPGCON &= ~(3<<22))
#define	WR1_GPG11		gpio_set_value(One_Wire_Data, GPIO_HIGH)
#define	WR0_GPG11		gpio_set_value(One_Wire_Data, GPIO_LOW)

#define	OneWire_Read	gpio_get_value(One_Wire_Data)

unsigned char SHAVM_Message[64];
unsigned char	OW_RomID[8];
unsigned char	OW_Buffer;
unsigned char CRC8;
unsigned int DS_CRC16;

//define return constant for authenticate_DS28E10 and Write Memory subroutines
#define No_Device -2
#define CRC_Error  -1
#define UnMatch_MAC  0
#define Match_MAC  1
#define Write_OK 2
#define Write_Failed 3


//define 64-bit secret and 256-bit extension secret attrib, unique=1, same secret=0
#define BasicSecretOption 1
#define ExtensionSecretOption 1 





// Hash buffer, in wrong order for Dallas MAC
unsigned long  SHAVM_Hash[5];
// MAC buffer, in right order for Dallas MAC
unsigned char SHAVM_MAC[20];

// Temp vars for SHA calculation
static unsigned long SHAVM_MTword[80];
static long SHAVM_Temp;
static int SHAVM_cnt;

static long SHAVM_KTN[4];

unsigned char Entropy;

//define extension secret

/* unsigned char     ExtensionSecret[28] = {0x11,0x24,0x4d,0x2a,0x43,0x29,0x4d,0x2a,
                                         0x22,0x25,0x62,0x21,0x43,0x39,0x4e,0x2b,
                                         0x33,0x26,0x4d,0x2a,0x43,0x9,0x4d,0x5e,
                                         0x44,0x27,0x5d,0x20};  */


//define basic 64-bit secret for DS28E10
unsigned char	DeviceSecret[8] = {0x18,0xA0,0xB0,0xA1,0xA2,0xC3,0xD3,0xDF};

//define for opening I/O
//int fd;
//unsigned int get;
void set_output()
{
	gpio_set_value(One_Wire_Data, GPIO_HIGH);
	gpio_direction_output(One_Wire_Data, GPIO_HIGH);
}

void DS_delay(unsigned int us)
{
	unsigned int i,j;
	for(i = 0; i < us; i++)
	{
		for(j = 0; j < 6; j++)usleep_range(0,10);
	}
}
// Calling this routine takes about 1us
void delay_ow(unsigned int us)
{
/*	volatile k1;
	for(us*=10;us > 0;us--)
	{
		k1++;
		k1=0;

		//EdbgOutputDebugString ("");
										//EdbgOutputDebugString ("k1=%Xh\r\n", k1);		
	}*/
	switch (us)
	{
		case 500: 
			DS_delay(1650);
			break;
		case 70: 
			DS_delay(220);
			break;
		case 430: 
			DS_delay(1600);
			break;
		case 2: 
			DS_delay(5);
			break;
		case 10: 
			DS_delay(30);
			break;
		default: DS_delay(250);
     
	}
//	usleep(us);
}

// one wire reset
unsigned int ow_reset(void)
{
	unsigned int presence;
//	SETGPG11_OUTPUT; // set GPG11 to output
//	SETGPG11_OUTPUT2;
	set_output();
	WR0_GPG11; //pull GPG11 line low
	delay_ow(500); // leave it low for 480s
	WR1_GPG11; // allow line to return high
//	SETGPG11_INPUT; // set GPG11 to input
	gpio_direction_input(One_Wire_Data);
	delay_ow(70); // wait for presence
//	presence = ioctl(fd,1,&get); // get presence signal

	presence =OneWire_Read;
//	set_output();
	delay_ow(430); // wait for end of timeslot
	return(presence); // presence signal returned
} // presence = 0, no part = 1

// one wire read bit
unsigned int read_bit(void)
{
//	unsigned int vamm;
//	SETGPG11_OUTPUT;
//	SETGPG11_OUTPUT2;
	set_output();
	WR0_GPG11;
	delay_ow(2);
	WR1_GPG11;
//	SETGPG11_INPUT;
	gpio_direction_input(One_Wire_Data);
	delay_ow(10); // delay 15μs from start of timeslot
	//vamm = ioctl(fd,1,&get);
	
	return(OneWire_Read); // return value of GPG11 line
}


// one write write bit
void write_bit(char bitval)
{
	WR0_GPG11;
	delay_ow(2);
	if(bitval==1) WR1_GPG11;
	delay_ow(70);// hold value for remainder of timeslot
	WR1_GPG11;
	delay_ow(2);	// must add this delay because the cup frequence is 203MHZ.
}


// one wire read byte
unsigned int read_byte(void)
{
	unsigned char i;
	unsigned int value = 0;
	for (i = 0; i < 8; i++)
		{
			if(read_bit()) value |= 0x01<<i; // reads byte in, one byte at a time and then shifts it left
			delay_ow(70); // wait for rest of timeslot
		}
	return(value);
}

// one wire write byte
void write_byte(char val)
{
	unsigned char i;
	unsigned char temp; 
//	SETGPG11_OUTPUT;
//	SETGPG11_OUTPUT2;
	set_output();
	for (i = 0; i < 8; i++) // writes byte, one bit at a time
		{
			temp = val>>i; // shifts val right ‘i’ spaces
			temp &= 0x01; // copy that bit to temp
			write_bit(temp); // write bit in temp into
		}
	delay_ow(10);
}





/*--------------------------------------------------------------------------
 * Update the Dallas Semiconductor One Wire CRC (CRC8) from the global
 * variable CRC8 and the argument.  Return the updated CRC8.
 */
unsigned char dscrc_table[] = {
        0, 94,188,226, 97, 63,221,131,194,156,126, 32,163,253, 31, 65,
      157,195, 33,127,252,162, 64, 30, 95,  1,227,189, 62, 96,130,220,
       35,125,159,193, 66, 28,254,160,225,191, 93,  3,128,222, 60, 98,
      190,224,  2, 92,223,129, 99, 61,124, 34,192,158, 29, 67,161,255,
       70, 24,250,164, 39,121,155,197,132,218, 56,102,229,187, 89,  7,
      219,133,103, 57,186,228,  6, 88, 25, 71,165,251,120, 38,196,154,
      101, 59,217,135,  4, 90,184,230,167,249, 27, 69,198,152,122, 36,
      248,166, 68, 26,153,199, 37,123, 58,100,134,216, 91,  5,231,185,
      140,210, 48,110,237,179, 81, 15, 78, 16,242,172, 47,113,147,205,
       17, 79,173,243,112, 46,204,146,211,141,111, 49,178,236, 14, 80,
      175,241, 19, 77,206,144,114, 44,109, 51,209,143, 12, 82,176,238,
       50,108,142,208, 83, 13,239,177,240,174, 76, 18,145,207, 45,115,
      202,148,118, 40,171,245, 23, 73,  8, 86,180,234,105, 55,213,139,
       87,  9,235,181, 54,104,138,212,149,203, 41,119,244,170, 72, 22,
      233,183, 85, 11,136,214, 52,106, 43,117,151,201, 74, 20,246,168,
      116, 42,200,150, 21, 75,169,247,182,232, 10, 84,215,137,107, 53};
//--------
unsigned char dowcrc(unsigned char x)
{
   CRC8 = dscrc_table[CRC8 ^ x];
   return CRC8;
}


/*--------------------------------------------------------------------------
 * Calculate a new DS_CRC16 from the input data shorteger.  Return the current
 * DS_CRC16 and also update the global variable DS_CRC16.
 */
static short oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

unsigned short docrc16(unsigned short data)
{
   data = (data ^ (DS_CRC16 & 0xff)) & 0xff;
   DS_CRC16 >>= 8;

   if (oddparity[data & 0xf] ^ oddparity[data >> 4])
     DS_CRC16 ^= 0xc001;

   data <<= 6;
   DS_CRC16   ^= data;
   data <<= 1;
   DS_CRC16   ^= data;

   return DS_CRC16;
}


//----------------------------------------------------------------------
// computes a SHA given the 64 byte MT digest buffer.  The resulting 5
// long values are stored in the long array, hash.
//
// 'SHAVM_Message' - buffer containing the message digest
// 'SHAVM_Hash'    - result buffer
// 'SHAVM_MAC'     - result buffer, in order for Dallas part
//
void SHAVM_Compute()
{
   SHAVM_KTN[0]=(long)0x5a827999;
   SHAVM_KTN[1]=(long)0x6ed9eba1;
   SHAVM_KTN[2]=(long)0x8f1bbcdc;
   SHAVM_KTN[3]=(long)0xca62c1d6;   
   for(SHAVM_cnt=0; SHAVM_cnt<16; SHAVM_cnt++)
   {
      SHAVM_MTword[SHAVM_cnt]
         = (((long)SHAVM_Message[SHAVM_cnt*4]&0x00FF) << 24L)
         | (((long)SHAVM_Message[SHAVM_cnt*4+1]&0x00FF) << 16L)
         | (((long)SHAVM_Message[SHAVM_cnt*4+2]&0x00FF) << 8L)
         |  ((long)SHAVM_Message[SHAVM_cnt*4+3]&0x00FF);
   }

   for(; SHAVM_cnt<80; SHAVM_cnt++)
   {
      SHAVM_Temp
         = SHAVM_MTword[SHAVM_cnt-3]  ^ SHAVM_MTword[SHAVM_cnt-8]
         ^ SHAVM_MTword[SHAVM_cnt-14] ^ SHAVM_MTword[SHAVM_cnt-16];
      SHAVM_MTword[SHAVM_cnt]
         = ((SHAVM_Temp << 1) & 0xFFFFFFFE)
         | ((SHAVM_Temp >> 31) & 0x00000001);
   }

   SHAVM_Hash[0] = 0x67452301;
   SHAVM_Hash[1] = 0xEFCDAB89;
   SHAVM_Hash[2] = 0x98BADCFE;
   SHAVM_Hash[3] = 0x10325476;
   SHAVM_Hash[4] = 0xC3D2E1F0;

   for(SHAVM_cnt=0; SHAVM_cnt<80; SHAVM_cnt++)
   {
      SHAVM_Temp
         = ((SHAVM_Hash[0] << 5) & 0xFFFFFFE0)
         | ((SHAVM_Hash[0] >> 27) & 0x0000001F);
      if(SHAVM_cnt<20)
         SHAVM_Temp += ((SHAVM_Hash[1]&SHAVM_Hash[2])|((~SHAVM_Hash[1])&SHAVM_Hash[3]));
      else if(SHAVM_cnt<40)
         SHAVM_Temp += (SHAVM_Hash[1]^SHAVM_Hash[2]^SHAVM_Hash[3]);
      else if(SHAVM_cnt<60)
         SHAVM_Temp += ((SHAVM_Hash[1]&SHAVM_Hash[2])
                       |(SHAVM_Hash[1]&SHAVM_Hash[3])
                       |(SHAVM_Hash[2]&SHAVM_Hash[3]));
      else
         SHAVM_Temp += (SHAVM_Hash[1]^SHAVM_Hash[2]^SHAVM_Hash[3]);
      SHAVM_Temp += SHAVM_Hash[4] + SHAVM_KTN[SHAVM_cnt/20]
                  + SHAVM_MTword[SHAVM_cnt];
      SHAVM_Hash[4] = SHAVM_Hash[3];
      SHAVM_Hash[3] = SHAVM_Hash[2];
      SHAVM_Hash[2]
         = ((SHAVM_Hash[1] << 30) & 0xC0000000)
         | ((SHAVM_Hash[1] >> 2) & 0x3FFFFFFF);
      SHAVM_Hash[1] = SHAVM_Hash[0];
      SHAVM_Hash[0] = SHAVM_Temp;
   }

   //iButtons use LSB first, so we have to turn
   //the result around a little bit.  Instead of
   //result A-B-C-D-E, our result is E-D-C-B-A,
   //where each letter represents four bytes of
   //the result.
   for(SHAVM_cnt=0; SHAVM_cnt<5; SHAVM_cnt++)
   {
      SHAVM_Temp = SHAVM_Hash[4-SHAVM_cnt];
      SHAVM_MAC[((SHAVM_cnt)*4)+0] = (unsigned char)SHAVM_Temp;
      SHAVM_Temp >>= 8;
      SHAVM_MAC[((SHAVM_cnt)*4)+1] = (unsigned char)SHAVM_Temp;
      SHAVM_Temp >>= 8;
      SHAVM_MAC[((SHAVM_cnt)*4)+2] = (unsigned char)SHAVM_Temp;
      SHAVM_Temp >>= 8;
      SHAVM_MAC[((SHAVM_cnt)*4)+3] = (unsigned char)SHAVM_Temp;
   }
   
}

//Compute unique basic secret
//input parameter: 64-bit unique ROM ID
//the result in DeviceBasicSecret
void ComputeBasicSecret(unsigned char *DeviceBasicSecret)
{
   if( BasicSecretOption )
   {
//calculate unique 64-bit secret in the DS28E10-100
  	memset(&SHAVM_Message[4], 0x00, 32);
   	memcpy(SHAVM_Message, DeviceSecret, 4);
   	memset(&SHAVM_Message[36], 0xff, 4);
        SHAVM_Message[40]=OW_RomID[0]&0x3f;
  	memcpy(&SHAVM_Message[41], &OW_RomID[1], 7);
    	memcpy(&SHAVM_Message[48], DeviceSecret[4], 4);
        memset(&SHAVM_Message[52],0xff, 3);
   	SHAVM_Message[55] = 0x80;
   	memset(&SHAVM_Message[56], 0x00, 6);
   	SHAVM_Message[62] = 0x01;
   	SHAVM_Message[63] = 0xB8;

        SHAVM_Compute();     //unique basic secret now in SHAVM_MAC[]
	memcpy(DeviceBasicSecret, SHAVM_MAC, 8);   //now unique 64-bit basic secret in DeviceBasicSecret
     }
     else
     {
       memcpy(DeviceBasicSecret, DeviceSecret, 8);     //load basic 64-bit secret into DeviceBasicSecret
     }

}
//read the 64-bit ROM ID of DS28E10-100
//Output ROM ID in OW_RomID
int ReadRomID()
{
   short j1;
   int i;
// read rom id
	if ( (ow_reset())!=0 ) return false;
    	write_byte(0x33);
   	 for(j1 = 0;j1 < 8;j1++)
         {
           OW_RomID[j1] = read_byte();
         }
   //CRC8 check if reading ROM ID is right
    CRC8=0;
    for(i=0;i<8;i++)
    	dowcrc(OW_RomID[i]); //check if reading ROM ID is right by CRC8 result
    if(CRC8!=0) return false;
    else return true; 

}
//Read DS28E10 page memory data
//Input parameter: page number, register page number=4
//Output page data in GetPageData
int ReadMemoryPage(unsigned char PageNumber, char *GetPageData)
{
      short i;
      unsigned char pbuf[40], cnt; 
      if(PageNumber<4)
      {
        ow_reset();
    	write_byte(0xCC);
        cnt=0;
    	pbuf[cnt++]=0xA5;          //Function Command, Read authentication memory
    	pbuf[cnt++]=PageNumber<<5;
    	pbuf[cnt++]=PageNumber>>3;
        for(i=0;i<3;i++) write_byte(pbuf[i]);

    	for(i = 0;i < 35;i++)
        {
          pbuf[cnt++] = read_byte();
          if(i<32) GetPageData[i]=pbuf[cnt-1];
        }
// run the CRC over this part
        DS_CRC16 = 0;
        for (i = 0; i < cnt; i++) docrc16(pbuf[i]);
        if( DS_CRC16 != 0xB001) return false;  //not 0 because that the calculating result is DS_CRC16 and the reading result is inverted DS_CRC16
	else return true;
       }
       else   //read the register page
       {
         ow_reset();
    	 write_byte(0xCC);
         write_byte(0xf0);     // read memory command
         write_byte(0x88 );     // write target address LSB, register page beginning address=0x0088
         write_byte(0x00 );     // write target address MSB
         for(i=0x88;i<0x9f;i++) GetPageData[i-0x88]=read_byte();
//repeat reading to chech if error in reading data        
	 	  ow_reset();
    	 write_byte(0xCC);
         write_byte(0xf0);     // read memory command
         write_byte(0x88 );     // write target address LSB, register page beginning address=0x0088
         write_byte(0x00 );     // write target address MSB
         for(i=0x88;i<0x9f;i++) if( GetPageData[i-0x88]!=read_byte() ) break;
  	 if(i==0x9f) return true;
	 else return false;

       }
}



//authenticate DS28E10 based on Authentication Page number
//input parameter is random number in Challenge, Unique Secret enbled by UniqueSecret, Authentication page number(0,1,2)
//Output code to indicate the result in No_Device, CRC_Error, UnMatch_MAC and Match_MAC
int Authenticate_DS28E10_By_64bitSecret(unsigned char *Challenge, unsigned char UniqueSecret,unsigned char AcctPageNum)
{
	int i,j1;

	unsigned char pbuf[40], PageData[32], cnt;

	// read rom id
	if ( (ow_reset())!=0 ) return No_Device;
		write_byte(0x33);
		
   	 for(j1 = 0;j1 < 8;j1++)
     {
       OW_RomID[j1] = read_byte();
     }
   //CRC8 check if reading ROM ID is right
    CRC8=0;
    for(i=0;i<8;i++)
	{	dowcrc(OW_RomID[i]); //check if reading ROM ID is right by CRC8 result
		printk("Reading ROM ID 0x%x,",OW_RomID[i]);
	}
	printk("\n");
    if(CRC8!=0) { printk("Reading ROM ID error\n"); return No_Device; }
    else { printk("Reading ROM ID OK #####\n");}
// identify the 64-bit basic secret in the DS28E10 and load into basic 64-bit secret
   if( UniqueSecret==true )
   {
//calculate unique 64-bit secret in the DS28E10
		memset(&SHAVM_Message[4], 0x00, 32);
		memcpy(SHAVM_Message, DeviceSecret, 4);
		memset(&SHAVM_Message[36], 0xff, 4);
		SHAVM_Message[40]=OW_RomID[0]&0x3f;
		memcpy(&SHAVM_Message[41], &OW_RomID[1], 7);
		memcpy(&SHAVM_Message[48], &DeviceSecret[4], 4);
		memset(&SHAVM_Message[52],0xff, 3);
		SHAVM_Message[55] = 0x80;
		memset(&SHAVM_Message[56], 0x00, 6);
		SHAVM_Message[62] = 0x01;
		SHAVM_Message[63] = 0xB8;

        SHAVM_Compute();     //unique secret now in SHAVM_MAC[]
     }
     else
     {
       memcpy(SHAVM_MAC, DeviceSecret, 8);     //load basic 64-bit secret into SHAVM[]
     }

//read MAC from the DS28E10
/****************************************************************/
		//write Challenge
    	ow_reset();                 //reset 1-wire bus and detect response pulse
    	write_byte(0xCC);          //SKIP ROM command
		delay_ow(10);
    	write_byte(0x0f);          //Function Command, Write challenge to scratchpad
		delay_ow(10);
        for(i=0;i<12;i++) write_byte(Challenge[i]);  //write 12-byte challenge
		delay_ow(10);
        for(i=0;i<12;i++) if( read_byte()!=Challenge[i]) break;
        if(i!=12) { printk("Wrinting Scracthpad error\n"); return CRC_Error; } 
		else
		{
			 printk("Wrinting Scracthpad OK OK!!!!!!!!!!!!\n");
		}
/****************************************************************/
			//read page_data and authMAC
        ow_reset();
    	write_byte(0xCC);
        cnt=0;
    	pbuf[cnt++]=0xA5;          //Function Command, Read authentication memory
    	pbuf[cnt++]=AcctPageNum<<5;
    	pbuf[cnt++]=AcctPageNum>>3;
        for(i=0;i<3;i++) write_byte(pbuf[i]);
    	for(i = 0;i < 31;i++)
        {
          pbuf[cnt++] = read_byte();
        }

//copy the EEPROM page data to PageData
        memcpy(PageData, &pbuf[3], 28);
     
// run the CRC over this part
        DS_CRC16 = 0;
        for (i = 0; i < cnt; i++) docrc16(pbuf[i]);
        if( DS_CRC16 != 0xB001) { printk("Reading  page data error\n"); return CRC_Error; } //not 0 because that the calculating result is DS_CRC16 and the reading result is inverted DS_CRC16
		else
		{
			printk("Reading  page data ok ###########\n");
		}
//	delay_ow(2000);      //waiting for finishing SHA-1 algorithm
		msleep(2);
		msleep(2); //jj_modify jannie add
        cnt=0;
		for(i = 0;i < 22;i++)
        {
          pbuf[cnt++] = read_byte();
        }
// run the CRC over this part MAC
        DS_CRC16 = 0;
        for (i = 0; i < cnt; i++) docrc16(pbuf[i]);
        if( DS_CRC16 != 0xB001) { printk("Reading  MAC data error\n"); return CRC_Error; } //not 0 because that the calculating result is DS_CRC16 and the reading result is inverted DS_CRC16
        else{
			printk("Reading  MAC data ok ###############################\n"); 
		}
//calculate the corresponding MAC by the host, device secret reserved in SHAVM_MAC[]
		

		memcpy(&SHAVM_Message[4], PageData, 28);
		memcpy(&SHAVM_Message[32], &Challenge[8], 4);
		memcpy(&SHAVM_Message[36], Challenge, 4);
		SHAVM_Message[40] =Challenge[7];
		memcpy(&SHAVM_Message[41], OW_RomID, 7);
		memcpy(&SHAVM_Message[52], &Challenge[4], 3);
		memcpy(SHAVM_Message, SHAVM_MAC, 4);
		memcpy(&SHAVM_Message[48], &SHAVM_MAC[4], 4);
		SHAVM_Message[55] = 0x80;
		memset(&SHAVM_Message[56], 0x00, 6);
		SHAVM_Message[62] = 0x01;
		SHAVM_Message[63] = 0xB8;
        SHAVM_Compute();     //MAC generated based on authenticated page now in SHAVM_MAC[]
	/*	for(i = 0; i < 20; i++)
		{
			printk("SHAVM_MAC[%d]=0x%x,pbuf[%d]=0x%x\n",i,SHAVM_MAC[i],i,pbuf[i]);
		}*/
//Compare calculated MAC with the MAC from the DS28E10-100
        for(i=0;i<20;i++){ if( SHAVM_MAC[i]!=pbuf[i] )  break;}
        if( i==20 ){ return Match_MAC;}
        else return UnMatch_MAC;
}

int PowerOnResetDS28E10()
{
     short i,cnt=0,flag;
     unsigned char ch, pbuf[20];
     ow_reset();
     write_byte(0xCC);
     cnt=0;
	 
	 printk("PowerOnResetDS28E10########################## \n");
  // construct a packet to send
     pbuf[cnt++] = 0x55; // write memory command
     pbuf[cnt++] = 0x00; // address LSB
     pbuf[cnt++] = 0x00; 		   // address MSB

// data to be written
     for (i = 0; i < 4; i++) pbuf[cnt++] = 0xff;
// perform the block writing 
     for(i=0;i<cnt;i++) write_byte(pbuf[i]);
// for reading crc bytes from DS28E10
     pbuf[cnt++] = read_byte();
     pbuf[cnt++] = read_byte();
// check CRC
     DS_CRC16 = 0;
     for (i = 0; i < cnt; i++) docrc16(pbuf[i]);
// return result of inverted CRC
     if(DS_CRC16 != 0xB001) return false; //not 0 because that the calculating result is DS_CRC16 and the reading result is inverted DS_CRC16
//     for(i=0;i<100;i++)  delay_ow(1100);       // wait for 110 ms
     delay_ow(100);
     write_byte(0x00);     // clock 0x00 byte as required
     delay_ow(100);
     ow_reset();
     return true;
}

int DS28E10_main (void)
{
/*	int	i,j1;		
	int	j2;
	int	jjj;*/
	int ret;
	unsigned char AcctPageNum;
	//unsigned char test1; 
 	// unsigned char page_data[35];	
 	// unsigned char AuthMAC[24];
    	
	//unsigned char	Scratchpad_data[13];
	unsigned char Challenge[12] = {0x13,0x24,0x4d,0x2a,0x73,0x12,0x24,0x4d,0x2a,0x73,0x24,0x56};

//Hereinafter will show some example subroutines
	printk(" DS28E10_main #####!!!+++\n");

	if(gpio_request(One_Wire_Data, "DS28E10_OneWire")<0)
        printk( "DS28E10_main gpio_request Error ++++  \n");
	gpio_set_value(One_Wire_Data, GPIO_HIGH);
	gpio_direction_output(One_Wire_Data, GPIO_HIGH);
	
	AcctPageNum = 0x0;


//	EdbgOutputDebugString ("eboot start!\n");
 /*       
while (1)
{
  // printk( "!!++++++++DS28E10_main ++++  \n");
	gpio_set_value(One_Wire_Data, GPIO_LOW);
//	gpio_direction_output(One_Wire_Data, GPIO_LOW);
	delay_ow(2);
	gpio_set_value(One_Wire_Data, GPIO_HIGH);
//	gpio_direction_output(One_Wire_Data, GPIO_HIGH);
	delay_ow(10);
}*/
/*
	while(1)
	{
//test the authentication based on Page 0
         if((ret=Authenticate_DS28E10_By_64bitSecret(Challenge, BasicSecretOption, 0))==Match_MAC) 
         	EdbgOutputDebugString ("Pass the authentication based on Page 0\r\n");
         else
         	EdbgOutputDebugString ("Verify Failed, the ret of Authenticate is %d\r\n",ret);
//         for(jjj=0;jjj<2000;jjj++)
         msleep(2000);
     }    	


//        close(fd);
    	while(1);
    	return 0;*/
for (ret=0;ret<3;ret++)
{
	
	 if(!PowerOnResetDS28E10())  printk( "DS Power_Reset Error ++++  \n");
	 else
		{
			 switch (Authenticate_DS28E10_By_64bitSecret(Challenge, BasicSecretOption, 0))
			 	{
			 		case Match_MAC:  
						return Match_MAC;
					case No_Device:  
						return No_Device;
					case CRC_Error:  
						return CRC_Error;
					default : 
						;
			 	}
		}
}
    	return No_Device;
}



int CK235_mod_open(struct inode *inode, struct file *file)
{
//	printk(" CK235_mod_open TV-BOX C-1.0\n");

	return 0; 
}



/*
ssize_t CK235_memory_read(struct file *file, char *buf, size_t count, 
			loff_t *ppos)
{
	int  i;
	unsigned char rx_data[10];
	unsigned char ex_data[8];
	struct SCK235_APP_data  CK235_APP_data;
	struct SCK235_APP_data  *CK235_APP_data_test;

	
	printk(" CK235_memory_read TV-BOX C-1.0 \n");
	msleep(30);
	
	rx_data[0] = 0xA0;
	CK235_rx_data(this_client, &rx_data[0], 10);
	EDesEn_Crypt(rx_data, ex_data);

   //    for(i=0;i<8;i++)*(buf+i)=ex_data[i];
   	for(i=0;i<8;i++)CK235_APP_data.ck235_outbuffer[i]=ex_data[i];
	for(i=0;i<8;i++)printk(" CK235_memory_read TV-BOX C-1.0 ex_data[i]= %d\n",i,ex_data[i]);
	CK235_APP_data.Password=9394;
	for(i=0;i<8;i++)
	{	
		 if(ex_data[i]!=ck235_APP_buffer[i+1])
		 	{		 		
				CK235_APP_data.Password=0;
			 	break;
		 	}
		 if(i==7)CK235_APP_data.Password=9394;
	}


//	for(i=0;i<8;i++)printk(" CK235_memory_read TV-BOX C-1.0 %d\n",CK235_APP_data.ck235_outbuffer[i]);


	buf=&CK235_APP_data;
	CK235_APP_data_test=(struct SCK235_APP_data  *)buf;
	for(i=0;i<8;i++)printk(" CK235_memory_read TV-BOX C-1.0  2  %d\n",(*CK235_APP_data_test).ck235_outbuffer[i]);
	buf=&CK235_APP_data;
	
	count=sizeof(CK235_APP_data);
return 8;
}*/
ssize_t CK235_memory_read(struct file *file, char *buf, size_t count, 
			loff_t *ppos)
{
	int  i;
//	unsigned char rx_data[10];
	unsigned char ex_data[]={0,1,2,3,4,5,6,7};
	
//	printk(" CK235_memory_read TV-BOX C-1.0 \n");
//	rx_data[0] = 0xA0;
//	CK235_rx_data(this_client, &rx_data[0], 10);
//	EDesEn_Crypt(rx_data, ex_data);

       for(i=0;i<8;i++)*(buf+i)=ex_data[i];
/*	for(i=0;i<10;i++)
		{
			printk(KERN_INFO "++++++++CK235_memory_read ++++ rx_data[%d]=%d\n",i,rx_data[i]);
			
		}*/
	count=8;
return 8;
}
ssize_t CK235_memory_write(struct file *file, const char *buf, size_t count,
			 							loff_t *ppos)
{
	int ret = 0,i;
	struct SCK235_APP_data  *CK235_APP_data;
 	CK235_APP_data= (struct SCK235_APP_data*)buf;

	//printk(" CK235_memory_write TV-BOX C-1.0 %d\n",(*CK235_APP_data).Password);
	
	if((*CK235_APP_data).Password==9394)
	{
		for( i=0;i<8;i++) 
		{
			ck235_APP_buffer[i+1] = 0xff & (*CK235_APP_data).ck235_inbuffer[i];
			//printk( "++++++++CK235_write_reg++++  %d  \n",ck235_APP_buffer[i+1]); 			
		}

		ck235_APP_buffer[0] = 0xA0;
		ret = CK235_tx_data(this_client, &ck235_APP_buffer[0], 9);
	}

	return ret;
}
/*
static int CK235_mod_ioctl(struct inode *inode, struct file *file,
		     unsigned int cmd, unsigned long arg)
{

//	printk(" CK235_mod_ioctl TV-BOX C-1.0\n");
return 0;
} */

static struct file_operations CK235_mod_fops = {
	.owner = THIS_MODULE,
	.open = CK235_mod_open,
	.read = CK235_memory_read,
	.write = CK235_memory_write,

};


static int CK235_rx_data(struct i2c_client *client, char *rxData, int length)
{
	int ret = 0;
	char reg = rxData[0];
	ret = i2c_master_reg8_recv(client, reg, rxData, length, CK235_SPEED);
	return (ret > 0)? 0 : ret;
}

static int CK235_tx_data(struct i2c_client *client, char *txData, int length)
{
	int ret = 0;
	char reg = txData[0];
	ret = i2c_master_reg8_send(client, reg, &txData[1], length-1,CK235_SPEED);
	return (ret > 0)? 0 : ret;
}



static int mmaCK235_remove(struct i2c_client *client)
{
    struct CK235_data *pCK235_data=i2c_get_clientdata(client);
    kfree(pCK235_data);   
    this_client = NULL;
	return 0;
}

static const struct i2c_device_id mmaCK235_id[] = {
		{"SC_CK235", 0},
		{ }
};



static int CK235_write_reg(struct i2c_client *client)
{
//	unsigned char ck235_buffer[9];
	int ret = 0,i;
	unsigned long duration;
	struct timespec done_stamp = current_kernel_time();

	duration = done_stamp.tv_sec + done_stamp.tv_nsec;
	//printk( "++++++++CK235_write_reg++++  %ld  \n",duration); 

	for( i = 0; i < 8; i++) 
	{
		ck235_buffer[i+1] = 0xffff & (duration*(i+1));
		//printk( "++++++++CK235_write_reg++++  %ld\n",ck235_buffer[i+1]); 
	}

	ck235_buffer[0] = 0xA0;
	ret = CK235_tx_data(client, &ck235_buffer[0], 9);
	//printk(KERN_INFO "++++++++CK235_write_reg++++ ret=%d\n",ret);
	
	return ret;
}

static char CK235_read_reg(struct i2c_client *client)
{
	int  i;
	unsigned char rx_data[10];
	unsigned char ex_data[8];
	

	rx_data[0] = 0xA0;
	CK235_rx_data(client, &rx_data[0], 10);
	EDesEn_Crypt(rx_data, ex_data);

	for(i=0;i<8;i++)
	{
		//printk(KERN_INFO "++++++++CK235_read_reg ++++ck235_buffer[%d]=%d  ex_data[%d]=%d\n",i,ck235_buffer[i+1],i,ex_data[i]);
		if(ex_data[i]!=ck235_buffer[i+1]) return 0;
	}
//	for( i=0;i<10;i++)printk(KERN_INFO "++++++++CK235_read_reg ++++ rx_data[%d]=%d\n",i,rx_data[i+1]);
	return 1;
}

void CK235_ERROR()
{
	unsigned int CK235_error=CK235_Version;
	while(1)
	{
		printk(" Inefficacy TV-BOX C- %d\n",CK235_error);
		msleep(500);
		//gpio_set_value(POWER_ON_state_PIN,1);
		//gpio_direction_output(POWER_ON_state_PIN, 1);

		//gpio_set_value(PLAY_Standby_PIN,0);
		///gpio_direction_output(PLAY_Standby_PIN,0);
		//msleep(500);
		//gpio_set_value(POWER_ON_state_PIN,0);
		//gpio_direction_output(POWER_ON_state_PIN, 0);

		//gpio_set_value(PLAY_Standby_PIN,1);
		//gpio_direction_output(PLAY_Standby_PIN,1);
	}
}
static char CK235_get_State(struct i2c_client *client)
{

	static char ret = 0;
	printk(KERN_INFO "+++++++mma8452_get_devid ++++ \n");
	client->addr = 0x60;
	CK235_write_reg(client);
	msleep(30);
	if(!CK235_read_reg(client))
	{
		for(ret = 0; ret < 5; ret++)
		{
			ssleep(1);
			CK235_write_reg(client);
			msleep(30);
			if(CK235_read_reg(client))break;
		}
	}
	if(ret > 4)
	{
		CK235_ERROR();
	}
	return 1;
}

static int  mmaCK235_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct CK235_data *pCK235_data;
	struct CK235_platform_data *pdata = pdata = client->dev.platform_data;
	static dev_t devnum = 0;
	int result = CK235_Version;
	static int CRC_state= No_Device;
	printk( "!!@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@++++++++CK235_probe ++++  \n");
	//printk( "Rk29xx  %d\n",result);
	
	CRC_state = DS28E10_main();
	if(CRC_state == Match_MAC)
	{
		printk("DS28E10_main Match_MAC####### \n");
		return 0;
	}
	else if(CRC_state == CRC_Error)
	{
		CK235_ERROR();
	}
	else 
	{
		pCK235_data = kzalloc(sizeof(struct CK235_data), GFP_KERNEL);
		if (!pCK235_data) 
		{
			goto exit_alloc_data_failed;
		}

		pCK235_data->client = client;
		i2c_set_clientdata(client, pCK235_data);

		this_client = client;
		CK235_get_State(this_client);	


		devnum = MKDEV(147, 0);//devnum = MKDEV(250, 0);
		result = register_chrdev_region(devnum, 1, ck235_modname);
		if (result < 0)
		{
			printk( "CK235 register_chrdev_region Error\n");
		}	
		
		cdev_init(&pCK235_data->cdev, &CK235_mod_fops);
		pCK235_data->cdev.owner = THIS_MODULE;
		pCK235_data->cdev.ops = &CK235_mod_fops;
		result=cdev_add(&pCK235_data->cdev, devnum, 1);		
		if (result)
		{
		//	printk( "CK235 Bad cdev\n");
		}
		pCK235_data->CK235_class = class_create(THIS_MODULE, ck235_classname);
		if (IS_ERR(pCK235_data->CK235_class))
		{
			//	  printk( "CK235 class create failed! exiting...");
			//	  goto exit_alloc_data_failed;

		}
		else device_create(pCK235_data->CK235_class, NULL, devnum,NULL, ck235_devicename);
	}	
	return 0;

	exit_alloc_data_failed:
		CK235_ERROR();
	
	return -1;
	
}
static struct i2c_driver mmaCK235_driver = {
	.driver = {
		.name = "SC_CK235",
		.owner = THIS_MODULE,
	    },

	.probe		= &mmaCK235_probe,           
	.remove		= &mmaCK235_remove,
	.id_table 	= mmaCK235_id,
/*
#ifndef CONFIG_HAS_EARLYSUSPEND	
	.suspend = &mmaCK235_suspend,
	.resume = &mmaCK235_resume,
#endif	*/
};

static int __init mmaCK235_i2c_init(void)
{
	printk( "#########################!!!!mmaCK235_i2c_init !!!\n");
	return i2c_add_driver(&mmaCK235_driver);
}

static void __exit mmaCK235_i2c_exit(void)
{
	i2c_del_driver(&mmaCK235_driver);
}

module_init(mmaCK235_i2c_init);
module_exit(mmaCK235_i2c_exit);

