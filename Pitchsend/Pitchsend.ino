#ifndef TIMERONE_h
#define TIMERONE_h

#include <avr/io.h>
#include <avr/interrupt.h>

#define RESOLUTION 65536    // Timer1 is 16 bit

class TimerOne
{
  public:
  
    // properties
    unsigned int pwmPeriod;
    unsigned char clockSelectBits;
	char oldSREG;					// To hold Status Register while ints disabled

    // methods
    void initialize(long microseconds=1000000);
    void start();
    void stop();
    void restart();
	void resume();
	unsigned long read();
    void pwm(char pin, int duty, long microseconds=-1);
    void disablePwm(char pin);
    void attachInterrupt(void (*isr)(), long microseconds=-1);
    void detachInterrupt();
    void setPeriod(long microseconds);
    void setPwmDuty(char pin, int duty);
    void (*isrCallback)();
};

extern TimerOne Timer1;
#endif


//////////////////////////////

#ifndef TIMERONE_cpp
#define TIMERONE_cpp


TimerOne Timer1;              // preinstatiate

ISR(TIMER1_OVF_vect)          // interrupt service routine that wraps a user defined function supplied by attachInterrupt
{
  Timer1.isrCallback();
}


void TimerOne::initialize(long microseconds)
{
  TCCR1A = 0;                 // clear control register A 
  TCCR1B = _BV(WGM13);  // set mode 8: phase and frequency correct pwm, stop the timer
  TCCR2B = _BV(WGM13);
  setPeriod(microseconds);
}


void TimerOne::setPeriod(long microseconds)		// AR modified for atomic access
{
  
  long cycles = (F_CPU / 2000000) * microseconds;                                // the counter runs backwards after TOP, interrupt is at BOTTOM so divide microseconds by 2
  if(cycles < RESOLUTION)              clockSelectBits = _BV(CS10);              // no prescale, full xtal
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11);              // prescale by /8
  else if((cycles >>= 3) < RESOLUTION) clockSelectBits = _BV(CS11) | _BV(CS10);  // prescale by /64
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12);              // prescale by /256
  else if((cycles >>= 2) < RESOLUTION) clockSelectBits = _BV(CS12) | _BV(CS10);  // prescale by /1024
  else        cycles = RESOLUTION - 1, clockSelectBits = _BV(CS12) | _BV(CS10);  // request was out of bounds, set as maximum
  
  oldSREG = SREG;				
  cli();							// Disable interrupts for 16 bit register access
  ICR1 = pwmPeriod = cycles;                                          // ICR1 is TOP in p & f correct pwm mode
  SREG = oldSREG;
  
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
  TCCR1B |= clockSelectBits; 
  
  TCCR2B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
  TCCR2B |= clockSelectBits;                    // reset clock select register, and starts the clock
}

void TimerOne::setPwmDuty(char pin, int duty)
{
  unsigned long dutyCycle = pwmPeriod;
  
  dutyCycle *= duty;
  dutyCycle >>= 10;
  
  oldSREG = SREG;
  cli();
  if(pin == 1 || pin == 9)       OCR1A = dutyCycle;
  else if(pin == 2 || pin == 10) OCR1B = dutyCycle;
  SREG = oldSREG;
}

void TimerOne::pwm(char pin, int duty, long microseconds)  // expects duty cycle to be 10 bit (1024)
{
  if(microseconds > 0) setPeriod(microseconds);
  if(pin == 1 || pin == 9) {
    DDRB |= _BV(PORTB1);                                   // sets data direction register for pwm output pin
    TCCR1A |= _BV(COM1A1);                                 // activates the output pin
  }
  else if(pin == 2 || pin == 10) {
    DDRB |= _BV(PORTB2);
    TCCR1A |= _BV(COM1B1);
  }
  setPwmDuty(pin, duty);
  resume();			// Lex - make sure the clock is running.  We don't want to restart the count, in case we are starting the second WGM
					// and the first one is in the middle of a cycle
}

void TimerOne::disablePwm(char pin)
{
  if(pin == 1 || pin == 9)       TCCR1A &= ~_BV(COM1A1);   // clear the bit that enables pwm on PB1
  else if(pin == 2 || pin == 10) TCCR1A &= ~_BV(COM1B1);   // clear the bit that enables pwm on PB2
}

void TimerOne::attachInterrupt(void (*isr)(), long microseconds)
{
  if(microseconds > 0) setPeriod(microseconds);
  isrCallback = isr;                                       // register the user's callback with the real ISR
  TIMSK1 = _BV(TOIE1);                                     // sets the timer overflow interrupt enable bit
	// AR - remove sei() - might be running with interrupts disabled (eg inside an ISR), so leave unchanged
//  sei();                                                   // ensures that interrupts are globally enabled
  resume();
}

void TimerOne::detachInterrupt()
{
  TIMSK1 &= ~_BV(TOIE1);                                   // clears the timer overflow interrupt enable bit 
}

void TimerOne::resume()				// AR suggested
{ 
  TCCR1B |= clockSelectBits;
  TCCR2B |= clockSelectBits;
}

void TimerOne::restart()		// Depricated - Public interface to start at zero - Lex 10/9/2011
{
	start();				
}

void TimerOne::start()	// AR addition, renamed by Lex to reflect it's actual role
{
  unsigned int tcnt1;
  
  TIMSK1 &= ~_BV(TOIE1);        // AR added 
  GTCCR |= _BV(PSRSYNC);   		// AR added - reset prescaler (NB: shared with all 16 bit timers);

  oldSREG = SREG;				// AR - save status register
  cli();						// AR - Disable interrupts
  TCNT1 = 0;                	
  SREG = oldSREG;          		// AR - Restore status register

  do {	// Nothing -- wait until timer moved on from zero - otherwise get a phantom interrupt
	oldSREG = SREG;
	cli();
	tcnt1 = TCNT1;
	SREG = oldSREG;
  } while (tcnt1==0); 
 
//  TIFR1 = 0xff;              		// AR - Clear interrupt flags
//  TIMSK1 = _BV(TOIE1);              // sets the timer overflow interrupt enable bit
}

void TimerOne::stop()
{
  TCCR1B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));          // clears all clock selects bits
  TCCR2B &= ~(_BV(CS10) | _BV(CS11) | _BV(CS12));
}

unsigned long TimerOne::read()		//returns the value of the timer in microseconds
{									//rember! phase and freq correct mode counts up to then down again
  	unsigned long tmp;				// AR amended to hold more than 65536 (could be nearly double this)
  	unsigned int tcnt1;				// AR added

	oldSREG= SREG;
  	cli();							
  	tmp=TCNT1;    					
	SREG = oldSREG;

	char scale=0;
	switch (clockSelectBits)
	{
	case 1:// no prescalse
		scale=0;
		break;
	case 2:// x8 prescale
		scale=3;
		break;
	case 3:// x64
		scale=6;
		break;
	case 4:// x256
		scale=8;
		break;
	case 5:// x1024
		scale=10;
		break;
	}
	
	do {	// Nothing -- max delay here is ~1023 cycles.  AR modified
		oldSREG = SREG;
		cli();
		tcnt1 = TCNT1;
		SREG = oldSREG;
	} while (tcnt1==tmp); //if the timer has not ticked yet

	//if we are counting down add the top value to how far we have counted down
	tmp = (  (tcnt1>tmp) ? (tmp) : (long)(ICR1-tcnt1)+(long)ICR1  );		// AR amended to add casts and reuse previous TCNT1
	return ((tmp*1000L)/(F_CPU /1000L))<<scale;
}

#endif



#define pwmRegister OCR1A    // the logical pin, can be set to OCR1B
const int   outPin1 =  9;
const int   outPin3 =  10;   // the physical pin

long period = 20000;      // the period in microseconds

int prescale[] = {0,1,8,64,256,1024}; // the range of prescale values
int pw1, pw3;
int dpwt;
int pw0 = 1000;
int dpw = 0;

int xg, yg, xa, ya, za;
int timer, reset = 0;
int dtime = 0;
float xangleg = 0, yangleg = 0, xanglea = 0, yanglea = 0;

void setup()
{
  Serial.begin(9600);
  pinMode(outPin1, OUTPUT);
  pinMode(outPin3, OUTPUT);
  pinMode(13,OUTPUT);
  Timer1.initialize(period);        // initialize timer1, 1000 microseconds
  pw1 = 1000;
  pw3 = 1000;
  setPulseWidth(pw1, pw3);
  delay(1000);
}


void loop()
{
  xg = analogRead(3)-279;
  yg = analogRead(4)-277;
  xa = analogRead(0)-338;
  ya = analogRead(1)-335;
  za = analogRead(2)-348;
    
  pw0 = pulseIn(2,HIGH);
  dpwt = pulseIn(3,HIGH);
  int pwyt = pulseIn(4,HIGH);
 
  dtime = millis() - timer;
  timer = millis();
  xangleg+=xg*dtime/100;
  yangleg+=yg*dtime/100;
  xanglea = RAD_TO_DEG*(atan2(ya, za));
  yanglea = RAD_TO_DEG*(atan2(xa, za));
  
  xangleg = .65*xangleg + .35* xanglea;
  yangleg = .65*yangleg + .35* yanglea;
  
  int pass = 50*(xangleg + 200);
  
  digitalWrite(13,HIGH);
  delayMicroseconds(pass);
  digitalWrite(13,LOW);
  
  dpw = (dpwt - 1488)/2+25;
  int pwy = (pwyt - 1492)/2;
  
  dpw-=yangleg*3+dpw/5;
  
  pw1 = pw0 + dpw - pwy;
  pw3 = pw0 - dpw - pwy;
  
  setPulseWidth(pw1, pw3); //pw in microsec
  
 /* Serial.print("xg: ");
    Serial.print(xg);
    Serial.print(" yg: ");
    Serial.print(yg);
    Serial.print(" xa: ");
    Serial.print(xa);
    Serial.print(" ya: ");
    Serial.print(ya);
    Serial.print(" za: ");
    */Serial.print(dpw);
    Serial.print(" <dpw ");
    
   
    Serial.print(xangleg);
    Serial.print(" <angles> ");
    Serial.print(yangleg);
    Serial.print("  ");
    Serial.print(xanglea);
    Serial.print(" <angles> ");
    Serial.print(yanglea);
    Serial.print("  ");
  Serial.print("pw1 = ");
  Serial.print(pw1);
  Serial.print("pw3 = ");
  Serial.println(pw3);


}

bool setPulseWidth(long microseconds1, long microseconds2)
{
  bool ret1 = false;
  bool ret2 = false;
  int outPin11, outPin22;
  int prescaleValue = prescale[Timer1.clockSelectBits];
  // calculate time per counter tick in nanoseconds
  long  precision = (F_CPU / 128000)  * prescaleValue;   
  
  outPin11 = outPin1;
  outPin22 = outPin3;
  period = precision * ICR1 / 1000; // period in microseconds
  if( microseconds1 < period)
  {
    int duty1 = map(microseconds1, 0,period, 0,1024);
    
    if( duty1 < 1)
      duty1 = 1;
    
    if(microseconds1 > 0 && duty1 < RESOLUTION)
    {
       Timer1.pwm(outPin11, duty1);

       ret1 = true;
    }
    
  }
  if( microseconds2 < period)
  {
    int duty2 = map(microseconds2, 0,period, 0,1024);
    
    if( duty2 < 1)
      duty2 = 1;
    
    if(microseconds2 > 0 && duty2 < RESOLUTION)
    {
       Timer1.pwm(outPin22, duty2);

       ret2 = true;
    }
    
  }
  
  return ret1 && ret2;
}
