/**
	************************************************************
	************************************************************
	************************************************************
	*	??????? 	onenet.c
	*
	*	????? 		?????
	*
	*	????? 		2017-05-08
	*
	*	????? 		V1.1
	*
	*	????? 		??onenet???????????????
	*
	*	???????	V1.0??????????????????????????????????????????????
	*				V1.1??????????????????????????????????????????????????
	************************************************************
	************************************************************
	************************************************************
**/

//?????????
#include "stm32f1xx_hal.h"

//???????
#include "esp8266.h"

//???????
#include "onenet.h"
#include "mqttkit.h"

//??
#include "base64.h"
#include "hmac_sha1.h"

//???????
#include "usart.h"

//C??
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "cJSON.h"

cJSON *raw_json, *params_json;

/* ---- 物模型变量（定义在 main.c） ---- */
/* 上报：传感器值 */
extern float temperature;
extern float humidity;
extern float light;
extern float waterlevel;
/* 上报：DHT11 实测温湿度 */
extern float dhtTemperature;
extern float dhtHumidity;
/* 上报：HC-SR04 超声波距离 */
extern float distance;
/* 接收：阈值 */
extern float temperatureMax;
extern float temperatureMin;
extern float humidityMax;
extern float humidityMin;
extern float lightMax;
extern float lightMin;
extern float waterlevelMax;
extern float waterlevelMin;
/* 接收：开关 */
extern float fanS;
extern float lightS;
extern float pumpS;

/* 控制量更新后的应用钩子（定义在 main.c） */
extern void SmartHouse_ApplyControl(void);



#define PROID			"GxSOl8476i" //???ID

#define ACCESS_KEY		"cVBBUkhPNTZkekRCRDdxTnM0WjRwcDZHd0xRbEZVY3A="  //??????

#define DEVICE_NAME		"temp"  //???????


char devid[16];

char key[48];






/*
************************************************************
*	?????????	OTA_UrlEncode
*
*	?????????	sign???????URL????
*
*	????????	sign????????
*
*	?????????	0-???	????-???
*
*	?????		+			%2B
*				???		%20
*				/			%2F
*				?			%3F
*				%			%25
*				#			%23
*				&			%26
*				=			%3D
************************************************************
*/
static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);
	
	if(sign == (void *)0 || sign_len < 28)
		return 1;
	
	for(; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;
	
	for(i = 0, j = 0; i < sign_len; i++)
	{
		switch(sign_t[i])
		{
			case '+':
				strcat(sign + j, "%2B");j += 3;
			break;
			
			case ' ':
				strcat(sign + j, "%20");j += 3;
			break;
			
			case '/':
				strcat(sign + j, "%2F");j += 3;
			break;
			
			case '?':
				strcat(sign + j, "%3F");j += 3;
			break;
			
			case '%':
				strcat(sign + j, "%25");j += 3;
			break;
			
			case '#':
				strcat(sign + j, "%23");j += 3;
			break;
			
			case '&':
				strcat(sign + j, "%26");j += 3;
			break;
			
			case '=':
				strcat(sign + j, "%3D");j += 3;
			break;
			
			default:
				sign[j] = sign_t[i];j++;
			break;
		}
	}
	
	sign[j] = 0;
	
	return 0;

}

/*
************************************************************
*	?????????	OTA_Authorization
*
*	?????????	????Authorization
*
*	????????	ver??????????????????????????????"2018-10-31"
*				res?????id
*				et?????????UTC???
*				access_key?????????
*				dev_name???????
*				authorization_buf??????token?????
*				authorization_buf_len????????????(???)
*
*	?????????	0-???	????-???
*
*	?????		????????sha1
************************************************************
*/
#define METHOD		"sha1"
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
											char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{
	
	size_t olen = 0;
	
	char sign_buf[64];								//?????????Base64?????? ?? URL??????
	char hmac_sha1_buf[64];							//???????
	char access_key_base64[64];						//????access_key??Base64??????
	char string_for_signature[72];					//????string_for_signature???????????key

//----------------------------------------------------?????????--------------------------------------------------------------------
	if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
		|| authorization_buf == (void *)0 || authorization_buf_len < 120)
		return 1;
	
//----------------------------------------------------??access_key????Base64????----------------------------------------------------
	memset(access_key_base64, 0, sizeof(access_key_base64));
	BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &olen, (unsigned char *)access_key, strlen(access_key));
	UsartPrintf(USART_DEBUG, "access_key_base64: %s\r\n", access_key_base64);
	
//----------------------------------------------------????string_for_signature-----------------------------------------------------
	memset(string_for_signature, 0, sizeof(string_for_signature));
	if(flag)
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
	else
		snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
	UsartPrintf(USART_DEBUG, "string_for_signature: %s\r\n", string_for_signature);
	
//----------------------------------------------------????-------------------------------------------------------------------------
	memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));
	
	hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
				(unsigned char *)string_for_signature, strlen(string_for_signature),
				(unsigned char *)hmac_sha1_buf);
	
	UsartPrintf(USART_DEBUG, "hmac_sha1_buf: %s\r\n", hmac_sha1_buf);
	
//----------------------------------------------------????????????Base64????------------------------------------------------------
	olen = 0;
	memset(sign_buf, 0, sizeof(sign_buf));
	BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));

//----------------------------------------------------??Base64??????????URL????---------------------------------------------------
	OTA_UrlEncode(sign_buf);
	UsartPrintf(USART_DEBUG, "sign_buf: %s\r\n", sign_buf);
	
//----------------------------------------------------????Token--------------------------------------------------------------------
	if(flag)
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
	else
		snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
	UsartPrintf(USART_DEBUG, "Token: %s\r\n", authorization_buf);
	
	return 0;

}

//==========================================================
//	?????????	OneNET_RegisterDevice
//
//	?????????	???????????????
//
//	????????	access_key?????????
//				pro_id?????ID
//				serial??????????
//				devid??????Y???devid
//				key??????Y???key
//
//	?????????	0-???		1-???
//
//	?????		
//==========================================================
_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;
	
	char authorization_buf[144];													//?????key
	
	send_ptr = malloc(send_len + 240);
	if(send_ptr == NULL)
		return result;
	
	while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
		HAL_Delay(500);
	
	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
							authorization_buf, sizeof(authorization_buf), 1);
	
	snprintf(send_ptr, 240 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
					"Authorization:%s\r\n"
					"Host:ota.heclouds.com\r\n"
					"Content-Type:application/json\r\n"
					"Content-Length:%d\r\n\r\n"
					"{\"name\":\"%s\"}",
	
					authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);
	
	ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));
	
	/*
	{
	  "request_id" : "f55a5a37-36e4-43a6-905c-cc8f958437b0",
	  "code" : "onenet_common_success",
	  "code_no" : "000000",
	  "message" : null,
	  "data" : {
		"device_id" : "589804481",
		"name" : "mcu_id_43057127",
		
	"pid" : 282932,
		"key" : "indu/peTFlsgQGL060Gp7GhJOn9DnuRecadrybv9/XY="
	  }
	}
	*/
	
	data_ptr = (char *)ESP8266_GetIPD(250);							//????????
	
	if(data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}
	
	if(data_ptr)
	{
		char name[16];
		int pid = 0;
		
		if(sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}
	
	free(send_ptr);
	ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");
	
	return result;

}

//==========================================================
//	?????????	OneNet_DevLink
//
//	?????????	??onenet????????
//
//	????????	??
//
//	?????????	1-???	0-???
//
//	?????		??onenet??????????
//==========================================================
_Bool OneNet_DevLink(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					//?????

	unsigned char *dataPtr;
	
	char authorization_buf[160];
	
	_Bool status = 1;
	
	OneNET_Authorization("2018-10-31", PROID, 1996499200, ACCESS_KEY, DEVICE_NAME,
								authorization_buf, sizeof(authorization_buf), 0);
	
	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
							"NAME: %s,	PROID: %s,	KEY:%s\r\n"
                        , DEVICE_NAME, PROID, authorization_buf);
	
	if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			//?????
		
		dataPtr = ESP8266_GetIPD(250);									//????????
		if(dataPtr != NULL)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	1\r\n");status = 0;break;
					
					case 1:UsartPrintf(USART_DEBUG, "WARN:	2\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	3\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	4\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	5\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	6\r\n");break;
					
					default:UsartPrintf(USART_DEBUG, "ERR:	7\r\n");break;
				}
			}
		}
		
		MQTT_DeleteBuffer(&mqttPacket);								//???
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");
	
	return status;
	
}

/* 从 params 中取出指定属性的数值，兼容 {"value":x} 与直接 x 两种下发布局 */
static unsigned char OneNet_GetParamValue(cJSON *params, const char *key, float *out)
{
	cJSON *item = cJSON_GetObjectItem(params, key);

	if(item == NULL)
		return 1;

	if((item->type & 0xFF) == cJSON_Object)
	{
		cJSON *v = cJSON_GetObjectItem(item, "value");
		if(v != NULL)
			item = v;
	}

	if((item->type & 0xFF) == cJSON_Number)
	{
		*out = (float)item->valuedouble;
		return 0;
	}

	if((item->type & 0xFF) == cJSON_String && item->valuestring != NULL)
	{
		*out = (float)atof(item->valuestring);
		return 0;
	}

	return 1;

}

/* 组装上行 JSON：只上报 4 个传感器属性 */
unsigned char OneNet_FillBuf(char *buf)
{
	snprintf(buf, 256,
		 "{\"id\":\"123\",\"params\":{"
		 "\"temperature\":{\"value\":%.2f},"
		 "\"humidity\":{\"value\":%.2f},"
		 "\"light\":{\"value\":%.2f},"
		 "\"waterlevel\":{\"value\":%.2f},"
		 "\"dhtTemperature\":{\"value\":%.2f},"
		 "\"dhtHumidity\":{\"value\":%.2f},"
		 "\"distance\":{\"value\":%.2f}"
		 "}}",
		 temperature, humidity, light, waterlevel, dhtTemperature, dhtHumidity, distance);

	return (unsigned char)strlen(buf);

}

//==========================================================
//	?????????	OneNet_SendData
//
//	?????????	??????????
//
//	????????	type?????????????
//
//	?????????	??
//
//	?????		
//==========================================================
void OneNet_SendData(void)
{
	
	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};												//?????
	
	char buf[256];
	
	short body_len = 0, i = 0;
	
	//UsartPrintf(USART_DEBUG, "Tips:	OneNet_SendData-MQTT\r\n");
	
	memset(buf, 0, sizeof(buf));
	
	body_len = OneNet_FillBuf(buf);																	//???????????????????????????
	
	if(body_len)
	{
		if(MQTT_PacketSaveData(PROID, DEVICE_NAME, body_len, NULL, &mqttPacket) == 0)				//???
		{
			for(; i < body_len; i++)
				mqttPacket._data[mqttPacket._len++] = buf[i];
			
			ESP8266_SendData(mqttPacket._data, mqttPacket._len);									//??????????
			//UsartPrintf(USART_DEBUG, "Send %d Bytes\r\n", mqttPacket._len);
			
			MQTT_DeleteBuffer(&mqttPacket);															//???
		}
		else
			UsartPrintf(USART_DEBUG, "WARN:	EDP_NewBuffer Failed\r\n");
	}
	
}

//==========================================================
//	?????????	OneNET_Publish
//
//	?????????	???????
//
//	????????	topic????????????
//				msg?????????
//
//	?????????	??
//
//	?????		
//==========================================================
void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//?????
	
	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);
	
	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);					//???????????????
		
		MQTT_DeleteBuffer(&mqtt_packet);										//???
	}

}

//==========================================================
//	?????????	OneNET_Subscribe
//
//	?????????	????
//
//	????????	??
//
//	?????????	??
//
//	?????		
//==========================================================
void OneNET_Subscribe(void)
{
	
	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						//?????
	
	char topic_buf[56];
	const char *topic = topic_buf;
	
	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);
	
	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);
	
	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);					//???????????????
		
		MQTT_DeleteBuffer(&mqtt_packet);										//???
	}

}

//==========================================================
//	?????????	OneNet_RevPro
//
//	?????????	????????????
//
//	????????	dataPtr?????????????
//
//	?????????	??
//
//	?????		
//==========================================================

void OneNet_RevPro(unsigned char *cmd)
{
	char *req_payload = NULL;
	char *cmdid_topic = NULL;
	unsigned short topic_len = 0;
	unsigned short req_len = 0;
	unsigned char qos = 0;
	static unsigned short pkt_id = 0;
	unsigned char type = 0;
	short result = 0;

	type = MQTT_UnPacketRecv(cmd);

	switch(type)
	{
		case MQTT_PKT_PUBLISH:
		{
			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len,
				&req_payload, &req_len, &qos, &pkt_id);
			if(result == 0 && req_payload != NULL)
			{
				cJSON *id_item = NULL;

				/* 把云端下发的原始内容打印到 USB 虚拟串口 */
				UsartPrintf(USART_DEBUG, "[OneNET] recv topic=%s\r\n",
							(cmdid_topic != NULL) ? cmdid_topic : "?");
				UsartPrintf(USART_DEBUG, "[OneNET] recv payload=%s\r\n", req_payload);
				char reply_topic[96];
				char reply_msg[128];
				char req_id[32] = "123";
				raw_json = cJSON_Parse(req_payload);
				if(raw_json != NULL)
				{
					id_item = cJSON_GetObjectItem(raw_json, "id");
					if(id_item != NULL)
					{
						if((id_item->type & 0xFF) == cJSON_String && id_item->valuestring != NULL)
						{
							snprintf(req_id, sizeof(req_id), "%s", id_item->valuestring);
						}
						else if((id_item->type & 0xFF) == cJSON_Number)
						{
							snprintf(req_id, sizeof(req_id), "%d", id_item->valueint);
						}
					}

					params_json = cJSON_GetObjectItem(raw_json, "params");
					if(params_json != NULL)
					{
						/* 阈值与开关：仅接收，不上报 */
						OneNet_GetParamValue(params_json, "temperatureMax", &temperatureMax);
						OneNet_GetParamValue(params_json, "temperatureMin", &temperatureMin);
						OneNet_GetParamValue(params_json, "humidityMax",    &humidityMax);
						OneNet_GetParamValue(params_json, "humidityMin",    &humidityMin);
						OneNet_GetParamValue(params_json, "lightMax",       &lightMax);
						OneNet_GetParamValue(params_json, "lightMin",       &lightMin);
						OneNet_GetParamValue(params_json, "waterlevelMax",  &waterlevelMax);
						OneNet_GetParamValue(params_json, "waterlevelMin",  &waterlevelMin);
						OneNet_GetParamValue(params_json, "fanS",           &fanS);
						OneNet_GetParamValue(params_json, "lightS",         &lightS);
						OneNet_GetParamValue(params_json, "pumpS",          &pumpS);

						UsartPrintf(USART_DEBUG,
							"[OneNET] Set T[%.1f,%.1f] H[%.1f,%.1f] L[%.1f,%.1f] W[%.1f,%.1f] fan=%.0f light=%.0f pump=%.0f\r\n",
							temperatureMin, temperatureMax,
							humidityMin, humidityMax,
							lightMin, lightMax,
							waterlevelMin, waterlevelMax,
							fanS, lightS, pumpS);

						SmartHouse_ApplyControl();
					}

					/* Reply property/set to avoid cloud timeout (10411) */
					snprintf(reply_topic, sizeof(reply_topic),
						 "$sys/%s/%s/thing/property/set_reply", PROID, DEVICE_NAME);
					snprintf(reply_msg, sizeof(reply_msg),
						 "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", req_id);
					OneNET_Publish(reply_topic, reply_msg);

					cJSON_Delete(raw_json);
					raw_json = NULL;
					params_json = NULL;
				}
			}
			break;
		}

		case MQTT_PKT_PUBACK:
			(void)MQTT_UnPacketPublishAck(cmd);
			break;

		case MQTT_PKT_SUBACK:
			(void)MQTT_UnPacketSubscribe(cmd);
			break;

		default:
			result = -1;
			break;
	}

	ESP8266_Clear();

	/* 仅在解析成功时释放。
	 * 注意：MQTT_UnPacketPublish 失败路径中如果 payload 分配失败，
	 * 其内部已经 free 过 topic 却没置空，这里再释放就是 double free，
	 * 会破坏堆 → HardFault（现象就是日志突然“卡死”）。 */
	if(result == 0 && (type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH))
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

	(void)result;

}

//==========================================================
//	??? OneNet_RevPro_Poll
//
//	?????? ?????? ESP8266 ?????? MQTT ???????
//
//	?????? ??
//
//	??????? ??
//==========================================================
void OneNet_RevPro_Poll(void)
{
	unsigned char *dataPtr = ESP8266_GetIPD(10);

	if(dataPtr != NULL)
		OneNet_RevPro(dataPtr);

}
