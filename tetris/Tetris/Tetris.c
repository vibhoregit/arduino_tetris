#include "font.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#define F_CPU 16000000L
#define d_595 1<<PORTB1
#define c_595 1<<PORTB0
#define latch 1<<PORTB2

uint8_t i = 0;
uint8_t j = 0;
uint8_t spawn_block_flag = 0;
uint16_t delay_var = 0;
uint8_t screen[18] = {0};
uint8_t game_over = 0;
const uint8_t tetris_code[] = {52, 37, 52, 50, 41, 51};
uint8_t block[4] = {0};
uint8_t block_id = 0;
uint8_t score = 0;
int8_t block_x = 0;
int8_t block_y = 0;	
uint8_t update_flag = 0;
uint8_t now = 0;
uint8_t last = 0x0F;

void init_spi()
{
	SPCR |= (1<<SPE|1<<MSTR);			// SPI clock 1MHz	 
}

void init_gpio()
{
	#if defined(UCSRB)
		UCSRB = 0;
	#elif defined(UCSR0B)
		UCSR0B = 0;
	#endif
	PRR	 |= 1<<PRUSART0;
	DDRD  = 0xFF;
	DDRC  |= 0x01;
	PORTC |= 0x1E;						// Enable pull-up on PORTC A1-A4 and make A5 high
	PCICR |= 1<<PCIE1;					// Enable pin change interrupt on PORTC
	PCMSK1|= 0x1E;						// Enable pin change interrupt on PORTC A0-A3
	DDRB  |= 0xFF;						// Setting MOSI, SS and SCK as outputs along with PB1 and PB0
	PORTB &= ~0x2F;
	
}

void init_timer1()
{
	TCCR1B |= (1<<CS12)|(1 << WGM12)|(1<<CS10);		// clk = clk_io/1 = 1 MHz
	OCR1A  = 14000;					// compare match every half second (1.9Hz actually)
	TIMSK1 |= 1<<OCIE1A;			// Enable compare match interrupt on OC1A
}
void adc_init()
{
    // AREF = Internal 1.1V
    ADMUX |= (1<<REFS0);
    ADCSRA|= (1<<ADEN)|(1<<ADPS2);
    ADMUX |= 5;
}

uint8_t adc_read()
{
  ADCSRA |= (1<<ADSC);
  while((ADCSRA & (1<<ADSC)));
  return ADC>>2;
}

void send_byte(uint8_t data)
{
	SPDR = data;
	while(!(SPSR&(1<<SPIF)));
}

void send_int(int data)
{
	send_byte(data>>8);
	send_byte(data&0xff);
}
void init_game()
{
	spawn_block_flag = 1;
	game_over = 0;
	score = 0;
	for(i = 0; i<4; i++)
		block[i] = 0;
	for(i = 0; i<18; i++)
		screen[i] = 0;	
}



void draw()
{
	uint8_t i_loc = 0;
	for(i_loc = 0; i_loc < 12; i_loc++)
		{	
			send_int((int)(1<<i_loc));
			PORTB |= latch;
			PORTD = ~(screen[i_loc+4]);
			PORTB &= ~(latch);
			_delay_ms(1);
		}	
	
}

void tetris()
{
	i = 0;
	j = 0;
	while(i < 6)
	{
		delay_var = 0;
		for(j = 6; j < 14; j++)
			screen[j] = pgm_read_byte(&(SmallFont[-2+j+8*tetris_code[i]]))&0x7E;
		for(j = 14; j < 16; j++)
			screen[j] = 0;
		while(delay_var < 1000)
		{
			draw();
			delay_var++;
		}
		i++;
	}		
}

void draw_game()
{
	uint8_t i_loc = 0;
	uint8_t data_loc = 0;
	for(i_loc = 4; i_loc < 16; i_loc++)
		{	
			send_int((int)(1<<(i_loc-4)));
			if(block_x>=0)
			{
				if(i_loc >= block_y && i_loc - block_y < 4)
					data_loc = screen[i_loc]|(block[i_loc - block_y]>>block_x);
				else
					data_loc = screen[i_loc];
			}
			else
			{
				if(i_loc >= block_y && i_loc - block_y < 4)
					data_loc = screen[i_loc]|(block[i_loc - block_y]<<(~block_x+1));
				else
					data_loc = screen[i_loc];
			}				
			PORTB |= latch;
			PORTB &= ~(latch);
			PORTB |= latch;
			PORTD = ~(data_loc);
			PORTB &= ~(latch);
			_delay_ms(1);
		}	
	
}


void copy_shape()
{
	block[0] = pgm_read_byte(&(shapes[4*block_id]));
	block[1] = pgm_read_byte(&(shapes[4*block_id+1]));
	block[2] = pgm_read_byte(&(shapes[4*block_id+2]));
	if(block_id >15)
		block[3] = pgm_read_byte(&(shapes[4*block_id+3]));
	else
		block[3] = 0;
}	

void spawn_block()
{
	uint8_t i_loc = 0;
	uint8_t j_loc = 1;
	if(block_x>=0)
	{
	screen[block_y]	  |= (uint8_t)block[0]>>block_x;
	screen[block_y+1] |= (uint8_t)block[1]>>block_x;  
	screen[block_y+2] |= (uint8_t)block[2]>>block_x;
	if(block_id == 17)
		screen[block_y+3] |= (uint8_t)block[3]>>block_x;
	}
	else
	{
	screen[block_y]	  |= (uint8_t)block[0]<<(~block_x+1);
	screen[block_y+1] |= (uint8_t)block[1]<<(~block_x+1);  
	screen[block_y+2] |= (uint8_t)block[2]<<(~block_x+1);
	if(block_id == 17)
		screen[block_y+3] |= (uint8_t)block[3]<<(~block_x+1);	
	}		
	
		
	if(screen[2] != 0)
		game_over = 1;
	
	for(i_loc = 15; i_loc>3; )
	{
		if(screen[i_loc] == 0xff)
			{
				for(j_loc = i_loc; j_loc>0; j_loc--)
					screen[j_loc] = screen[j_loc-1];
				score++;
			}			
		else
			i_loc--;
	}
	block_id = (adc_read()+TCNT1L)%19;
	//screen[15] = block_id;
	//screen[14] = ADCL;
	if(block_id < 16)
	{
		block_x = 3;
		block_y = 1;
	}
	else if(block_id == 16)
	{
		block_x = 2;
		block_y = 2;
	}
	else if(block_id == 17)
	{
		block_x = 3;
		block_y = 0;
	}
	else
	{
		block_x = 2;
		block_y = 1;
		block_id = 18;
	}
	copy_shape();
	
}
uint8_t check_sum()
{
	uint8_t i_loc = 0;
	uint8_t sum = 0;
	if(block_x >=0)
	{
		for(i_loc = block_y; i_loc < block_y+4; i_loc++)
		{
			sum += screen[i_loc]&((uint8_t)block[i_loc-block_y]>>block_x);
		}
	}
	else
	{
		for(i_loc = block_y; i_loc < block_y+4; i_loc++)
		{
			sum += screen[i_loc]&((uint8_t)block[i_loc-block_y]>>(1+(~block_x)));
		}
	}				
	return sum;
}
void clear_screen()
{
	uint8_t i_loc = 0;
	for(i_loc = 0; i_loc < 18; i_loc++)
		screen[i_loc] = 0;
}
void show_score()
{
	clear_screen();
	uint8_t i_loc = 0;
	uint8_t tens = score/10;
	uint8_t ones = score%10;
	uint8_t j_loc = 0;
	while(j_loc<10)
	{
		for(i_loc = 0; i_loc < 5; i_loc++)
			screen[i_loc+8] = pgm_read_byte(&(numbers[5*tens+i_loc])) | ((uint8_t)pgm_read_byte(&(numbers[5*ones+i_loc]))>>4);
		delay_var = 0;
		while(delay_var < 600)
		{
			draw();
			delay_var++;
		}
		clear_screen();
		delay_var = 0;
		while(delay_var < 600)
		{
			draw();
			delay_var++;
		}
		j_loc++;
	}		
}

int main(void)
{
	cli();
	init_gpio();
	init_spi();
	init_timer1();
	adc_init();
	while(1)
	{
		init_game();
		tetris();
		clear_screen();
		sei();
		while(game_over == 0)
			{
				if(spawn_block_flag == 1)
				{
					spawn_block();
					spawn_block_flag = 0;
				}
				if(update_flag == 1)
				{
					switch(block_id)
						{
							case 0:
								if(((screen[block_y+2]&((uint8_t)0xC0>>block_x))+(screen[block_y+3]&((uint8_t)0x20>>block_x)) == 0) && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 1:
								if((screen[block_y+3]&((uint8_t)0xC0>>block_x)) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 2:
								if((screen[block_y+3]&((uint8_t)0xE0>>block_x)) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 3:
								if((screen[block_y+1]&((uint8_t)0x40>>(block_x+1)))+(screen[block_y+3]&((uint8_t)0x80>>(block_x+1))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 4:
								if((screen[block_y+2]&((uint8_t)0x60>>block_x))+(screen[block_y+3]&((uint8_t)0x80>>block_x)) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 5:
								if((screen[block_y+1]&((uint8_t)0x80>>block_x))+(screen[block_y+3]&((uint8_t)0x40>>block_x)) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 6:
								if((screen[block_y+2]&((uint8_t)0x60>>block_x))+(screen[block_y+3]&((uint8_t)0x80>>block_x)) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 7:
								if((screen[block_y+3]&((uint8_t)0xC0>>(block_x+1))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 8:
								if((screen[block_y+2]&((uint8_t)0xA0>>(block_x)))+(screen[block_y+3]&((uint8_t)0x40>>(block_x))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 9:
								if((screen[block_y+2]&((uint8_t)0x80>>(block_x)))+(screen[block_y+3]&((uint8_t)0x40>>(block_x))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 10:
								if((screen[block_y+3]&((uint8_t)0xE0>>(block_x))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 11:
								if((screen[block_y+2]&((uint8_t)0x40>>(block_x+1)))+(screen[block_y+3]&((uint8_t)0x80>>(block_x+1))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 12:
								if((screen[block_y+2]&((uint8_t)0x20>>(block_x)))+(screen[block_y+3]&((uint8_t)0xC0>>(block_x))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 13:
								if((screen[block_y+2]&((uint8_t)0x80>>(block_x)))+(screen[block_y+3]&((uint8_t)0x40>>(block_x))) ==  0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 14:
								if((screen[block_y+2]&((uint8_t)0x80>>(block_x)))+(screen[block_y+3]&((uint8_t)0x60>>(block_x))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 15:
								if((screen[block_y+2]&((uint8_t)0x40>>(block_x+1)))+(screen[block_y+3]&((uint8_t)0x80>>(block_x+1))) == 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 16:
								if((screen[block_y+2]&((uint8_t)0xF0>>(block_x))) == 0 && block_y < 14)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 17:
								if((screen[block_y+4]&((uint8_t)0x80>>(block_x+2))) == 0 && block_y < 12)
									block_y++;
								else
									spawn_block_flag = 1;
								break;
							case 18:
								if((screen[block_y+3]&((uint8_t)0xC0>>(block_x+1)))== 0 && block_y < 13)
									block_y++;
								else
									spawn_block_flag = 1;
								break;	
							default:
								break;
						}
						update_flag = 0;
				}
				if(update_flag==2)
				{
					switch((now^last)&last)
					{
						case 1:
								if((block_id&3) == 1 && block_x < (6+(block_id>>4)))
									block_x++;
								else if(block_x<5)
									block_x++;
								if(block_id==17&&block_x>5)
									block_x=5;
								if((block_id == 16&&block_x>4)||check_sum()!=0)
									block_x--;
								break;
								
						case 2:
								if(block_id <12)
									{
										block_id = (block_id&0xFC)|((block_id+1)&0x03);
										copy_shape();
										if((block_id&1)==0 && block_x > 5)
												block_x--;
										if((block_id&1)==0 && block_x < 0)
												block_x++;
										if(check_sum() != 0)
										{
											block_id = (block_id&0xFC)|((block_id-1)&0x03);
											copy_shape();
										}
									}
								else if(block_id < 16)
									{
										block_id = (block_id&0xFE)|((block_id+1)&0x01);
										copy_shape();
										if((block_id&1)==0 && block_x > 5)
												block_x--;
										if((block_id&1)==0 && block_x < 0)
												block_x++;
										if(check_sum() != 0)
										{
											block_id = (block_id&0xFE)|((block_id-1)&0x01);
											copy_shape();
										}
									}
								else if(block_id < 18)
									{
										block_id = (block_id&0xFE)|((~block_id)&0x01);
										copy_shape();
										if(block_id==16&&block_x>4)
											block_x = 4;
										if(block_id==16&&block_x<0)
											block_x = 0;
										if(check_sum() != 0)
										{
											block_id = (block_id&0xFE)|((~block_id)&0x01);
											copy_shape();
										}											
									}
								break;															
						case 4:
								TCNT1 = 4000;
								OCR1A = 5000;
								break;
						case 8:
								if((block_id&3)==3 && block_x>=0)
									block_x--;
								else if(block_id == 17 && block_x > -2)
									block_x--;
								else if(block_id == 18 && block_x > -1)
									block_x--;
								else if (block_x > 0)
									block_x--;
								if(check_sum() != 0)
									block_x++;
								break;
						default:
								OCR1A = 14000;
								break;
					}
				update_flag = 0;	
				}
				draw_game();
			}	
			last = now;			
		show_score();
	}	
    /*while(1)
    {	
		for(i = 1; i > 0; i = i<<1)
		{	send_byte(i);
			PORTB |= latch;
			//_delay_ms(10);
			PORTB &= ~(latch);
			for(j = 0; j < 8; j++)
			{
				PORTD = ~(1<<j);
				_delay_ms(1);
			}
			
		}			
			  
    }*/
}

ISR (TIMER1_COMPA_vect)
{
	update_flag = 1;	
}
ISR (PCINT1_vect)
{
	now = (PINC&0x1E)>>1;
	update_flag = 2;
}