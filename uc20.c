/*
 * uc20.c
 *
 *  Created on: Nov 18, 2019
 *      Author: user
 */

#include "uc20.h"
#include "protobufc.h"
#include "rtc.h"
#include "MQTTPacket.h"
#include "fill_level_sensor.h"
#include "usart.h"

#define START_TIME (946684800)		//2000년 1월 1일 0시 0분 0초 UTC
#define _OTA_TEST_ 0
#define _SKT_USIM_USED_	(1)
AtCmdList	uc20AtList[] ={
#if _SKT_USIM_USED_
		{0, "AT+QICSGP=1,1,\"web.sktelecom.com\",\"\",\"\",0\r\n"},
		{1, "AT+QURCCFG=\"urcport\",\"uart1\"\r\n"},
		{2, "AT+QIACT=1\r\n"},
		//{3, "AT+QIOPEN=1,0,\"TCP\",\"52.33.207.242\",80,0,2\r\n"},
		{3, "AT+QIOPEN=1,0,\"TCP\",\"test.mosquitto.org\",1883,0,2\r\n"},
		{4, "AT+IFC=2,2\r\n"},
		{5, "+++"},
		{6, "AT+QCCID\r\n"},
		{7, "AT+QICLOSE=0\r\n"},
		{8, "AT+QPOWD\r\n"},
		{9, "AT+QSCLK=1\r\n"},
		{10, "AT+QRST=1,0\r\n"},
		{11, "AT+QIDEACT=1\r\n"},
};

extern void resetHardware();
extern uint16_t Decimal_number_digit(uint32_t number);

/* URL */
char pbc_ctype[] = "Content-Type: application/octet-stream\r\n\r\n";

char        DATA_TX_READY[] = ">";
char         DATA_SEND_OK[] = "SEND OK";
char      DATA_RECEIVE_OK[] = "+QIURC: \"recv\",0";
char              READ_OK[] = "HTTP/1.1 200 OK";

char            QIOPEN_OK[] = "+QIOPEN: 0,0";
char           QIOPEN_NOK[] = "+QIOPEN: 0,";
char		   CONNECT_OK[] = "CONNECT";


char fwver_ask[9] = {0x02, 0x46, 0x57, 0x56, 0x45, 0x52, 0x3f, 0x0d, 0x03};
char fw_ver_buff[50] = {0,};
char fw_download = 0;
char g_fwfile_ready = 0;
int new_version = 0;

//extern uint32_t counter;
//extern FIL FWfile;
extern CRC_HandleTypeDef hcrc;

extern uint8_t fw_write_flag;
extern uint16_t fw_down_idx;
extern uint32_t file_count;
extern uint8_t chunked_phase;
extern uint32_t chunked_size, chunked_count;

extern uint8_t RX_Buffer1[RX_BUFF_SIZE];
extern uint8_t RX_Buffer2[RX_BUFF_SIZE];

void MQTT_CONNECT()
{
	int rc = 0;
	unsigned char buf[100];
	int buflen = sizeof(buf);

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

	printf("Starting test 1 - serialization of connect and back\r\n");

	data.clientID.cstring = "me";

	data.keepAliveInterval = 20;
	data.cleansession = 1;
	data.username.cstring = ""; //"testuser";
	data.password.cstring = ""; //"testpassword";

	data.willFlag = 1;
	data.will.message.cstring = "will message";
	data.will.qos = 1;
	data.will.retained = 0;
	data.will.topicName.cstring = "will topic";

	rc = MQTTSerialize_connect(buf, buflen, &data);

	if(rc>0){
		printf("good rc from serialize connect rc was %d\r\n", rc);
		COM_sendAtString(buf, rc);
	}
}

#if 0
구독 요청에 따른 토픽을 받을 때 수신되는 데이터 
Rxd[0]	char	32 ' '	
Rxd[1]	char	2 '\002'	
Rxd[2]	char	0 '\0'	
Rxd[3]	char	0 '\0'	
Rxd[4]	char	144 '\220'	
Rxd[5]	char	3 '\003'	
Rxd[6]	char	0 '\0'	
Rxd[7]	char	1 '\001'	
Rxd[8]	char	0 '\0'	
Rxd[9]	char	48 '0'	
Rxd[10]	char	17 '\021'	
Rxd[11]	char	0 '\0'	
Rxd[12]	char	9 '\t'	
#endif
uint32_t publish_count = 0;
void MQTT_SUBSCRIBE()
{
	int rc = 0;
	unsigned char buf[100];
	int buflen = sizeof(buf);
	unsigned char dup = 0;
	unsigned short msgid = 1;
	int count = 1;
	MQTTString topicStrings = MQTTString_initializer;
	int req_qoss = 0;

	topicStrings.cstring = "substopic-jshan";

	rc = MQTTSerialize_subscribe(buf, buflen, dup, msgid, count, &topicStrings, &req_qoss);

	if(rc>0){
		printf("good rc from serialize subscribe rc was %d \r\n", rc);
		COM_sendAtString(buf, rc);
	}
}

void MQTT_PUBLISH()
{
	int rc = 0;
	unsigned char buf[100];
	int buflen = sizeof(buf);
	uint16_t distance;

	unsigned char dup = 0;
	int qos = 0;
	unsigned char retained = 0;
	unsigned short msgid = 0;
	MQTTString topicString = MQTTString_initializer;
	//unsigned char *payload = (unsigned char*)"hello world ecubelabs!!";
	unsigned char payload[16] = {0,};
	int payloadlen = 0; //strlen((char*)payload);

	distance = getRawDistance();

	sprintf(payload, "%ld,%d", publish_count++, distance);
	payloadlen = strlen((char*)payload);

	topicString.cstring = "pubtopic-jshan";

	rc = MQTTSerialize_publish(buf, buflen, dup, qos, retained, msgid, topicString, payload, payloadlen);

	if(rc>0){
		printf("good rc from serialize publish rc was %d \r\n", rc);
		COM_sendAtString(buf, rc);
	}
}


void UC20_exeAtCmd(COM_DrvTypeDef *this)
{
	char	*temp_addr = NULL;

	switch(this->atcmd)
	{
		case QIFC_C:
			COM_sendAtCmd(this, uc20AtList[4].cmdStr, SEC_30, QIFC_CHK);
			break;

		case QIFC_CHK:
			COM_checkAtCmdRsp(this, 1, QURCCFG_C);
			break;

		case QURCCFG_C:
			COM_sendAtCmd(this, uc20AtList[1].cmdStr, SEC_30, QURCCFG_CHK);
			break;

		case QURCCFG_CHK:
			COM_checkAtCmdRsp(this, 1, QICSGP_C);
			break;

		case QICSGP_C: // configure context
			COM_sendAtCmd(this, uc20AtList[0].cmdStr, SEC_30, QICSGP_CHK);
			break;

		case QICSGP_CHK:
			COM_checkAtCmdRsp(this, 1, CTZU_C);
			break;

		case CCID_C:
		    COM_sendAtCmd(this, uc20AtList[6].cmdStr, SEC_30, CCID_CHK);
		    break;

		case CCID_CHK:
			if(COM_checkAtCmd(this->ch.Rxd, OK)){
				temp_addr = strstr(this->ch.Rxd, DATA_SPACE)+2;

				if((*(temp_addr) == 0x38) && (*(temp_addr+1) == 0x39)){
					for(int shift=0, i=0; i < COM_ICCID_LEN; i++){
						if((*(temp_addr+i) < 0x30) || (*(temp_addr+i) > 0x39)){
							this->iccid[i/2] = 0x00;
						}
						else{
							shift = (i%2)?0:4;
							this->iccid[i/2] |= (*(temp_addr+i)-0x30)<<shift;
						}
					}

#if _SKT_USIM_USED_
					this->iccid[0] = 0x89; this->iccid[1] = 0x31; this->iccid[2] = 0x44; this->iccid[3] = 0x04;
					this->iccid[4] = 0x00; this->iccid[5] = 0x06; this->iccid[6] = 0x92; this->iccid[7] = 0x95;
					this->iccid[8] = 0x64; this->iccid[9] = 0x60;
#endif
					this->atcmd = CGREGQ_C;
					setAtCmdTimeout(&this->time, CLEAR);
				}
				else{
					this->state = RST;
					setAtCmdTimeout(&this->time, CLEAR);
				}
			}
			else if(COM_checkAtCmd(this->ch.Rxd, AT_ERROR)){
				this->state = RST;
				setAtCmdTimeout(&this->time, CLEAR);
			}
			break;

		case QIACT_C:
			COM_sendAtCmd(this, uc20AtList[2].cmdStr, SEC_150, QIACT_CHK);
			break;

		case QIACT_CHK:
			if(COM_checkAtCmd(this->ch.Rxd, OK)){
				this->atcmd = QIOPEN_C;
				this->fpSetAtCmdTimeout(&this->time, SEC_10);
			}
			else if(COM_checkAtCmd(this->ch.Rxd, AT_ERROR)){
				this->state = RST;
				setAtCmdTimeout(&this->time, CLEAR);
			}
			break;

		case QIOPEN_C:
			COM_sendAtCmd(this, uc20AtList[3].cmdStr, SEC_150, QIOPEN_CHK);
			break;

		case QIOPEN_CHK:
			if(COM_checkAtCmd(this->ch.Rxd, QIOPEN_OK) || COM_checkAtCmd(this->ch.Rxd, CONNECT_OK)){
//				if((SYS_getDrvParam(&SYS, STATUS, SYS_BUT_FLG) == TRUE) && (_GET_FLAG_(GPS.status, GPS_TIME_OUT) == FALSE)){
//#if _GPS_SKIP_USED_
//					this->atcmd = QISEND_C;
//#else
//					this->atcmd = QISEND_READY;
//#endif
//					this->fpSetAtCmdTimeout(&this->time, SEC_90);
//				}
//				else{
					this->atcmd = QISEND_READY;
					this->fpSetAtCmdTimeout(&this->time, SEC_30);
//				}
#if _COM_OTA_TEST_
				_SET_FLAG_(this->status, COM_OTA_FLG);
				this->atcmd = QFOTA_C;
#endif
			}
			else if(COM_checkAtCmd(this->ch.Rxd, AT_ERROR)){
				this->state = RST;
				this->fpSetAtCmdTimeout(&this->time, CLEAR);
			}
			break;

		case QISEND_READY:
//			if(_GET_FLAG_(GPS.status, GPS_TIME_OUT) || _GET_FLAG_(GPS.status, GPS_DATA_RDY)){
//				_CLR_FLAG_(GPS.status, GPS_TIME_OUT);
//				this->atcmd = QISEND_C;
//				this->fpSetAtCmdTimeout(&this->time, CLEAR);
//			}
			MQTT_CONNECT();
			initChBuffer(&this->ch);
			MQTT_SUBSCRIBE();

			FLS_PWR_ON;
			SER_setDrvParam(NULL, SER_ACT_INTR, _FLS_);
			_SET_FLAG_(this->status, FLS_PWR_CTRL);
			SER_setDrvParam(NULL, SER_FLS_INTR_CTRL, ON);

			clearChRxBuf(&FLS);

			this->atcmd = QISEND_C;
			this->fpSetAtCmdTimeout(&this->time, CLEAR);
			break;

		case QISEND_C:
			this->atcmd = QIREIVE_CHK;
			this->fpSetAtCmdTimeout(&this->time, SEC_5);
//			UC20_SEND_DATA();
			MQTT_PUBLISH();
			clearChRxBuf(&FLS);
			break;

		case QIREIVE_CHK:
#if 0
			if(COM_checkAtCmd(this->ch.Rxd, (char*)READ_OK) && COM_checkAtCmd(this->ch.Rxd, (char*)DataLength)){
				if(UC20_GET_RX_DATA(&this->ch)){
					if(MOT.log.cnt>0){
						MOT_setDrvParam(&MOT, MOT_CMP_CNT, CLEAR);
					}

					if(FLS.log.cnt>0){
						FLS.log.cnt = CLEAR;
					}

					if(SYS.simpleEventLog.count>0){
						memset(&SYS.simpleEventLog, 0, sizeof(SimpleEvtDef));
					}

					if(SYS.errorEventLog.count>0){
						memset(&SYS.errorEventLog, 0, sizeof(ErrorEvtDef));
					}

					if(SYS.debugLog>0){
						SYS.debugLog = 0;
					}

//					if(SYS.reportType>0){
//						SYS.reportType = 0;
//					}

					if(SYS.persistentEventLog.count>0){
						for(uint8_t i=1; i<CNT_OF_PERSISTENT_EVT+1; i++){
							// 종료시간이 기록되어 있으면 지속성 이벤트 완료 처리
							if(SYS.persistentEventLog.status & (65536<<i)){
								//시작과 종료 플래그 초기화
								SYS.persistentEventLog.status &= ~((65536<<i)|(1<<i));
								//지속성 이벤트 변수 초기화
								SYS.persistentEventLog.event[i-1].startTime = 0;
								SYS.persistentEventLog.event[i-1].endTime = 0;
								SYS.persistentEventLog.event[i-1].lastCheckTime = 0;
								SYS.persistentEventLog.event[i-1].type = 0;
								//지속성 이벤트 갯수 감소
								if(SYS.persistentEventLog.count)
									SYS.persistentEventLog.count--;
							}
						}
					}

					if(SYS.tmLog.count>0){
						memset(&SYS.tmLog, 0, sizeof(TM_TypeDef));
					}

					if(SYS.tpLog.count>0){
						memset(&SYS.tpLog, 0, sizeof(TP_TypeDef));
					}

					if(COM.csq>0){
						COM.csq = 0;
					}

					if(QRS.log.tagCnt>0){
						memset(&QRS.log, 0, sizeof(TagLogTypeDef));
					}

					_SET_FLAG_(this->status, COM_RXD_FLAG);

					if(_GET_FLAG_(this->status, COM_OTA_FLG)){
						this->atcmd = QFOTA_C;
					}
					else{
						this->atcmd = QIDISCONNECT_C;
					}

					this->fpSetAtCmdTimeout(&this->time, SEC_30);
				}
			}
#endif
			if(COM_checkAtCmdNull(&this->ch, (char*)"substopicLED_ON")){
				this->fpSetAtCmdTimeout(&this->time, CLEAR);
				HEARTBEAT_ON;
				initChBuffer(&this->ch);
			}
			else if(COM_checkAtCmdNull(&this->ch, (char*)"substopicLED_OFF")){
				this->fpSetAtCmdTimeout(&this->time, CLEAR);
				HEARTBEAT_OFF;
				initChBuffer(&this->ch);
			}
			else{
				for(int i=0; i<this->ch.RxCnt; i++){
					printf("%c", this->ch.Rxd[i]);
				}
			}
			break;

		case QFOTA_C:
			DEBUGMSG("erase Flash F/W region - ");
			SYS_eraseFlashRegion(FW_DL_START_ADDR, FW_DL_END_FLASH_ADDR);
			SYS_setFwUpdate();

			this->fpSetAtCmdTimeout(&this->time, SEC_180);
			this->atcmd = QFOTA_CHK;

			HAL_FLASH_Unlock();

			COM_sendAtString(Protocol, strlen(Protocol));
			COM_sendAtString(Host, strlen(Host));
			COM_sendAtString(CRLF, strlen(CRLF));
			break;

		case QFOTA_CHK:
			if(_GET_FLAG_(this->status, COM_OTA_S3A) && fw_write_flag > 0){
				if(fw_write_flag == 1){
					fw_flash_write(RX_Buffer1, RX_BUFF_SIZE,fw_down_idx);
				}
				else if(fw_write_flag == 2){
					fw_flash_write(RX_Buffer1, FW_FILE_SIZE%(RX_BUFF_SIZE),fw_down_idx);
				}
				else if(fw_write_flag == 3){
					fw_flash_write(RX_Buffer2, RX_BUFF_SIZE,fw_down_idx);
				}
				else if(fw_write_flag == 4){
					fw_flash_write(RX_Buffer2, FW_FILE_SIZE%(RX_BUFF_SIZE),fw_down_idx);
				}

				fw_write_flag = 0;
				fw_down_idx++;
			}
			else if(file_count >= FW_FILE_SIZE){
				printf("\r\n");
				this->atcmd = QFOTA_END;
				HAL_FLASH_Lock();
			}
			break;

		case QFOTA_END:
			_CLR_FLAG_(this->status, COM_OTA_FLG);
			DEBUGMSG("check CRC download F/W - ");
			if(SYS_checkCRC(FW_DL_START_ADDR, FW_DATA_LENGTH, FW_DL_CHKSUM_ADDR)){
			    _SET_FLAG_(this->status, COM_OTA_CRC);
			}
			else{
				//TBD::firmware download fail
			}
			this->atcmd = QIDISCONNECT_C;
			this->fpSetAtCmdTimeout(&this->time, CLEAR);
			break;


	  case QIDISCONNECT_C:
		  COM_sendAtCmd(this, uc20AtList[5].cmdStr, SEC_30, QIDISCONNECT_CHK);
		  break;

	  case QIDISCONNECT_CHK:
		  if(COM_checkAtCmd(this->ch.Rxd, OK) || COM_checkAtCmd(this->ch.Rxd, "NO CARRIER")){
			  this->atcmd = QICLOSE_C;
			  this->fpSetAtCmdTimeout(&this->time, CLEAR);
		  }
		  break;

	  case QICLOSE_C:
		  COM_sendAtCmd(this, uc20AtList[7].cmdStr, SEC_30, QICLOSE_CHK);
		  break;

	  case QICLOSE_CHK:
		  COM_checkAtCmdRsp(this, 1, QIDEACT_C);
		  break;

	  case QIDEACT_C:
		  COM_sendAtCmd(this, uc20AtList[11].cmdStr, SEC_40, QIDEACT_CHK);
		  break;

	  case QIDEACT_CHK:
		  COM_checkAtCmdRsp(this, 2, QPOWD_C);
		  break;

	  case QPOWD_C:
		  COM_sendAtCmd(this, uc20AtList[8].cmdStr, SEC_60, QPOWD_CHK);
		  break;

	  case QPOWD_CHK:
		  if(COM_checkAtCmd(this->ch.Rxd, OK) || COM_checkAtCmd(this->ch.Rxd, AT_ERROR)){ //
			  this->atcmd = MPWR_OFF;
			  this->fpSetAtCmdTimeout(&this->time, SEC_3);
		  }
		  break;

	  case QRST_C:
		  COM_sendAtCmd(this, uc20AtList[10].cmdStr, SEC_30, QRST_CHK);
		  break;

	  case QRST_CHK:
		  if(COM_checkAtCmd(this->ch.Rxd, OK)){
			  this->atcmd = MODEM_ON_READY;
			  this->fpSetAtCmdTimeout(&this->time, SEC_20);
		  }
		  else if(COM_checkAtCmd(this->ch.Rxd, AT_ERROR)){
			  resetHardware();
			  this->resetCnt++;
			  this->atcmd = MODEM_ON_READY;
			  this->fpSetAtCmdTimeout(&this->time, SEC_20);
		  }
		  break;

	  case MODEM_NOP:
		  __NOP();
		  break;

	  default:
		  __NOP();
		  break;
	}
}
