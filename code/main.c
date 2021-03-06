/* ********************************************************************** */
/* 
 * 
 * 
 */
/* ********************************************************************** */

//
/* ********************************************************************** */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <math.h>
#include "ctype.h"
#include "led.h"
#include "CRC16.h"
#include "config.h"
/* ********************************************************************** */

//#define MYDEBUG
#define ONE_MODE_DL 0   /*电流*/
#define ONE_MODE_LL 1   /*流量*/
#define ONE_MODE_YW 2   /*液位*/
#define ONE_MODE_WD 3   /*温度*/
#define ONE_MODE_YL 4   /*压力*/

#define MEASURE_MODE_NUMBER  ONE_MODE_DL

#define ONE_MODE

//#define LL_DEBUG

//
/* ********************************************************************** */
void ErrorDisplay(unsigned int ErrCode);
/* ********************************************************************** */


/* ********************************************************************** */
#ifndef MYDEBUG
/*
static YI_BOAI_REG REG_MAP[] = {
    {ADDR_DEV,def_DeviceAddr},
    {ADDR_CID,CODE_CID},
    {ADDR_PID,CODE_PID},
    {ADDR_VID,CODE_VID},
    {ADDR_FID,CODE_FID},
    {ADDR_XSD,},
    {ADDR_F1_VALUE,},
    {ADDR_F2_VALUE,},
}
*/
unsigned char ADDR_EEP __attribute__((section(".eeprom"))) = def_DeviceAddr;
unsigned char MEASURE_MODE_EEP __attribute__((section(".eeprom"))) = (MEASURE_MODE_NUMBER);

unsigned int  I_MAX_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MAX(I_RangeMAX,I_RangeMIN);
unsigned int  I_MIN_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MIN(I_RangeMAX,I_RangeMIN);
unsigned char I_XSD_EEP  __attribute__((section(".eeprom"))) = I_DEF_XiaoShuDian;
unsigned int I_ZERO_EEP  __attribute__((section(".eeprom"))) = I_ZERO;

unsigned int  T_MAX_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MAX(T_RangeMAX,T_RangeMIN);
unsigned int  T_MIN_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MIN(T_RangeMAX,T_RangeMIN);
unsigned char T_XSD_EEP  __attribute__((section(".eeprom"))) = T_DEF_XiaoShuDian;
unsigned int T_ZERO_EEP  __attribute__((section(".eeprom"))) = T_ZERO;

unsigned int  Y_MAX_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MAX(Y_RangeMAX,Y_RangeMIN);
unsigned int  Y_MIN_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MIN(Y_RangeMAX,Y_RangeMIN);
unsigned char Y_XSD_EEP  __attribute__((section(".eeprom"))) = Y_DEF_XiaoShuDian;
unsigned int Y_ZERO_EEP  __attribute__((section(".eeprom"))) = Y_ZERO;
unsigned int Y_H_EEP  __attribute__((section(".eeprom"))) = DEF_YW_H;

unsigned int  L_MAX_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MAX(L_RangeMAX,L_RangeMIN);
unsigned int  L_MIN_EEP  __attribute__((section(".eeprom"))) = DEF_VALUE_MIN(L_RangeMAX,L_RangeMIN);
unsigned char L_XSD_EEP  __attribute__((section(".eeprom"))) = L_DEF_XiaoShuDian;
unsigned int L_ZERO_EEP  __attribute__((section(".eeprom"))) = L_ZERO;


unsigned int ID0_EEP  __attribute__((section(".eeprom"))) = ID0;
unsigned int ID1_EEP  __attribute__((section(".eeprom"))) = ID1;

static unsigned int  * pMAX_EEP;
static unsigned int  * pMIN_EEP;
static unsigned char * pXSD_EEP;
static unsigned int  * pADJ_ZERO;

#endif
static unsigned char MEASURE_MODE;
static unsigned int  ADJ_ZERO;
static unsigned int  YW_H;

/* ********************************************************************** */
unsigned int READ_WORD_EEP(unsigned int * addr) {eeprom_busy_wait();return eeprom_read_word(addr);}
void WRITE_WORD_EEP(unsigned int x,unsigned int * addr) {eeprom_busy_wait();eeprom_write_word(addr,x);}
unsigned char READ_BYTE_EEP(unsigned char * addr) {eeprom_busy_wait();return eeprom_read_byte(addr);}
void WRITE_BYTE_EEP(unsigned char x,unsigned char * addr) {eeprom_busy_wait();eeprom_write_byte(addr,x);}
/* ********************************************************************** */

static LONG V_MAX;
static LONG V_MIN;
static FLOAT V_DATA;
static LONG ID;
/* ********************************************************************** */
volatile unsigned char RxMode = 0;
volatile unsigned char RxFrameComplete = 0;
volatile unsigned char WorkMode = 0;
volatile unsigned char RXDataTemp = 0;
volatile MODBUSFRAME RxFrame;
volatile unsigned char DeviceAddr;
unsigned char RxArrayTemp[RXARRAYTEMP_MAX];
volatile unsigned int  ADCResult = 0;
volatile unsigned int  ErrFlag;
volatile unsigned int  ErrCodeDisplay;
volatile unsigned int  VALUE_MAX;
volatile unsigned int  VALUE_MIN;
volatile unsigned char XiaoShuDian;
volatile unsigned int  T_C;
/* ********************************************************************** */



/* ********************************************************************** */
#define TIMERDELAY
#ifdef TIMERDELAY
/* ---------------------------------------------------------------------- */
#define TimerEnable()       do{TIMSK |= (1 << TOIE1);}while(0)
#define TimerDisable()      do{TIMSK &= ~(1 << TOIE1);}while(0)

#define TimerStop()         do{TCCR1B &= ~((1 << CS12)|(1 << CS11)|1 << CS10);}while(0)
#define TimerStart()        do{TCCR1B |= (1 << CS11);}while(0)

#define SetTimerCount(v)    do{\
                                TCNT1H = (unsigned char)((unsigned int)(v)>>8);\
                                TCNT1L = (unsigned char)((unsigned int)(v)& 0xFF);\
                            }while(0)
/* ---------------------------------------------------------------------- */
volatile unsigned int   TimerCycleI;//
volatile unsigned int   TimerCycleF;//
volatile unsigned char  TimerOver;//
/* ---------------------------------------------------------------------- */
void TimerDelayMs(unsigned int s)
{
    TimerCycleI = s >> 6;                   //TimerCycleN = s/64
    TimerCycleF = 65535 - (s & 0x3F)*1000;  //TimerCycleF = s%64
    TimerOver = 0;
    
    switch(TimerCycleI)
    {
    case 0:
        SetTimerCount(TimerCycleF);
        break;
    case -1:
        TimerDisable();
        TimerOver = 1;
        break;
    default:
        SetTimerCount(1535);
        //TCNT1H = 1535/256;
        //TCNT1L = 1535%256;
        break;
    }
TimerEnable();
TimerStart();
}
/* ---------------------------------------------------------------------- */
SIGNAL(SIG_OVERFLOW1)
{
    
    TimerCycleI--;
    switch(TimerCycleI)
    {
    case 0:
    	SetTimerCount(TimerCycleF);
    	break;
    case -1:
    	TimerStop();
    	TimerDisable();
    	TimerOver = 1;
    	return;
    default:
    	SetTimerCount(1535);
    	break;	
    }
}
#endif
/* ********************************************************************** */






/* ********************************************************************** */
void USART_Init(unsigned int baud)
{
    /* set baud rate */
    UBRRH = (F_CPU/baud/16-1)/256;
    UBRRL = (F_CPU/baud/16-1)%256;
    /* Enable Receiver and Transmitter Enable Receiver interrupt */
    UCSRB =(1<<RXCIE)|(1<<RXEN)|(1<<TXEN);
    /* Set frame format: 8data,2stop bit */
    UCSRC = (1<<URSEL)|
        (0<<UMSEL)|    /* 0:Asynchronous 1:Synchronous */
        (0<<UPM0)|    /* 0:Disable 1:Reserved 2:Even Parity 3:Odd Parity */
        (0<<USBS)|    /* Stop Bit 0:1-bit 1:2-bit */
        (3<<UCSZ0);    /* Data bit 0 ~ 7 : 5,6,7,8,-,-,-,9 */
}
void USART_Transmit(unsigned char data)
{
    while(!(UCSRA & (1<<UDRE)));
    UDR = data;
}

#define putchar USART_Transmit
void printk(char *s)
{
    if(s == NULL || *s == '\0')
        return;
    TX485();
    while(*s != '\0'){
        putchar(*s++);
    }
    RX485();
}
/* ********************************************************************** */
void RxOverrunTimer(void)
{
    /* enabile Timer0 */
    TCNT2 = FrameInterval;
    TIFR |= (1<<TOV2);
    TIMSK |= (1<<TOIE2);
    RxFrameComplete = 0;
}
SIGNAL(SIG_OVERFLOW2)
{
    TIMSK &= ~(1<<TOIE2);
    TIFR |= (1<<TOV2);
#if 1
    switch(RxMode)
    {
        case 1: /* receive Addr only */
            //ErrorDisplay(ERRCODE_OnlyReceiveAddr);
            break;
        case 2: /* receive Addr and FunCode only */
            //ErrorDisplay(ERRCODE_ReceiveDataErr);
            break;
        case 3: /* receive All */
            if(RxFrame.Len < 2){
                //ErrorDisplay(ERRCODE_ReceiveDataErr);
                RxFrame.Data = RxArrayTemp + 2;
                break;
            }
            RxFrameComplete = 1;
            RxFrame.CRCL = *(RxFrame.Data - 2);
            RxFrame.CRCH = *(RxFrame.Data - 1);
            RxFrame.Data = RxArrayTemp + 2;
            break;
        default :
		    break;
            //ErrorDisplay(ERRCODE_ReceiveDataErr);
    }
    /* Stop Timer2 */
    TIMSK &= ~(1<<TOIE2);
    TCNT2 = 0x00;
    RxMode = 0;
#else
    PORTD ^= (1<<6);
    RxOverrunTimer();
#endif
}
/* ********************************************************************** */
SIGNAL(SIG_UART_RECV)
{
    if(! (UCSRA & (1<<RXC))){
        return;
    }
    cli();
    TIMSK &= ~(1<<TOIE2);
    //if(UCSRA & ((1<<FE)|(1<<DOR)|(1<<PE))){
        //ErrorDisplay(ERRCODE_ReceiveBitErr);
    //}
    RXDataTemp = UDR;
#if 0
    TX485();
    putchar(RXDataTemp);
    RX485();
#endif
    switch(RxMode)
    {
        case 0x00:
            RxFrame.Addr = RXDataTemp;
            RxArrayTemp[0] = RXDataTemp;
            RxMode = 0x01;
            break;
        case 0x01:
            RxFrame.FunCode = RXDataTemp;
            RxArrayTemp[1] = RXDataTemp;
            RxFrame.Data = RxArrayTemp + 2;
            RxFrame.Len = 0;
            RxMode = 0x02;
            break;
        case 0x02:
        case 0x03:
            *(RxFrame.Data++) = RXDataTemp;
            RxFrame.Len++;
            if(RxFrame.Len == (RXARRAYTEMP_MAX-2)){
                RxFrame.Data = RxArrayTemp + 2;
                RxFrame.Len = 0;
                //ErrorDisplay(ERRCODE_RXARRAYTEMP_MAXErr);
            }
            RxMode = 0x03;
            break;
    }
    RxOverrunTimer();
    sei();
}
/* ********************************************************************** */
void ADCInit(void)
{
#ifdef ADC_ADS1110
     Init_ADS1110();
#else
    ADMUX =  (0<<REFS0)|    /* 0:Aref,1:AVCC,2:-,3:2.56V */
             (1<<ADLAR)|    /* 0:right adjust,1:left adjust */
             (7<<MUX0);     /* 0~7 channel */
    ADCSRA = (1<<ADEN)|     /* ADC enable */
             (0<<ADSC)|     /* ADC start */
             (0<<ADFR)|     /* ADC Free Running Select */
             (0<<ADIF)|     /* ADC Interrupt Flag ,1:clear */
             (0<<ADIE)|     /* ADC Interrupt Enable */
             (0<<ADPS1);    /* ADC Prescaler Select Bits */
#endif
}
#ifdef ADC_ADS1110
#define ADCOutOne ReadADS1110
#else
unsigned int ADCOutOne(void)
{
    unsigned int i;
    unsigned long sum = 0;

#ifdef ADC_ADS1110
    #define ADC_COUNT 2
    for(i = 0;i < ADC_COUNT; i++){
        sum += ReadADS1110();
    }
    return ((unsigned int)(sum>>1));
#else
    #define ADC_COUNT 64
    unsigned int ret = 0;
    unsigned char tmpH,tmpL;

    for(i = 0;i < ADC_COUNT; i++)
    {
        ADCSRA |= (1<<ADSC);  //启动转换
        while(ADCSRA & (1<<ADSC));
        tmpL = ADCL;
        tmpH = ADCH;
        ret =(unsigned int)(tmpH << 2);
        ret = ret + (tmpL>>6 & 0x3);
    sum = sum + ret;
    }

    return sum>>6;
#endif
}
#endif
#define ADC_Arrya_Len_MAX   (64)
unsigned int ADC_Arrya_Num;
unsigned int ADC_Arrya[ADC_Arrya_Len_MAX];
unsigned int ADCOut(void)
{
    unsigned int temp;
    unsigned int tempMAX = 0,tempMIN = 0;
    unsigned long SUM = 0;
    unsigned char i;

    tempMAX = ADCOutOne();
    tempMIN = ADCOutOne();

    for(i = 0;i < 34;i++)
    {
        temp = ADCOutOne();
    if(temp < tempMIN)
        tempMIN = temp;
        if(temp > tempMAX)
        tempMAX = temp;
    SUM = SUM + temp;
    }
    SUM = SUM - tempMAX - tempMIN;
    temp = (unsigned int)(SUM>>5);

    ADC_Arrya_Num ++;
    if(ADC_Arrya_Num >= ADC_Arrya_Len_MAX){
        ADC_Arrya_Num = 0;
    }
    ADC_Arrya[ADC_Arrya_Num] = temp;

    SUM = 0;
    for(i = 0;i < ADC_Arrya_Len_MAX;i++){
        SUM += ADC_Arrya[i];
    }
    temp = SUM / (long)ADC_Arrya_Len_MAX;

    return  temp;
}
#ifdef LL_DEBUG
float  QC_TS;
#endif
#if 1
float  QC_K_h = 0.00085;
float  QC_C_e = 0.577;
float  QC_C_ = 2.361 * 0.577;
#else
#define  QC_K_h  (0.00085)
#define  QC_C_e  (0.577)
//#define  QC_C_   (2.361 * 0.577)
#define  QC_C_   (1.362)
#endif
float ChangeADCResult_LL(void)
{
    float LL_Result;
    float temp;
    float tmp;
    long adc = 0;
#if 0
    LL_Result =  (float)(ADCOut());
    LL_Result += (float)ADJ_ZERO;
	
    temp = (LL_Result * (float)(VALUE_MAX - VALUE_MIN)) /(float)ADC_JINGDU + (float)VALUE_MIN ;
#endif
    ADCResult = ADCOut();
    ADCResult = ADCResult + ADJ_ZERO;
    adc = ((long) ADCResult * (long)(VALUE_MAX - VALUE_MIN)) /(long)ADC_JINGDU + VALUE_MIN ;
    ADCResult = (unsigned int) adc;
    
    temp = (float)ADCResult;
    
    temp /= 1000.0;
#ifdef LL_DEBUG
    QC_TS = temp;
#endif
    temp = temp + QC_K_h;
    tmp = powf(temp,5);
    temp = sqrtf(tmp);

    LL_Result = (QC_C_ * temp);
    
    LL_Result *= 1000;
    return LL_Result;
}

float LL_value;
void RefreshLED_LL(void)
{
    float T_C_temp;
    unsigned int tmp;

    T_C_temp = ChangeADCResult_LL();
    if(LL_value > T_C_temp){
        if( (LL_value - T_C_temp) > 3 )
    {
            LL_value = T_C_temp;
            tmp = (unsigned int)ceil(LL_value);
            Display10(tmp);
    }
    } else {
        if( (T_C_temp - LL_value) > 3 )
    {
            LL_value = T_C_temp;
            tmp = (unsigned int)ceil(LL_value);
            Display10(tmp);
    }
    }
#ifdef TIMERDELAY
    if(TimerOver != 0)
    {
        /* change T_C value 1S */
        tmp = (unsigned int)ceil(LL_value);
        Display10(tmp);
        LL_value = ChangeADCResult_LL();
        /*  */
        TimerOver = 0;
        TimerDelayMs(1000);
        /* D2 LED 1S flicker */
        PORTD ^= (1<<7);
    if(ErrFlag !=0)
            ErrFlag ++;
    }
#endif
}

unsigned int ChangeADCResult(void)
{
    long temp;
    //double tmp;

    ADCResult = ADCOut();
    ADCResult += ADJ_ZERO;
	
    temp = ((long) ADCResult * (long)(VALUE_MAX - VALUE_MIN)) /(long)ADC_JINGDU + VALUE_MIN ;
    ADCResult = (unsigned int) temp;
    
	
	if(MEASURE_MODE == ONE_MODE_YW)
	
	ADCResult = YW_H - ADCResult;
	
    return ADCResult;
}
void RefreshLED(void)
{
    unsigned int T_C_temp;
    T_C_temp = ChangeADCResult();
    if(T_C > T_C_temp){
        if( (T_C - T_C_temp) > 3 )
    {
            T_C = T_C_temp;
            Display10(T_C_temp);
    }
    } else {
        if( (T_C_temp - T_C) > 3 )
    {
            T_C = T_C_temp;
            Display10(T_C_temp);
    }
    }
#ifdef TIMERDELAY
    if(TimerOver != 0)
    {
        /* change T_C value 1S */
        Display10(T_C);
        T_C = ChangeADCResult();
        /*  */
        TimerOver = 0;
        TimerDelayMs(1000);
        /* D2 LED 1S flicker */
        PORTD ^= (1<<7);
    if(ErrFlag !=0)
            ErrFlag ++;
    }
#endif
}
void SendDataToPC(FUNCODE Fcode, unsigned char * SendTemp, unsigned int Len)
{

    unsigned char ModbusTemp[RXARRAYTEMP_MAX];
    unsigned short RxCRC16;
    unsigned char i;
    unsigned char count;
    ModbusTemp[0] = DeviceAddr;
    ModbusTemp[1] = Fcode;

    if(Fcode != SET_DEVICEADDR){
        count = 3;
        ModbusTemp[2] = (unsigned char)(Len & 0xFF);
    }else{
        count = 2;
    }
    for(i = 0;i < Len;i++){
        ModbusTemp[i+count] = SendTemp[i];
    }
    RxCRC16 = CRC16(ModbusTemp, Len + count);

    TX485();

    for(i = 0;i < Len + count;i++){
        putchar(ModbusTemp[i]);
    }
    
    putchar((unsigned char) (RxCRC16 & 0xFF));
    putchar((unsigned char) (RxCRC16 >> 8));
    
    _delay_ms(10);
    RX485();

}
unsigned char FrameCRC16IsOK(MODBUSFRAME Temp)
{
    unsigned char i;
    unsigned int CRCtemp;
    unsigned char sch[RXARRAYTEMP_MAX];
    sch[0] = Temp.Addr;
    sch[1] = Temp.FunCode;
    for(i=0;i<(Temp.Len-2);i++,Temp.Data++)
        sch[i+2] = *(Temp.Data);
    CRCtemp = CRC16(sch, Temp.Len);

    if(Temp.CRCL != (unsigned char)CRCtemp)
        return 0;
    if(Temp.CRCH != (unsigned char)(CRCtemp>>8))
        return 0;
    return 1;
}
#if 0
void ErrorDisplay(unsigned int ErrCode)
{
    ErrFlag = 1;
    ErrCode &= 0x0FFF;
    ErrCodeDisplay = ErrCode | 0xE000;

}
#endif
void IsErrorOrNot(void)
{
    if(ErrFlag)
    {
        if(ErrFlag > 2);
            ErrFlag = 0;
        DisplayHex(ErrCodeDisplay);
    }
}
#if 1
void LoadEEPROM(void)
{
#ifndef ONE_MODE
    /*
    MEASURE_MODE = MEASURE_MODE_NUMBER;
    DeviceAddr  = def_DeviceAddr;
    VALUE_MAX   = DEF_VALUE_MAX(I_RangeMAX,I_RangeMIN);
    VALUE_MIN   = DEF_VALUE_MIN(I_RangeMAX,I_RangeMIN);
    XiaoShuDian = I_DEF_XiaoShuDian;
    ADJ_ZERO    = 0;
    */
    MEASURE_MODE = READ_BYTE_EEP(&MEASURE_MODE_EEP);
    MEASURE_MODE ++;
    if(MEASURE_MODE>3)
        MEASURE_MODE = 0;

    switch(MEASURE_MODE){
    case 0:
        pMAX_EEP = &I_MAX_EEP;
        pMIN_EEP = &I_MIN_EEP;
        pXSD_EEP = &I_XSD_EEP;
        pADJ_ZERO = &I_ZERO_EEP;
        break;
    case 1:
        pMAX_EEP = &T_MAX_EEP;
        pMIN_EEP = &T_MIN_EEP;
        pXSD_EEP = &T_XSD_EEP;
        pADJ_ZERO = &T_ZERO_EEP;
        break;
    case 2:
        pMAX_EEP = &Y_MAX_EEP;
        pMIN_EEP = &Y_MIN_EEP;
        pXSD_EEP = &Y_XSD_EEP;
        pADJ_ZERO = &Y_ZERO_EEP;
        break;
    case 3:
        pMAX_EEP = &L_MAX_EEP;
        pMIN_EEP = &L_MIN_EEP;
        pXSD_EEP = &L_XSD_EEP;
        pADJ_ZERO = &L_ZERO_EEP;
        break;
    default:
        MEASURE_MODE = 0;
        break;
    }
    WRITE_BYTE_EEP(MEASURE_MODE,&MEASURE_MODE_EEP);
#else
    MEASURE_MODE = READ_BYTE_EEP(&MEASURE_MODE_EEP);
    pMAX_EEP = &I_MAX_EEP;
    pMIN_EEP = &I_MIN_EEP;
    pXSD_EEP = &I_XSD_EEP;
    pADJ_ZERO = &I_ZERO_EEP;
	
#endif
    DeviceAddr   = READ_BYTE_EEP(&ADDR_EEP);

    VALUE_MAX   = READ_WORD_EEP(pMAX_EEP);
    VALUE_MIN   = READ_WORD_EEP(pMIN_EEP);
    if(MEASURE_MODE == ONE_MODE_LL)
        XiaoShuDian = 0;
    else
        XiaoShuDian = READ_BYTE_EEP(pXSD_EEP);
    
    ADJ_ZERO    = READ_WORD_EEP(pADJ_ZERO);
	YW_H = READ_WORD_EEP(&Y_H_EEP);
}
#endif
/* ********************************************************************** */
void SystemInit(void)
{

    
//IO Direction
    DDRC = 0x0F;
    DDRB = 0x07;
    DDRD = 0xFE;
//
    LoadEEPROM();
//enable interrupt 
    sei();
//Display Open
    OpenDisplayLED();
//485 enabile
    RX485();
//Open Uart
    USART_Init(UartBaud);
//Set Timer2 clk
    /* clock select clk/128 */
    TCCR2 = Timer2ClkSel;
//Enable ADC
    ADCInit();
//Enable watchdog
    wdt_enable(WDTO_1S);
//
#ifdef TIMERDELAY
    TimerOver = 0;
    TimerDelayMs(1000);
#endif

}

int main(void)
{
    unsigned char SendTemp[10];
    unsigned int tempint;
    long tempA = 0;
    long tempB = 0;
    unsigned char CRCYesOrNo;
    unsigned char tempch;
    unsigned char ReturnID = 1;
    //unsigned int i = 0;
    /* 0:wait receive frame 1:receive frame finish */
    RxFrameComplete = 0;
    /* Init Receive status mode*/
    RxMode = 0;
    /* Error Flag clear */
    ErrFlag = 0;
    /* System init */
    SystemInit();
    //RxOverrunTimer();
    ID.L = 0;
    ID.L = READ_WORD_EEP(&ID0_EEP);
    ID.L <<=16;
    ID.L += READ_WORD_EEP(&ID1_EEP);
#ifndef MYDEBUG
    ErrCodeDisplay = MEASURE_MODE + 0xD000;
    DisplayHex(ErrCodeDisplay);
    for(tempint = 0;tempint < 10;tempint ++)
    {
        _delay_ms(200);
        wdt_reset();
    }
#endif
    while(1)
    {
        if(!RxFrameComplete)
        {
            //if(ErrFlag == 0)
            if(MEASURE_MODE == ONE_MODE_LL)
                RefreshLED_LL();
            else
                RefreshLED();
            IsErrorOrNot();
            wdt_reset();
            continue;
        }

        /* clear receive finish flag */
        RxFrameComplete = 0;
        /* Judge Addr is OK */
        if(RxFrame.Addr != DeviceAddr)
        {
            if(RxFrame.Addr != 0xFE){
                continue;
            }
            if((RxFrame.FunCode == 0x88)&&(ReturnID)){
                if(RxFrame.Data[0] == 'B' &&
                   RxFrame.Data[1] == 'o' &&
                   RxFrame.Data[2] == 's' &&
                   RxFrame.Data[3] == 's' &&
                   RxFrame.Data[4] == 'u' &&
                   RxFrame.Data[5] == 'N'){
                    SendTemp[0] = ID.ch[3];
                    SendTemp[1] = ID.ch[2];
                    SendTemp[2] = ID.ch[1];
                    SendTemp[3] = ID.ch[0];
                    SendDataToPC(0x78, SendTemp , 4);
                }
            continue;
            }
#if 0
            if(RxFrame.FunCode == 0x89){
                if(RxFrame.Data[0] == ID.ch[3] &&
                   RxFrame.Data[1] == ID.ch[2] &&
                   RxFrame.Data[2] == ID.ch[1] &&
                   RxFrame.Data[3] == ID.ch[0] ){
                    
                    SendTemp[0] = DeviceAddr;
                    SendDataToPC(0x79, SendTemp , 1);
                    DeviceAddr = RxFrame.Data[4];
#ifndef MYDEBUG
                    WRITE_BYTE_EEP(DeviceAddr,&ADDR_EEP);
#endif
                }
            continue;
            }
#endif
        }
        /* Judge CRC16 is OK */
        if(!FrameCRC16IsOK(RxFrame))
        {
            //ErrorDisplay(ERRCODE_CRC16Error);
             CRCYesOrNo = 0x80;
        }else{
             CRCYesOrNo = 0;
        }
        tempint = (unsigned int)RxFrame.Data[0];
        tempint <<= 8;
        tempint |= (unsigned int)RxFrame.Data[1];
        switch(RxFrame.FunCode){
            case CommunicationTest:
                SendDataToPC(CommunicationTest, RxFrame.Data, RxFrame.Len - 2);
                break;
            case GET_VALUE:
                switch(tempint){
                    case ADDR_VALUE:/* Get ADC data */
                        if(MEASURE_MODE == ONE_MODE_LL)
                            V_DATA.F = LL_value;
                        else{
                            V_DATA.F = (float)T_C;
                        switch(XiaoShuDian){
                            case 2:V_DATA.F /= 10;
                                   break;
                            case 3:V_DATA.F /= 100;
                                   break;
                            case 4:V_DATA.F /= 1000;
                                   break;
                        }
                        }
                        SendTemp[0] = V_DATA.ch[3];
                        SendTemp[1] = V_DATA.ch[2];
                        SendTemp[2] = V_DATA.ch[1];
                        SendTemp[3] = V_DATA.ch[0];
                        SendDataToPC(GET_VALUE | CRCYesOrNo, SendTemp , 4);
#ifdef LL_DEBUG
                        if(MEASURE_MODE == ONE_MODE_LL){
                            V_DATA.F = QC_TS;
                            SendTemp[0] = V_DATA.ch[3];
                            SendTemp[1] = V_DATA.ch[2];
                            SendTemp[2] = V_DATA.ch[1];
                            SendTemp[3] = V_DATA.ch[0];
                            SendDataToPC(GET_VALUE | CRCYesOrNo, SendTemp , 4);
                        }
#endif
                        break;
                    default:
                        SendTemp[0] = 0x00;
                        SendDataToPC(GET_VALUE | 0x80, SendTemp , 1);
                        break;
                }
                break;
            case GET_DATA:
                switch(tempint)
                {
                    case ADDR_XSD:/* Get XiaoShuDian Data */
                        SendTemp[0] = 0;
                        SendTemp[1] = XiaoShuDian;
                        SendDataToPC(GET_DATA | CRCYesOrNo, SendTemp , 2);
                        break;
                    case ADDR_ZERO:/* Get XiaoShuDian Data */
                        SendTemp[0] = 0;
                        SendTemp[1] = ADJ_ZERO;
                        SendDataToPC(GET_DATA | CRCYesOrNo, SendTemp , 2);
                        break;
#if 0
                    case ADDR_F1_VALUE:/* Get Freq Data */
                        SendTemp[0] = 0x00;
                        SendTemp[1] = 0x00;
                        SendDataToPC(GET_DATA | CRCYesOrNo, SendTemp , 2);
                        break;
                    case ADDR_F2_VALUE:/* Get Freq Data */
                        SendTemp[0] = 0x00;
                        SendTemp[1] = 0x00;
                        SendDataToPC(GET_DATA | CRCYesOrNo, SendTemp , 2);
                        break;
#endif
                    default:
                        SendTemp[0] = 0x00;
                        CRCYesOrNo = 0x80;
                        SendDataToPC(GET_DATA | CRCYesOrNo, SendTemp , 1);
                        break;
                }
                break;
            case SET_DEVICEADDR:
                switch(tempint)
                {
                    case ADDR_DEV:/* change device addr */
                        if(RxFrame.Data[3] == 0x00){
                            break;
                        }
                        tempch = RxFrame.Data[3];
                        //SendTemp[0] = DeviceAddr;
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = 0x00;
                        SendTemp[3] = tempch;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
                        DeviceAddr = tempch;
#ifndef MYDEBUG
                        WRITE_BYTE_EEP(DeviceAddr,&ADDR_EEP);
#endif
                        break;
                    case ADDR_XSD:
                        XiaoShuDian =  RxFrame.Data[3];
                        //SendTemp[0] = XiaoShuDian;
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = 0x00;
                        SendTemp[3] = XiaoShuDian;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
#ifndef MYDEBUG
                        WRITE_BYTE_EEP(XiaoShuDian,pXSD_EEP);
#endif
                        break;
                    case ADDR_ZERO:
                        ((char *)&ADJ_ZERO)[1] =  RxFrame.Data[2];
                        ((char *)&ADJ_ZERO)[0] =  RxFrame.Data[3];
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = (char)(ADJ_ZERO>>8);
                        SendTemp[3] = (char)ADJ_ZERO;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
#ifndef MYDEBUG
                        WRITE_WORD_EEP(ADJ_ZERO,pADJ_ZERO);
#endif
                        break;
                    case ADDR_YW_H:/* change device addr */
                        if(RxFrame.Data[3] == 0x00){
                            break;
                        }
                        YW_H = RxFrame.Data[3];
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        //SendTemp[2] = 0x00;
                        //SendTemp[3] = YW_H;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
#ifndef MYDEBUG
                        WRITE_WORD_EEP(DeviceAddr,&Y_H_EEP);
#endif
                        break;
                    case ADDR_FID:/* change device addr */

                        MEASURE_MODE = RxFrame.Data[3];
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = 0x00;
                        SendTemp[3] = MEASURE_MODE;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
#ifndef MYDEBUG
                        WRITE_BYTE_EEP(MEASURE_MODE,&MEASURE_MODE_EEP);
#endif
                        while(1);
                        break;
                    case ADDR_RETURN_ID:
                        if(RxFrame.Data[3] != 0){
                            ReturnID = 1;
                        }else{
                            ReturnID = 0;
                        }
                        //SendTemp[0] = XiaoShuDian;
                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = 0x00;
                        SendTemp[3] = ReturnID;
                        SendDataToPC(SET_DEVICEADDR | CRCYesOrNo, SendTemp , 4);
                        break;
                    default:
                        SendTemp[0] = 0x00;
                        SendDataToPC(SET_DEVICEADDR | 0x80, SendTemp , 1);
                        break;
                }
                break;
            case SET_VALUE:
                switch(tempint){
                    case ADDR_FW_MAX:

                        V_MAX.ch[3] = RxFrame.Data[5];
                        V_MAX.ch[2] = RxFrame.Data[6];
                        V_MAX.ch[1] = RxFrame.Data[7];
                        V_MAX.ch[0] = RxFrame.Data[8];

                        V_MIN.ch[3] = RxFrame.Data[9];
                        V_MIN.ch[2] = RxFrame.Data[10];
                        V_MIN.ch[1] = RxFrame.Data[11];
                        V_MIN.ch[0] = RxFrame.Data[12];

                        tempA =(unsigned int) V_MAX.L;
                        tempB =(unsigned int) V_MIN.L;

                        VALUE_MAX = Out_MAX(tempA,tempB);
                        VALUE_MIN = Out_MIN(tempA,tempB);

                        SendTemp[0] = RxFrame.Data[0];
                        SendTemp[1] = RxFrame.Data[1];
                        SendTemp[2] = 0x08;

                        SendDataToPC(SET_VALUE | CRCYesOrNo, SendTemp , 3);
#ifndef MYDEBUG
                        WRITE_WORD_EEP(VALUE_MAX,pMAX_EEP);
                        WRITE_WORD_EEP(VALUE_MIN,pMIN_EEP);
                        WRITE_WORD_EEP(VALUE_MAX,pMAX_EEP);
                        WRITE_WORD_EEP(VALUE_MIN,pMIN_EEP);
#endif
                }
                break;
            case GET_ID:
                SendTemp[0] = 0x00;
                switch(tempint)
                {
                    case ADDR_CID:
                        SendTemp[1] = CODE_CID;
                        break;
                    case ADDR_PID:
                        SendTemp[1] = CODE_PID;
                        break;
                    case ADDR_VID:
                        SendTemp[1] = CODE_VID;
                        break;
                    case ADDR_FID:
                        SendTemp[1] = MEASURE_MODE;
                        break;
                    default:
                        SendTemp[0] = 0x00;
                        CRCYesOrNo = 0x80;
                        break;
                }
                SendDataToPC(GET_ID | CRCYesOrNo, SendTemp , 2);
                break;
#if 0
            case GET_CHAR:
                switch(tempint){
                    case SEL_CHAR_COMPANY:
                        printk(CHAR_COMPANY);
                        break;
                    case SEL_CHAR_DESCRIBE:
                        printk(CHAR_DESCRIBE);
                        break;
                    default:
                        SendTemp[0] = 0x00;
                        SendDataToPC(GET_CHAR | 0x80, SendTemp , 1);
                        break;
                }
                break;
#endif
            default:
                RxFrame.FunCode |= 0x80;
                SendDataToPC(RxFrame.FunCode, RxFrame.Data, RxFrame.Len - 2);
                //ErrorDisplay(ERRCODE_FunCodeInvalid);
                //PORTD ^= (1<<6);
                break;
        }//switch end
    }//while end
    //return 0;
}//main end





