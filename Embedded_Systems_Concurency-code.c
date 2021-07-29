//Embedded systems code to simulate the operations of a motor with manual control features. Concurrency was the main focus.

#include "mkl25z4.h"
#define MASK(x)	(1UL << x)

//defining state of switches
int sw1_state=0;
int sw2_state=0;
int sw3_state=0;

//initializing facilitator methods
static void check_inputs(void);
static void initialize(void);
static void delay_ms(int n);
static void forward_or_back(void);
static void stop_state(void);
static void flashB(void);
static void flashW(void);
static void fast_forward_flash(void);
static void fast_back_flash(void);
static void forward_ext_flash(void);
static void back_ext_flash(void);

//initializing my versions of millis for different purposes
static void bmillis(void);
static void wmillis(void);
static void fast_flash_millis(void);
static void second_counter_millis(void);
static void forward_red_counter_millis(void);
static void back_yellow_counter_millis(void);
int rotate_fast_millis(int n);
int rotate_slow_millis(int n);

//creating variables to keep count for the millis methods
volatile uint32_t bcounter=0;
volatile uint32_t wcounter=0;
volatile uint32_t rotate_fast_counter =0;
volatile uint32_t rotate_slow_counter =0;
volatile uint32_t fast_flash_counter = 0;
volatile uint32_t second_counter=0;
volatile uint32_t forward_red_counter=0;
volatile uint32_t back_yellow_counter=0;


int main()
{
	//defining pins to map with ports
	#define RED (18) //PTB 18
	#define GREEN (19) //PTB 19
	#define BLUE (1) //PTD 1
	#define yled (13) //PTA 13
	#define rled (5) //PTD 5
	#define c (4) //PTD 4
	#define d (12) //PTA 12
	#define e (4) //PTA 4
	#define g (5) //PTA 5
	#define sw1 (3) //PTC 3
	#define sw2 (4) //PTC 4
	#define sw3 (5) //PTC 5

	//configuring SysTick
	SysTick->LOAD = (20971520u/1000u)-1 ;  //configure for every half sec restart.
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk |SysTick_CTRL_TICKINT_Msk;
		
	initialize();
	while (1)  //forever
	{ 
		check_inputs();       //check all key presses
		forward_or_back();    //rotates r shape forward or backward depending and switch state
		stop_state();         //flash pattern for external led in the stop state
		flashB();             //flash pattern blue onboard led when moving back
		flashW();             //flash pattern for white onboard led when forward
		fast_forward_flash(); //flash pattern for onboard led when moving fast and forward
		fast_back_flash();    //flash pattern for onboard led when moving fast and backward
		forward_ext_flash();  //flash pattern for ext led when moving forward
		back_ext_flash();     //flash pattern for ext led when moving backward
	}
}

/*Initializes ports and pins to be used on board*/
static void initialize(){
	//clock gating to port A, B, C & D
	SIM->SCGC5 |=SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK; ////gate ports A, B, C & D

	//set up pins as GPIO
	PORTA->PCR[yled] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTA->PCR[yled] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTA->PCR[d] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTA->PCR[d] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTA->PCR[e] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTA->PCR[e] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTA->PCR[g] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTA->PCR[g] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTB->PCR[RED] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTB->PCR[RED] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTB->PCR[GREEN] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTB->PCR[GREEN] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTD->PCR[BLUE] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTD->PCR[BLUE] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTD->PCR[rled] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTD->PCR[rled] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTD->PCR[c] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTD->PCR[c] |= PORT_PCR_MUX(1);	//setup to be GPIO

	//configure input
	PORTC->PCR[sw1] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTC->PCR[sw1] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTC->PCR[sw2] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTC->PCR[sw2] |= PORT_PCR_MUX(1);	//setup to be GPIO
	PORTC->PCR[sw3] &= ~PORT_PCR_MUX_MASK;	//Clear mux
	PORTC->PCR[sw3] |= PORT_PCR_MUX(1);	//setup to be GPIO


	//set up outputs
	PTA->PDDR |= MASK(yled);
	PTA->PDDR |= MASK(d);
	PTA->PDDR |= MASK(e);
	PTA->PDDR |= MASK(g);
	PTB->PDDR |= MASK(RED);
	PTB->PDDR |= MASK(GREEN);
	PTD->PDDR |= MASK(BLUE);
	PTD->PDDR |= MASK(rled);
	PTD->PDDR |= MASK(c);
	
	//set up inputs
	PTC->PDDR &= ~MASK(sw1);		
	PTC->PDDR &= ~MASK(sw2);	
	PTC->PDDR &= ~MASK(sw3);

	//put the LEDs in a known state (off for active low config)
	PTB->PSOR =MASK(RED);	//turn off RED
	PTB->PSOR =MASK(GREEN);	//turn off GREEN
	PTD->PSOR =MASK(BLUE);	//turn off BLUE
}

/*delays for a couple of microseconds*/
static void delay_ms(int n){
	for (int j=0; j<n; j++)
		for (int i=0; i<1000; i++);
}

/*rotates r shape forward or backward depending and switch state*/
static void forward_or_back()
{
	enum rotate_state {S1, S2, S3, S4};
	static enum rotate_state next_state=S1;
	if(sw1_state)//go
	{
		PTD->PCOR=MASK(rled); 	//off ext red
		PTA->PCOR=MASK(yled); 	//off ext yellow
		if(sw2_state)//forward
		{
			if(sw3_state)//forward fast
			{
				if(rotate_fast_millis(100))
				{//switch section changes state after every 100ms to rotate the r shape
					switch(next_state){
						case S1: 
							PTA->PCOR=MASK(e);
							PTA->PSOR=MASK(g);
							PTD->PSOR=MASK(c);
							next_state=S2; 
							break;
						case S2: 
							PTA->PTOR=MASK(g);
							PTD->PSOR=MASK(c);
							PTA->PSOR=MASK(d);
							next_state=S3; 
							break;
						case S3: 
							PTD->PTOR=MASK(c);
							PTA->PSOR=MASK(d);
							PTA->PSOR=MASK(e);
							next_state=S4;
							break;
						case S4:
							PTA->PTOR=MASK(d);
							PTA->PSOR=MASK(e);
							PTA->PSOR=MASK(g);
							next_state=S1; 
							break;
						default: next_state=S1; break;
					}
				}
			}
			else//forward slow
			{
				if(rotate_slow_millis(200))
				{//switch section changes state after every 200ms to rotate the r shape
					switch(next_state){
						case S1: 
							PTA->PCOR=MASK(e);
							PTA->PSOR=MASK(g);
							PTD->PSOR=MASK(c);
							next_state=S2; 
							break;
						case S2: 
							PTA->PTOR=MASK(g);
							PTD->PSOR=MASK(c);
							PTA->PSOR=MASK(d);
							next_state=S3; 
							break;
						case S3: 
							PTD->PTOR=MASK(c);
							PTA->PSOR=MASK(d);
							PTA->PSOR=MASK(e);
							next_state=S4;
							break;
						case S4:
							PTA->PTOR=MASK(d);
							PTA->PSOR=MASK(e);
							PTA->PSOR=MASK(g);
							next_state=S1; 
							break;
						default: next_state=S1; break;
					}
				}
			}
		}
		else//back
		{
			if(sw3_state)//back fast
			{
				if(rotate_fast_millis(100))
				{//switch section changes state after every 100ms to rotate the r shape
					switch(next_state){
						case S1: 
							PTD->PCOR=MASK(c);
							PTA->PSOR=MASK(g);
							PTA->PSOR=MASK(e);
							next_state=S2; 
							break;
						case S2: 
							PTA->PTOR=MASK(g);
							PTA->PSOR=MASK(e);
							PTA->PSOR=MASK(d);
							next_state=S3; 
							break;
						case S3: 
							PTA->PTOR=MASK(e);
							PTA->PSOR=MASK(d);
							PTD->PSOR=MASK(c);
							next_state=S4;
							break;
						case S4:
							PTA->PTOR=MASK(d);
							PTD->PSOR=MASK(c);
							PTA->PSOR=MASK(g);
							next_state=S1; 
							break;
						default: 
							next_state=S1; 
							break;
					}
				}
			}
			else//back slow
			{
				if(rotate_slow_millis(200))
				{//switch section changes state after every 200ms to rotate the r shape
					switch(next_state){
						case S1: 
							PTD->PCOR=MASK(c);
							PTA->PSOR=MASK(g);
							PTA->PSOR=MASK(e);
							next_state=S2; 
							break;
						case S2: 
							PTA->PTOR=MASK(g);
							PTA->PSOR=MASK(e);
							PTA->PSOR=MASK(d);
							next_state=S3; 
							break;
						case S3: 
							PTA->PTOR=MASK(e);
							PTA->PSOR=MASK(d);
							PTD->PSOR=MASK(c);
							next_state=S4;
							break;
						case S4:
							PTA->PTOR=MASK(d);
							PTD->PSOR=MASK(c);
							PTA->PSOR=MASK(g);
							next_state=S1; 
							break;
						default: next_state=S1; break;
					}
				}
			}
		}
	}
}

/*flash pattern for external led in the stop state*/
static void stop_state(){
	enum flash_state {S1, S2, S3, S4, S5, S6, S7, S8, S9, S10};
	static enum flash_state next_state=S1;
	if(!sw1_state)
	{
		switch(next_state)
		{
			case S1:
				PTA->PCOR=MASK(yled); 	//off yellow
				PTD->PSOR=MASK(rled); 	//on red
				next_state = S2;
				break;
			case S2:
				PTD->PSOR=MASK(rled); 	//on red
				next_state = S3;
				break;
			case S3:
				PTD->PSOR=MASK(rled); 	//on red
				next_state = S4;
				break;
			case S4:
				PTD->PSOR=MASK(rled); 	//on red
				next_state = S5;
				break;
			case S5:
				PTD->PSOR=MASK(rled); 	//on red
				next_state = S6;
				break;
			case S6:
				PTD->PCOR=MASK(rled); 	//off red
				PTA->PSOR=MASK(yled); 	//on yellow
				next_state = S7;
				break;
			case S7:
				PTA->PSOR=MASK(yled); 	//on yellow
				next_state = S8;
				break;
			case S8:
				PTA->PSOR=MASK(yled); 	//on yellow
				next_state = S9;
				break;
			case S9:
				PTA->PSOR=MASK(yled); 	//on yellow
				next_state = S10;
				break;
			case S10:
				PTA->PSOR=MASK(yled); 	//on yellow
				next_state = S1;
				break;
			default:
				next_state = S1;
				break;
		}
		delay_ms(500);
	}
}

/*flash pattern blue onboard led when moving back*/
static void flashB(){
	if(sw1_state)
	{
		if(!sw2_state)
		{
			if(!sw3_state)
			{
				bmillis();
				if(bcounter<250)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else if(bcounter >= 250 && bcounter < 500)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}
				else if(bcounter >= 500 && bcounter < 750)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}
			}
		}
	}
}

/*flash pattern for white onboard led when forward*/
static void flashW(){
	if(sw1_state)
	{
		if(sw2_state)
		{
			if(!sw3_state)
			{
				wmillis();
				if(wcounter<250)
				{
					PTB->PCOR=MASK(RED); 	//on
					PTB->PCOR=MASK(GREEN); 	//on
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else if(wcounter >= 250 && wcounter < 500)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}
				else if(wcounter >= 600 && wcounter < 750)
				{
					PTB->PCOR=MASK(RED); 	//on
					PTB->PCOR=MASK(GREEN); 	//on
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}			
			}
		}
	}
}

/*flash pattern for onboard led when moving fast and forward*/
static void fast_forward_flash()
{
	if(sw1_state)
	{
		if(sw2_state)
		{
			if(sw3_state)
			{
				fast_flash_millis();
				if(fast_flash_counter < 100)
				{
					PTB->PCOR=MASK(RED); 	//on
					PTB->PCOR=MASK(GREEN); 	//on
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else if(fast_flash_counter >= 100 && fast_flash_counter <150)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off		
				}
				else if(fast_flash_counter >= 150 && fast_flash_counter <300)
				{
					PTB->PCOR=MASK(RED); 	//on
					PTB->PCOR=MASK(GREEN); 	//on
					PTD->PCOR=MASK(BLUE); 	//on		
				}
				else if(fast_flash_counter >= 300 && fast_flash_counter <400)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}
				else if(fast_flash_counter >= 400 && fast_flash_counter <750)
				{
					PTB->PCOR=MASK(RED); 	//on
					PTB->PCOR=MASK(GREEN); 	//on
					PTD->PCOR=MASK(BLUE); 	//on		
				}
				else
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off		
				}			
			}
		}
	}
}

/*flash pattern for onboard led when moving fast and backward*/
static void fast_back_flash()
{
	if(sw1_state)
	{
		if(!sw2_state)
		{
			if(sw3_state)
			{
				fast_flash_millis();
				if(fast_flash_counter < 100)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else if(fast_flash_counter >= 100 && fast_flash_counter <150)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off		
				}
				else if(fast_flash_counter >= 150 && fast_flash_counter <300)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PCOR=MASK(BLUE); 	//on
				}
				else if(fast_flash_counter >= 300 && fast_flash_counter <400)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off
				}
				else if(fast_flash_counter >= 400 && fast_flash_counter <750)
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PCOR=MASK(BLUE); 	//on	
				}
				else
				{
					PTB->PSOR=MASK(RED); 	//off
					PTB->PSOR=MASK(GREEN); 	//off
					PTD->PSOR=MASK(BLUE); 	//off		
				}			
			}
		}
	}
}

/*flash pattern for ext led when moving forward*/
static void forward_ext_flash()
{
	if(sw1_state)
	{
		if(sw2_state)
		{
			//flashes ext red yellow
			second_counter_millis();
			if(second_counter<1000)
			{
				PTA->PSOR=MASK(yled); 	//on		
			}
			else
			{
				PTA->PCOR=MASK(yled); 	//off
			}
			//flashes ext red led
			forward_red_counter_millis();
			if(forward_red_counter<600)
			{
				PTD->PSOR=MASK(rled); 	//on		
			}
			else
			{
				PTD->PCOR=MASK(rled); 	//off
			}		
		}
	}
}

/*flash pattern for ext led when moving backward*/
static void back_ext_flash()
{
	if(sw1_state)
	{
		if(!sw2_state)
		{
			//flashes ext red led
			second_counter_millis();
			if(second_counter<1000)
			{
				PTD->PSOR=MASK(rled); 	//on		
			}
			else
			{
				PTD->PCOR=MASK(rled); 	//off
			}
			//flashes yellow ext led
			back_yellow_counter_millis();
			if(back_yellow_counter<400)
			{
				PTA->PSOR=MASK(yled); 	//on		
			}
			else
			{
				PTA->PCOR=MASK(yled); 	//off
			}
		}
	}
}

/*checks if any of the buttons is pressed*/
static void check_inputs(){
	if(PTC->PDIR & MASK(sw1)){	//if sw1 is high
		sw1_state=1;
	}
	else
		sw1_state=0;
	if(PTC->PDIR & MASK(sw2)){	//if sw2 is high
		sw2_state=1;
		}
		else
		sw2_state=0;
	if(PTC->PDIR & MASK(sw3)){	//if sw3 is high
		sw3_state=1;
		}
		else
		sw3_state=0;
}

/********************************************************************
millis() functions are next. the millis funtions here do not 
do the same thing as traditional millis functions. These functions 
only set certain counter values to zero so they restart counting. based on
the value of such counters, some event could occur. This helps 
achieve concurrency in the program.
*********************************************************************/

/*millis() function for blue onboard led*/
static void bmillis()
{
	if(bcounter >= 1000)
	{
		bcounter =0;
	}
}

static void wmillis()
{
	if(wcounter >= 1000)
	{
		wcounter =0;
	}
}

/*millis() function for fast rotation*/
int rotate_fast_millis(int n)
{
	if(rotate_fast_counter >= n)
	{
		rotate_fast_counter =0;
		return 1;
	}
	else
	{
		return 0;
	}
}

/*millis() function for slow rotation*/
int rotate_slow_millis(int n)
{
	if(rotate_slow_counter >= n)
	{
		rotate_slow_counter =0;
		return 1;
	}
	else
	{
		return 0;
	}
}

/*millis() function for fast flashing of onboard led*/
static void fast_flash_millis()
{
	if(fast_flash_counter >= 1000)
	{
		fast_flash_counter =0;
	}
}

/*millis() function for external leds when there are supposed to blink every second*/
static void second_counter_millis()
{
	if(second_counter>=2000)
	{
		second_counter = 0;
	}
}

/*millis() function for ext red led when in forward rotation*/
static void forward_red_counter_millis()
{
	if(forward_red_counter>=1200)
	{
		forward_red_counter = 0;
	}
}

/*millis() function for ext yellow led when in backward rotation*/
static void back_yellow_counter_millis()
{
	if(back_yellow_counter>=800)
	{
		back_yellow_counter = 0;
	}
}
/*SysTick_Handler ---- keeps increasing the count variables for millis functions*/
void SysTick_Handler(){
	bcounter++;
	wcounter++;
	rotate_fast_counter++;
	rotate_slow_counter++;
	fast_flash_counter++;
	second_counter++;
	forward_red_counter++;
	back_yellow_counter++;
}
