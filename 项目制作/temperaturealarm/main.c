#include <msp430.h> 
#include<math.h>
#include "oled.h"
#include "bmp.h"

unsigned int duty=0;    //PWM duty cycle(%)
unsigned int iflag=0;    //over temperature's signal
char tem[8]="TEMP:";
unsigned int speed=0;
unsigned int mode=1;

void ADinit()
{
    ADC12CTL0|=ADC12MSC;
    ADC12CTL0|=ADC12ON;
    ADC12CTL1|=ADC12CONSEQ_3;
    ADC12CTL1|=ADC12SHP;
    ADC12CTL1|=ADC12CSTARTADD_0;
    ADC12MCTL0|=ADC12INCH_1;
    ADC12MCTL1|=ADC12INCH_2+ADC12EOS;
    ADC12CTL0|=ADC12ENC;
}
void ClockInit()
{
    //MCLK:16MHZ,SMCLK:8MHZ,ACLK:32KHZ
    UCSCTL6&=~XT1OFF;       //open XT1
    P5SEL|=BIT2+BIT3;       //XT2 choose IO's functions
    UCSCTL6&=~XT2OFF;       //open XT2
    __bis_SR_register(SCG0);
    UCSCTL0=DCO0+DCO1+DCO2+DCO3+DCO4;
    UCSCTL1=DCORSEL_4;      //DCO<=28.2MHz
    UCSCTL2=FLLD_5+1;       //D=16,N=1

    //n=8,FLLREFCLK's clk:XT2CLK
    //DCOCLK=D*(N+1)*(FLLREFCLK/n)
    //DCOCLKDIV=(N+1)*(FLLREFCLK/n)
    UCSCTL3=SELREF_5+FLLREFDIV_3;

    //ACLK's clk:DCOCLKDIV
    //MCLK\SMCLK's clk:DCOCLK
    UCSCTL4=SELA_4+SELS_3+SELM_3;

    //ACLK obtained by dividing DCOCLKDIV by 32
    //SMCLK obtained by dividing DCOCLK by 2
    UCSCTL5=DIVA_5+DIVS_1;
}

void IO_Init(void)
{
    //keys setting
    P2DIR&=~BIT3;
    P2REN|=BIT3;
    P2OUT|=BIT3;

    P1DIR&=~BIT1;
    P1REN|=BIT1;
    P1OUT|=BIT1;
    P1IE|=BIT1;
    P1IES|=BIT1;
    P1IFG&=~BIT1;

    //H-Bridge setting
    P1DIR|=BIT5;
    P2DIR|=BIT4+BIT5;
    P2SEL|=BIT4;

    //buzzer setting
    P3DIR|=BIT6;
    P3OUT&=~BIT6;

    //LED setting
    P8DIR|=BIT1;
    P8OUT&=~BIT1;

}

void Timer_Init(void)
{
    TA2CTL=MC_1+TASSEL_2+TACLR; //up mode,SMCLK(8MHz),clear TAR
    TA2CCR0=512;                //PWM Period
    TA2CCTL1=OUTMOD_7;          //CCR1 toggle/set
    TA2CCR1=0;                  // CCR1 PWM duty cycle(%)
}

void OLEDInit(void)
{
    OLED_Init();        //initialize OLED
    OLED_Clear();
    OLED_ShowString(30,0,"ALARM: OFF ");
    OLED_ShowString(0,2,tem);
    OLED_ShowString(0,4,"MODE:Manual");//OLED_ShowString(8,4,"FANS MODE: Auto");
    OLED_ShowString(20,6,"FANS SPEED:");
}

int GetAD1(void)
{
    int a=0;
    ADC12CTL0|=ADC12SC;
    a=ADC12MEM0;
    return a;
}

int GetAD2(void)
{
    int a=0;
    ADC12CTL0|=ADC12SC;
    a=ADC12MEM1;
    return a;
}

void ADC(void)
{
    int v1=GetAD1();
    int v2=GetAD2();
    if(v1>1500)
        iflag=1;
    else
        iflag=0;
    float T=0;
    T=0.0262*v2-31.853;
    tem[5]=((int)T)/10+48;
    tem[6]=((int)T)%10+48;
    if(T>=36&&T<40&&mode==1)
    {
        duty=300;
        speed=1;
    }
    if(T>=38&&T<44&&mode==1)
    {
        duty=350;
        speed=2;
    }
    if(T>=40&&mode==1)
    {
        duty=400;
        speed=3;
    }
}

void HBridge(void)
{
    if(iflag) TA2CCR1=duty;
    else TA2CCR1=0;
}

void BuzzerLED(void)
{
    if(iflag)
    {
        P3OUT^=BIT6;
        P8OUT^=BIT1;
        __delay_cycles(8000);
    }
}

void OLEDScreen(void)
{
    if(iflag)
        OLED_ShowString(30,0,"ALARM: ON  ");
    else
        OLED_ShowString(30,0,"ALARM: OFF ");
    OLED_ShowString(0,2,tem);
    if(mode)
        OLED_ShowString(0,4,"MODE: Auto  ");
    else
        OLED_ShowString(0,4,"MODE: Manual");
    if(speed==1)
        OLED_ShowString(20,6,"FANS SPEED: 1");
    else if(speed==2)
        OLED_ShowString(20,6,"FANS SPEED: 2");
    else if(speed==3)
        OLED_ShowString(20,6,"FANS SPEED: 3");
}

void judgement(void)
{
    if(!(P2IN&BIT3)&&mode==1)
    {
        mode=0;
        __delay_cycles(10000000);
    }
    else if(!(P2IN&BIT3)&&mode==0)
    {
        mode=1;
        __delay_cycles(10000000);
    }
}

/**
 * main.c
 */

int main(void)
{
    WDTCTL=WDTPW|WDTHOLD;   //Stop watchdog timer

    ClockInit();
    IO_Init();
    ADinit();
    Timer_Init();
    OLEDInit();

    __enable_interrupt();

    while(1)
    {
        judgement();
        ADC();
        HBridge();
        BuzzerLED();
        OLEDScreen();
    }
}

#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void)
{
    if(P1IFG&BIT1&&speed<3&&mode==0)
    {
        duty+=50;
        speed+=1;
        P1IFG&=~BIT1;
        __delay_cycles(10000000);
    }
    else if(P1IFG&BIT1&&speed==3&&mode==0)
    {
        duty=300;
        speed=1;
        P1IFG&=~BIT1;
        __delay_cycles(10000000);
    }
}

