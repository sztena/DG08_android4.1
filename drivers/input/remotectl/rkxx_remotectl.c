/*$_rbox_$_ modify _$hzb,20120522*/
/*$_rbox_$_ modify _$add this file for rk29 remotectl*/

/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/adc.h>
#include <asm/gpio.h>
#include <mach/remotectl.h>
#include <mach/iomux.h>
#include <linux/wakelock.h>
#include <linux/suspend.h>


#if 1
#define remotectl_dbg(bdata, format, arg...)		\
	dev_printk(KERN_INFO , &bdata->input->dev , format , ## arg)
#else
#define remotectl_dbg(bdata, format, arg...)	
#endif

extern suspend_state_t get_suspend_state(void);

struct rkxx_remotectl_suspend_data{
    int suspend_flag;
    int cnt;
    long scanTime[50];
};

struct rkxx_remote_key_table{
    int scanCode;
	int keyCode;		
};

struct rkxx_remotectl_button {	
    int usercode;
    int nbuttons;
    struct rkxx_remote_key_table *key_table;
};

struct rkxx_remotectl_drvdata {
    int state;
	int nbuttons;
	int result;
    unsigned long pre_time;
    unsigned long cur_time;
    long period;
    int scanData;
    int count;
    int keybdNum;
    int keycode;
    int press;
    int pre_press;
    
    struct input_dev *input;
    struct timer_list timer;
    struct tasklet_struct remote_tasklet;
    struct wake_lock remotectl_wake_lock;
    struct rkxx_remotectl_suspend_data remotectl_suspend_data;
};



//特殊功能键值定义
    //193      //photo
    //194      //video
    //195      //music
    //196      //IE
    //197      //
    //198
    //199
    //200
    
    //183      //rorate_left
    //184      //rorate_right
    //185      //zoom out
    //186      //zoom in
  
static struct rkxx_remote_key_table remote_key_table_HJ[] = {
    {0xd8, KEY_BACK},
    {0x78, KEY_REPLY},		
    {0x60, KEY_UP},
    {0xf8, KEY_DOWN},
    {0xba, KEY_LEFT},
    {0x3a, KEY_RIGHT},  	
    {0x58, KEY_POWER},
    {0x9a, KEY_HOME},
    {0x22, KEY_VOLUMEUP},    
    {0x68, KEY_VOLUMEDOWN},
    {0xaa, TV_MEDIA_PLAY_PAUSE},
  //  {0xaa, TV_Mouse_Switch},
    {0x2a, TV_MEDIA_PREVIOUS}, 
    {0xE8, TV_MEDIA_NEXT},    
    {0x1a, KEY_MENU},
		
};
#if  1 
//----- add at 2012-1225 by chenweichao for C&D
static struct rkxx_remote_key_table remote_key_table_dg07new[] = {
	  {0xea15, KEY_POWER},
    {0xaa95, TV_Mouse_Switch},
    {0xd1ae, KEY_MUTE},
    
    {0xca35, KEY_MENU},
    {0x8877, KEY_HOME},
    {0x91ee, KEY_BACK},
    
    {0xb04f, KEY_UP},
    {0xb28d, KEY_LEFT},
    {0x9867, KEY_ENTER},
    {0xe19e, KEY_RIGHT},
    {0xf20d, KEY_DOWN},
    
    {0xd22d, KEY_PREVIOUSSONG},
    {0x906f, KEY_SEARCH}, 
    {0xa1de, KEY_VOLUMEUP},
    
    {0xe21d, KEY_NEXTSONG},
    {0xa05f, KEY_BACKSPACE},
    {0xc1be, KEY_VOLUMEDOWN},
};
//------ add end
#endif


static struct rkxx_remote_key_table remote_key_table_Egreat[] = {
#if 1
	{0xB24D, KEY_POWER },
	{0xC23D, KEY_MUTE}, 

	{0xCA35, TV_MUSIC},	
	{0xDA25, TV_Film}, 
	{0xEA15, TV_Browser},
	{0xD5AA, TV_APP},

	{0xF18E, KEY_VOLUMEDOWN}, 
	{0xB887, KEY_PREVIOUSSONG}, 
	{0xF807, KEY_NEXTSONG}, 
	{0xA857, KEY_VOLUMEUP}, 

	{0x98A7, KEY_HOME}, 
	{0x82BD, KEY_BACK},
	{0xA25D, KEY_MENU},
	{0xC5BA, TV_Mouse_Switch},

	{0xD02F, KEY_UP},
	{0x91EE, KEY_LEFT},	  
	{0xB04F, KEY_ENTER}, 		  
	{0x8877, KEY_RIGHT},			
	{0xB08F, KEY_DOWN}, 

	{0x807F, KEY_1}, 
	{0x80BF, KEY_2}, 
	{0xC03F, KEY_3}, 
	{0xC1BE, KEY_4}, 
	{0xA05F, KEY_5}, 
	{0xA09F, KEY_6}, 
	{0xE01F, KEY_7}, 
	{0xA1DE, KEY_8}, 
	{0x906F, KEY_9}, 
	{0xF00F, KEY_SPACE},   //{0xc5ba, KEY_DOT},
	{0x81FE, KEY_0}, 
	{0xE19E, KEY_BACKSPACE}, 

#else
   {0x4D,KEY_POWER },//电源			    
   {0x0F, },//出仓			    
   {0x0C,KEY_BACKSPACE },//退格		    
   {0x43,KEY_MUTE },//静音			    
   {0x1D,TV_Mouse_Switch },//信息KEY_INFO			    
 //  {0x47,KEY_SWITCHVIDEOMODE },//选时	
   {0x47,TV_Browser},	
// {0x47,TV_Mouse_Switch},	    
//   {0x44,KEY_SWITCHVIDEOMODE },//视频模式	 
   {0x44,KEY_DOT },//点
   {0x14, },//比例			    
   {0x01,KEY_1 },//数字键1		  
   {0x02,KEY_2 },//数字键2		  
   {0x03,KEY_3 },//数字键3		  
   {0x04,KEY_4 },//数字键4		  
   {0x05,KEY_5 },//数字键5		  
   {0x06,KEY_6 },//数字键6		  
   {0x07,KEY_7 },//数字键7		  
   {0x08,KEY_8 },//数字键8		  
   {0x09,KEY_9 },//数字键9		  
   {0x00,KEY_0 },//数字键0		  
   {0x1A,KEY_HOME },//首页			    
   {0x45,KEY_MENU },//菜单			    
 /*  {0x0B,KEY_UP },//方向键上	    
   {0x0E,KEY_DOWN },//方向键下	    
   {0x10,KEY_LEFT },//方向键左	    
   {0x11,KEY_RIGHT },//方向键右	    
*/
   {0x0B,KEY_UP },//方向键上	    
   {0x0E,KEY_DOWN },//方向键下	    
   {0x10,KEY_LEFT },//方向键左	    
   {0x11,KEY_RIGHT },//方向键右	  

  // {0x0D,KEY_OK },//OK	
 //  {0x0D,KEY_ENTER },//OK	
   {0x0D,KEY_REPLY },//OK
   {0x42,KEY_BACK },//返回			    
   {0x15,KEY_VOLUMEUP },//VOL+			    
   {0x1C,KEY_VOLUMEDOWN },//VOL-			    
   {0x1F,TV_MEDIA_PREVIOUS },//上一个		    
   {0x1E,TV_MEDIA_NEXT },//下一个		    
   {0x16,TV_MEDIA_MULT_FORWARD},//快退			    
   {0x40,TV_MEDIA_PLAY_PAUSE },//播放/暂停  	
   {0x19,TV_MEDIA_MULT_FORWARD},//快进			    
   {0x41,TV_MEDIA_STOP },//停止			    
   {0x46, },//循环			    
 //  {0x17,TV_Zoom_In},//音轨			    
 //  {0x18,TV_Zoom_Out},//字幕
   {0x17,KEY_VOLUMEDOWN},//音轨			    
   {0x18,KEY_VOLUMEUP},//字幕	   			    
   {0x53,TV_Film },//红 A			    
   {0x5B,TV_TV },//绿 B			    
   {0x57,TV_GAME },//黄 C			    
   {0x54,TV_APP },//蓝 D			    
   {0x1B, },//缩放			    
   {0x4A, },//慢放			    
   {0x13, },//标题			    
   {0x31,KEY_AGAIN },//A-B重复		  
   {0x12, },//书签			    
   {0x20,TV_F1 },//F1				    
   {0x21,TV_F2 },//F2				    
   {0x22,TV_F3 },//F3				    
   {0x23,TV_F4 },//F4				    
   {0xD1,TV_Setting},//数字电视			
   {0x52,KEY_WIMAX},//导视			    
   {0xD4,KEY_PAGEUP},//时移			    
   {0xC7,KEY_PAGEDOWN},//录像         
#endif
};	


static struct rkxx_remote_key_table remote_key_table_ybd[] = {
    {0xB2, KEY_BACK},
    {0x40, KEY_REPLY},		
    {0xC2, KEY_UP},
    {0x50, KEY_DOWN},
    {0x60, KEY_LEFT},
    {0x70, KEY_RIGHT},  	
    {0x58, KEY_MENU},
    {0x72, KEY_HOME},
    {0xD8, KEY_VOLUMEUP},    
    {0x1A, KEY_VOLUMEDOWN},
    {0x0A, TV_Mouse_Switch}, 
    {0xF2, KEY_TAB},
    //{0xF0, TV_Zoom_In}, 
    //{0x32, TV_Zoom_Out},  
    {0xF0, TV_ZOOM_IN}, 
    {0x32, TV_ZOOM_OUT},  
    {0xEA, KEY_POWER},
    {0x68, TV_TV},
    {0x5a, TV_Browser}, 
    {0xd0, TV_APP},    
    {0xda, TV_GAME},	
    {0x62, TV_Film},		
};
  
static struct rkxx_remote_key_table remote_key_table_meiyu_202[] = {
    {0xB0, KEY_ENTER},
    {0xA2, KEY_BACK}, 
    {0xD0, KEY_UP},
    {0x70, KEY_DOWN},
    {0x08, KEY_LEFT},
    {0x88, KEY_RIGHT},  ////////
    {0x42, KEY_HOME},     //home
    {0xA8, KEY_VOLUMEUP},
    {0x38, KEY_VOLUMEDOWN},
    {0xE2, KEY_SEARCH},     //search
    {0xB2, KEY_POWER},     //power off
    {0xC2, KEY_MUTE},       //mute
    {0xC8, KEY_MENU},

//media ctrl
    {0x78,   0x190},      //play pause
    {0xF8,   0x191},      //pre
    {0x02,   0x192},      //next

//pic
    {0xB8, 183},          //rorate left
    {0x58, 184},          //rorate right
    {0x68, 185},          //zoom out
    {0x98, 186},          //zoom in
//mouse switch
    {0xf0,388},
//display switch
    {0x82,   0x175},
};

static struct rkxx_remote_key_table remote_key_table_df[] = {
    {0xf8, KEY_REPLY},
    {0xc0, KEY_BACK}, 
    {0xf0, KEY_UP},
    {0xd8, KEY_DOWN},
    {0xd0, KEY_LEFT},
    {0xe8,KEY_RIGHT},  ////////
    {0x90, KEY_VOLUMEDOWN},
    {0x60, KEY_VOLUMEUP},
    {0x80, KEY_HOME},     //home
    {0xe0, 183},          //rorate left
    {0x10, 184},          //rorate right
    {0x20, 185},          //zoom out
    {0xa0, 186},          //zoom in
    {0x70, KEY_MUTE},       //mute
    {0x50, KEY_POWER},     //power off
    {0x40, KEY_SEARCH},     //search
};


#if  1 
//----- add at 2012-1110 by chenweichao
static struct rkxx_remote_key_table remote_key_table_grand[] = {
	  {0x88b7, KEY_POWER},
    {0x98a7, TV_APP},
    {0xb887, KEY_MUTE},
    {0xc1be, KEY_MENU},
    {0xa09f, KEY_UP},
    {0x80bf, KEY_LEFT},
    {0x807f, KEY_ENTER},
    {0xc03f, KEY_RIGHT},
    {0xa05f, KEY_DOWN},
    {0x906f, KEY_HOMEPAGE},
    {0xa1de, KEY_BACK},    
    {0x90af, TV_MUSIC},   //{0x90af, TV_Film},  // 未定义   //{0x215e, TV_MUSIC}, 
    {0xd827, TV_TV},
    {0xf807, TV_PICTURE}, //{0xf807, TV_GAME}, // 未定义  //{0x3807, TV_PICTURE},
    {0xe19e, TV_ZOOM_IN}, //{0xe19e, KEY_ZOOMIN}, //{0xe19e, TV_Zoom_In},
    {0xb04f, TV_Film}, //{0xb04f, KEY_PAGEUP},
    {0xb08f, KEY_VOLUMEUP},
    {0x81fe, TV_ZOOM_OUT}, //{0x81fe, KEY_ZOOMOUT}, //{0x81fe, TV_Zoom_Out},
    {0xf00f, TV_GAME}, //{0xf00f, KEY_PAGEDOWN},
    {0x9867, KEY_VOLUMEDOWN},
    {0xe01f, TV_Browser},
};
//------ add end
#endif

#if  1 
//----- add at 2012-1112 by chenweichao
static struct rkxx_remote_key_table remote_key_table_air_mouse[] = {
	  {0xa25d, KEY_POWER},
};
//------ add end
#endif


unsigned char HL_change(unsigned char kd )
{
	char i,n_kd=0;
	for (i=0;i<8;i++)
		{
		//	n_kd=(n_kd<< 1)|((kd>>(7-i))&1);
			n_kd=n_kd|(((kd>>(7-i))&1)<<i);
		//	printk("%i  kd=0x%x  n_kd=0x%x\n",i,kd,n_kd);
		}
	return n_kd;
}

extern suspend_state_t get_suspend_state(void);


static struct rkxx_remotectl_button remotectl_button[] = 
{
    /*{  
       .usercode = 0x202, 
       .nbuttons =  22, 
       .key_table = &remote_key_table_meiyu_202[0],
    },*/
	/*{  
       .usercode = 0xff, 
       .nbuttons =  17, 
       .key_table = &remote_key_table_ybd[0],
    },	
    {  
       .usercode = 0xdf, 
       .nbuttons =  16, 
       .key_table = &remote_key_table_df[0],
    },
    {  
       .usercode = 0x40bf, 
       .nbuttons =  14, 
       .key_table = &remote_key_table_HJ[0],
    },*/
	{  
			.usercode = 0x40bf, 
			.nbuttons =	17,
		 	.key_table = &remote_key_table_dg07new[0],
	},

	{  
       .usercode = 0x0202, 
       .nbuttons =  34, 
       .key_table = &remote_key_table_Egreat[0],
    },	

#if 0    
  //----- add at 2012-1110 by chenweichao
  {  
			.usercode = 0x1fe, 
			.nbuttons =	21, 
		 	.key_table = &remote_key_table_grand[0],
	},
	//------ add end
#endif

#if 0    
  //----- add at 2012-1112 by chenweichao
  {  
			.usercode = 0x40bd, 
			.nbuttons =	1, 
		 	.key_table = &remote_key_table_air_mouse[0],
	},
	//------ add end
#endif 
};


static int remotectl_keybdNum_lookup(struct rkxx_remotectl_drvdata *ddata)
{	
    int i;	

	printk("\33[32m [MARCO][%d]---[0x%x]---\33[0m\n", __LINE__, (ddata->scanData)&0xFFFF); 
    for (i = 0; i < sizeof(remotectl_button)/sizeof(struct rkxx_remotectl_button); i++){		
        if (remotectl_button[i].usercode == (ddata->scanData&0xFFFF)){			
            ddata->keybdNum = i;
            return 1;
        }
    }
    return 0;
}


static int remotectl_keycode_lookup(struct rkxx_remotectl_drvdata *ddata)
{	
    int i;	
    unsigned char keyData = ((ddata->scanData >> 8) & 0xff);

    //if(ddata->keybdNum == 3) keyData=HL_change(keyData);
	printk("\33[32m [MARCO][%d]-- scan --[%d]-[0x%x]---\33[0m\n", __LINE__, ddata->keybdNum, keyData); 
	//printk("0ddata->keycode=0x%x\n",keyData);
	//printk("0ddata->keybdNum=0x%x\n",ddata->keybdNum);

    for (i = 0; i < remotectl_button[ddata->keybdNum].nbuttons; i++){
        if (remotectl_button[ddata->keybdNum].key_table[i].scanCode == keyData){
			printk("1ddata->keycode=0x%x\n",remotectl_button[ddata->keybdNum].key_table[i].scanCode);
			printk("2ddata->keycode=0x%x\n",keyData);
            ddata->keycode = remotectl_button[ddata->keybdNum].key_table[i].keyCode;
            return 1;
        }
    }
    return 0;
}

static int remotectl_keycodeNotNec_lookup(struct rkxx_remotectl_drvdata *ddata)
{	
    int i;	
    //unsigned char keyData = ((ddata->scanData >> 8) & 0xff);
    unsigned int keyData = ddata->scanData;

    //if(ddata->keybdNum == 3) keyData=HL_change(keyData);
	///printk("10ddata->keycode=0x%x\n",keyData);
	///printk("10ddata->keybdNum=0x%x\n",ddata->keybdNum);

    for (i = 0; i < remotectl_button[ddata->keybdNum].nbuttons; i++){
		
		//printk("11ddata->keycode=0x%x\n",remotectl_button[ddata->keybdNum].key_table[i].scanCode);
        if (remotectl_button[ddata->keybdNum].key_table[i].scanCode == keyData){
            ddata->keycode = remotectl_button[ddata->keybdNum].key_table[i].keyCode;
            return 1;
        }
    }
    return 0;
}


static void remotectl_do_something(unsigned long  data)
{
    struct rkxx_remotectl_drvdata *ddata = (struct rkxx_remotectl_drvdata *)data;

    switch (ddata->state)
    {
        case RMC_IDLE:
        {
            ;
        }
        break;
        
        case RMC_PRELOAD:
        {
            if ((TIME_PRE_MIN < ddata->period) && (ddata->period < TIME_PRE_MAX)){
                
                ddata->scanData = 0;
                ddata->count = 0;
                ddata->state = RMC_USERCODE;
            }else{
                ddata->state = RMC_PRELOAD;
            }
            ddata->pre_time = ddata->cur_time;
            //mod_timer(&ddata->timer,jiffies + msecs_to_jiffies(130));
        }
        break;
        
        case RMC_USERCODE:
        {
            ddata->scanData <<= 1;
            ddata->count ++;

            if ((TIME_BIT1_MIN < ddata->period) && (ddata->period < TIME_BIT1_MAX)){
                ddata->scanData |= 0x01;
            }

            if (ddata->count == 0x10){//16 bit user code
                printk("u2222=0x%x\n",((ddata->scanData)&0xFFFF));
                if (remotectl_keybdNum_lookup(ddata)){
                    ddata->state = RMC_GETDATA;
                    ddata->scanData = 0;
                    ddata->count = 0;
                }else{                //user code error
                    ddata->state = RMC_PRELOAD;
                }
            }
        }
        break;
        
        case RMC_GETDATA:
        {
            ddata->count ++;
            ddata->scanData <<= 1;

          
            if ((TIME_BIT1_MIN < ddata->period) && (ddata->period < TIME_BIT1_MAX)){
                ddata->scanData |= 0x01;
            }           
            if (ddata->count == 0x10){
                printk(KERN_ERR "222222d=%x\n",(ddata->scanData&0xFFFF));

					if ( 0 ){
                    if (remotectl_keycode_lookup(ddata)){
                        ddata->press = 1;
                         if (get_suspend_state()==0){
						 	
								printk(KERN_ERR "ddata->keycode=%x\n",(ddata->keycode));
                                input_event(ddata->input, EV_KEY, ddata->keycode, 1);
                                input_sync(ddata->input);
                            }else if ((get_suspend_state())&&(ddata->keycode==KEY_POWER)){
							//jj_modify add by pirlo for solve the problem (the second suspend later,two powerkey down must to resume system 2013.01.16)
                            	input_event(ddata->input, EV_KEY, KEY_WAKEUP, 0);
	            				input_sync(ddata->input);
                                input_event(ddata->input, EV_KEY, KEY_WAKEUP, 1);
                                input_sync(ddata->input);
                            }
                        //input_event(ddata->input, EV_KEY, ddata->keycode, ddata->press);
		                //input_sync(ddata->input);
                        ddata->state = RMC_SEQUENCE;
                    }else{
                        ddata->state = RMC_PRELOAD;
                    }
                }else{

					#if 0
					ddata->state = RMC_PRELOAD;
					#else
					if (remotectl_keycodeNotNec_lookup(ddata))
					{
						ddata->press = 1;
						if (get_suspend_state()==0)
						{
							printk(KERN_ERR "ddata->keycodeqwewqe=%x\n",(ddata->keycode));
							input_event(ddata->input, EV_KEY, ddata->keycode, 1);
							input_sync(ddata->input);
						}
						else if ((get_suspend_state())&&(ddata->keycode==KEY_POWER))
						{
							//jj_modify add by pirlo for solve the problem (the second suspend later,two powerkey down must to resume system 2013.01.16)
							printk(KERN_ERR "ddata->keycodeqwe=%x\n",(ddata->keycode));
							input_event(ddata->input, EV_KEY, KEY_WAKEUP, 0);
	            			input_sync(ddata->input);
							//jj_modify end
							input_event(ddata->input, EV_KEY, KEY_WAKEUP, 1);
							input_sync(ddata->input);
						}
						//input_event(ddata->input, EV_KEY, ddata->keycode, ddata->press);
						//input_sync(ddata->input);
						ddata->state = RMC_SEQUENCE;

					}
					else
					{
						ddata->state = RMC_PRELOAD;
					}
					}

					#endif
                }
        }
        break;
             
        case RMC_SEQUENCE:{

            //printk( "S\n");
            
            if ((TIME_RPT_MIN < ddata->period) && (ddata->period < TIME_RPT_MAX)){
                ;
            }else if ((TIME_SEQ_MIN < ddata->period) && (ddata->period < TIME_SEQ_MAX)){
	            if (ddata->press == 1){
                    ddata->press = 3;
                }else if (ddata->press & 0x2){
                    ddata->press = 2;
                //input_event(ddata->input, EV_KEY, ddata->keycode, 2);
		            //input_sync(ddata->input);
                }
                //mod_timer(&ddata->timer,jiffies + msecs_to_jiffies(130));
                //ddata->state = RMC_PRELOAD;
            }
        }
        break;
       
        default:
            break;
    } 
	return;
}


#ifdef CONFIG_PM
void remotectl_wakeup(unsigned long _data)
{
    struct rkxx_remotectl_drvdata *ddata =  (struct rkxx_remotectl_drvdata*)_data;
    long *time;
    int i;

    time = ddata->remotectl_suspend_data.scanTime;

    if (get_suspend_state()){
        
        static int cnt;
       
        ddata->remotectl_suspend_data.suspend_flag = 0;
        ddata->count = 0;
        ddata->state = RMC_USERCODE;
        ddata->scanData = 0;
        
        for (i=0;i<ddata->remotectl_suspend_data.cnt;i++){
            if (((TIME_BIT1_MIN<time[i])&&(TIME_BIT1_MAX>time[i]))||((TIME_BIT0_MIN<time[i])&&(TIME_BIT0_MAX>time[i]))){
                cnt = i;
                break;;
            }
        }
        
        for (;i<cnt+32;i++){
            ddata->scanData <<= 1;
            ddata->count ++;

            if ((TIME_BIT1_MIN < time[i]) && (time[i] < TIME_BIT1_MAX)){
                ddata->scanData |= 0x01;
            }
            
            if (ddata->count == 0x10){//16 bit user code
                          
                if (ddata->state == RMC_USERCODE){
//                    printk(KERN_ERR "d=%x\n",(ddata->scanData&0xFFFF));  
                    if (remotectl_keybdNum_lookup(ddata)){
                        ddata->scanData = 0;
                        ddata->count = 0;
                        ddata->state = RMC_GETDATA;
                    }else{
                        ddata->state = RMC_PRELOAD;
                    }
                }else if (ddata->state == RMC_GETDATA){
                    if ((ddata->scanData&0x0ff) == ((~ddata->scanData >> 8)&0x0ff)){
//                        printk(KERN_ERR "d=%x\n",(ddata->scanData&0xFFFF));
                        if (remotectl_keycode_lookup(ddata)){
                             if (ddata->keycode==KEY_POWER){
                                input_event(ddata->input, EV_KEY, KEY_WAKEUP, 1);
                                input_sync(ddata->input);
                                input_event(ddata->input, EV_KEY, KEY_WAKEUP, 0);
                                input_sync(ddata->input);
                            }
                            ddata->state = RMC_PRELOAD;
                        }else{
                            ddata->state = RMC_PRELOAD;
                        }
                    }else{
                        ddata->state = RMC_PRELOAD;
                    }
                }else{
                    ddata->state = RMC_PRELOAD;
                }
            }
        }
    }
    memset(ddata->remotectl_suspend_data.scanTime,0,50*sizeof(long));
    ddata->remotectl_suspend_data.cnt= 0; 
    ddata->state = RMC_PRELOAD;
    
}

#endif


static void remotectl_timer(unsigned long _data)
{
    struct rkxx_remotectl_drvdata *ddata =  (struct rkxx_remotectl_drvdata*)_data;
    
    //printk("to\n");
    
    if(ddata->press != ddata->pre_press) {
        ddata->pre_press = ddata->press = 0;

        if (get_suspend_state()==0){
            //input_event(ddata->input, EV_KEY, ddata->keycode, 1);
            //input_sync(ddata->input);
            
			printk("remotectl_timer3 ddata->keycode=0x%x\n",ddata->keycode);
			printk("remotectl_timer3 ddata->keycode=0x%x\n",ddata->scanData);
            input_event(ddata->input, EV_KEY, ddata->keycode, 0);
		    input_sync(ddata->input);
        }else if ((get_suspend_state())&&(ddata->keycode==KEY_POWER)){
            //input_event(ddata->input, EV_KEY, KEY_WAKEUP, 1);
            //input_sync(ddata->input);
	            //jj_modify remove  by pirlo for solve the problem (the second suspend later,two powerkey down must to resume system 2013.01.16)
#if 0
            input_event(ddata->input, EV_KEY, KEY_WAKEUP, 0);
            input_sync(ddata->input);
#endif
        }
    }
#ifdef CONFIG_PM
    remotectl_wakeup(_data);
#endif
    ddata->state = RMC_PRELOAD;
}



static irqreturn_t remotectl_isr(int irq, void *dev_id)
{
    struct rkxx_remotectl_drvdata *ddata =  (struct rkxx_remotectl_drvdata*)dev_id;
    struct timeval  ts;


    ddata->pre_time = ddata->cur_time;
    do_gettimeofday(&ts);
    ddata->cur_time = ts.tv_usec;

    if (ddata->cur_time && ddata->pre_time)
        ddata->period =  ddata->cur_time - ddata->pre_time;

    tasklet_hi_schedule(&ddata->remote_tasklet); 
    if ((ddata->state==RMC_PRELOAD)||(ddata->state==RMC_SEQUENCE))
    mod_timer(&ddata->timer,jiffies + msecs_to_jiffies(130));
#ifdef CONFIG_PM
    //wake_lock_timeout(&ddata->remotectl_wake_lock, HZ);
   if ((get_suspend_state())&&(ddata->remotectl_suspend_data.cnt<50))
       ddata->remotectl_suspend_data.scanTime[ddata->remotectl_suspend_data.cnt++] = ddata->period;
#endif

    return IRQ_HANDLED;
}


static int __devinit remotectl_probe(struct platform_device *pdev)
{
    struct RKxx_remotectl_platform_data *pdata = pdev->dev.platform_data;
    struct rkxx_remotectl_drvdata *ddata;
    struct input_dev *input;
    int i, j;
    int irq;
    int error = 0;

    printk("++++++++remotectl_probe\n");

    if(!pdata) 
        return -EINVAL;

    ddata = kzalloc(sizeof(struct rkxx_remotectl_drvdata),GFP_KERNEL);
    memset(ddata,0,sizeof(struct rkxx_remotectl_drvdata));

    ddata->state = RMC_PRELOAD;
    input = input_allocate_device();
    
    if (!ddata || !input) {
        error = -ENOMEM;
        goto fail0;
    }

    platform_set_drvdata(pdev, ddata);

    input->name = pdev->name;
    input->phys = "gpio-keys/input0";
    input->dev.parent = &pdev->dev;

    input->id.bustype = BUS_HOST;
    input->id.vendor = 0x0001;
    input->id.product = 0x0001;
    input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);
    
	ddata->nbuttons = pdata->nbuttons;
	ddata->input = input;
  wake_lock_init(&ddata->remotectl_wake_lock, WAKE_LOCK_SUSPEND, "rk29_remote");
  if (pdata->set_iomux){
  	pdata->set_iomux();
  }
  error = gpio_request(pdata->gpio, "remotectl");
	if (error < 0) {
		printk("gpio-keys: failed to request GPIO %d,"
		" error %d\n", pdata->gpio, error);
		//goto fail1;
	}
	error = gpio_direction_input(pdata->gpio);
	if (error < 0) {
		pr_err("gpio-keys: failed to configure input"
			" direction for GPIO %d, error %d\n",
		pdata->gpio, error);
		gpio_free(pdata->gpio);
		//goto fail1;
	}
    irq = gpio_to_irq(pdata->gpio);
	if (irq < 0) {
		error = irq;
		pr_err("gpio-keys: Unable to get irq number for GPIO %d, error %d\n",
		pdata->gpio, error);
		gpio_free(pdata->gpio);
		goto fail1;
	}
	
	error = request_irq(irq, remotectl_isr,	IRQF_TRIGGER_FALLING , "remotectl", ddata);
	
	if (error) {
		pr_err("gpio-remotectl: Unable to claim irq %d; error %d\n", irq, error);
		gpio_free(pdata->gpio);
		goto fail1;
	}
    setup_timer(&ddata->timer,remotectl_timer, (unsigned long)ddata);
    
    tasklet_init(&ddata->remote_tasklet, remotectl_do_something, (unsigned long)ddata);
    
    for (j=0;j<sizeof(remotectl_button)/sizeof(struct rkxx_remotectl_button);j++){ 
    	printk("remotectl probe j=0x%x\n",j);
		for (i = 0; i < remotectl_button[j].nbuttons; i++) {
			unsigned int type = EV_KEY;
	        
			input_set_capability(input, type, remotectl_button[j].key_table[i].keyCode);
		}
  }
	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, error: %d\n", error);
		goto fail2;
	}
    
    input_set_capability(input, EV_KEY, KEY_WAKEUP);

	device_init_wakeup(&pdev->dev, 1);

	return 0;

fail2:
    pr_err("gpio-remotectl input_allocate_device fail\n");
	input_free_device(input);
	kfree(ddata);
fail1:
    pr_err("gpio-remotectl gpio irq request fail\n");
    free_irq(gpio_to_irq(pdata->gpio), ddata);
    del_timer_sync(&ddata->timer);
    tasklet_kill(&ddata->remote_tasklet); 
    gpio_free(pdata->gpio);
fail0: 
    pr_err("gpio-remotectl input_register_device fail\n");
    platform_set_drvdata(pdev, NULL);

	return error;
}

static int __devexit remotectl_remove(struct platform_device *pdev)
{
	struct RKxx_remotectl_platform_data *pdata = pdev->dev.platform_data;
	struct rkxx_remotectl_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
    int irq;

	device_init_wakeup(&pdev->dev, 0);
    irq = gpio_to_irq(pdata->gpio);
    free_irq(irq, ddata);
    tasklet_kill(&ddata->remote_tasklet); 
    gpio_free(pdata->gpio);

	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int remotectl_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct RKxx_remotectl_platform_data *pdata = pdev->dev.platform_data;
    struct rkxx_remotectl_drvdata *ddata = platform_get_drvdata(pdev);
    
    //ddata->remotectl_suspend_data.suspend_flag = 1;
    ddata->remotectl_suspend_data.cnt = 0;

	if (device_may_wakeup(&pdev->dev)) {
		if (pdata->wakeup) {
			int irq = gpio_to_irq(pdata->gpio);
			enable_irq_wake(irq);
		}
	}
    
	return 0;
}

static int remotectl_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct RKxx_remotectl_platform_data *pdata = pdev->dev.platform_data;

    if (device_may_wakeup(&pdev->dev)) {
        if (pdata->wakeup) {
            int irq = gpio_to_irq(pdata->gpio);
            disable_irq_wake(irq);
        }
    }

	return 0;
}

static const struct dev_pm_ops remotectl_pm_ops = {
	.suspend	= remotectl_suspend,
	.resume		= remotectl_resume,
};
#endif



static struct platform_driver remotectl_device_driver = {
	.probe		= remotectl_probe,
	.remove		= __devexit_p(remotectl_remove),
	.driver		= {
		.name	= "rkxx-remotectl",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
	    .pm	= &remotectl_pm_ops,
#endif
	},

};

static int  remotectl_init(void)
{
    printk(KERN_INFO "++++++++remotectl_init\n");
    return platform_driver_register(&remotectl_device_driver);
}


static void  remotectl_exit(void)
{
	platform_driver_unregister(&remotectl_device_driver);
    printk(KERN_INFO "++++++++remotectl_init\n");
}

module_init(remotectl_init);
module_exit(remotectl_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("rockchip");
MODULE_DESCRIPTION("Keyboard driver for CPU GPIOs");
MODULE_ALIAS("platform:gpio-keys1");


