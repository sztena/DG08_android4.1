extern unsigned char EDesEn_Crypt(unsigned char *ck235_des_data,unsigned char *pDataBuffer);
//extern static int  mmaCK235_probe(struct i2c_client *client, const struct i2c_device_id *id);
//extern static int mmaCK235_remove(struct i2c_client *client);
//extern static const struct i2c_device_id mmaCK235_id[];
//#define One_Wire_Data RK29_PIN4_PD1
#ifdef CONFIG_ARCH_RK29
#define One_Wire_Data RK29_PIN6_PA0
#endif 
#ifdef CONFIG_ARCH_RK30
#define One_Wire_Data RK30_PIN4_PD0
#endif 

