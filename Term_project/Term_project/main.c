#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "lcd.h"
#include "keypad.h"

/**************** Segment define ���� ****************/

#define N_BUF 5
#define N_SEGMENT 4
#define CONT_MASK ((1<<PB3)|(1<<PB2)|(1<<PB1)|(1<<PB0))
#define OUTPUT_VALUE (1<<PB0)


/********************�Լ� ����**********************/

void msec_delay(int n); // delay �Լ� ����
void lcd_seg_velocity(short value); //lcd�� segment�� ǥ���� ���� ����� �Լ� ����
void ISeg7DispNum(unsigned short num, unsigned short radix);

/****************Segment ���� �����*************/

static unsigned char SegTable[17]
={0x3f, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x58, 0x5E, 0x79, 0x71, 0x00};
//Segment ���� 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, c, d, e, F
static unsigned char cnumber[N_BUF] = {16,16,16,16,16}; //Segment 16���� ��� �ʱ�ȭ

/*****************Dot Matrix ���� �����***********/

unsigned char Dot_Basic_Driving[4][8] = { //�ʺ����� ���� �����
	{0xE7,0x00,0xDB,0xBD,0x7E,0xE7,0xE7,0x00}, //��
	{0x7E,0x7E,0x00,0x7E,0x00,0xE7,0xE7,0x00}, //��
	{0xE7,0xDB,0xE7,0xFF,0x00,0xE7,0x7F,0x00}, //��
	{0x06,0xDE,0xA8,0x76,0xFE,0x7F,0x7F,0x00}  //��
};

/************LCD�� ����� ���� 5�� �����************/

char font1[8]= {0x01, 0x01, 0x09, 0x15, 0x15, 0x15, 0x01, 0x01}; //��
char font2[8]= {0x1F, 0x10, 0x1F, 0x04, 0x1F, 0x04, 0x0A, 0x04}; //��
char font3[8]= {0x0E, 0x04, 0x0A, 0x1F, 0x04, 0x04, 0x0A, 0x04}; //��
char font4[8]= {0x04, 0x04, 0x0A, 0x0A, 0x11, 0x11, 0x00, 0x1F}; //��
char font5[8]= {0x1B, 0x13, 0x1F, 0x13, 0x1B, 0x00, 0x1F, 0x1F}; //��

/*****************���� ���� ����********************/
unsigned short power=0;
unsigned short emergency_on_off_test=0;
unsigned short on_off_test=0;
unsigned short present_state=0;

char present_velocity[4];
char velocity_unit[6];
char emergency_ment[5];
char start_ment[5];
char end_ment[8];

/************�����÷ο� Ÿ�̸�2 ���ͷ�Ʈ ���*********/
ISR(TIMER2_OVF_vect)
{
	static short type = 0;
	static short index = 0;
	static short emergency_state = 0;
	static int one_sec_change = 0;
	static int seg_index = 0;

	/*************������ ������ ��*************/
	if(present_state){
		/*************Segment �κ�***************/
		PORTB = ( PORTB & ~CONT_MASK)|(~(OUTPUT_VALUE<<seg_index)&CONT_MASK);
		PORTE = ~SegTable[cnumber[seg_index]];
		seg_index++;
		if(seg_index == N_SEGMENT) seg_index=0;
		//Segment ���

		/*****Dot matrix & ���� �κ�**********/
		if(++index==8) index=0;
		PORTD = 0x01 << index;
		PORTA = Dot_Basic_Driving[type][index];
		//type�� ����, index�� ���ư��� ���

		one_sec_change++;
		if(one_sec_change == 1000) //1sec
		{
			one_sec_change = 0;
			if(++type==4) type=0; //1�� ���� "�ʺ�����" ���ڰ� �ٲ�
			
			if(emergency_on_off_test){ // ���� ���� 1�ʸ��� �����ư��鼭 on off
				if(emergency_state){
					PORTB=PORTB | 0x60; // 1<<PB6 1<<PB5 ��
					LcdMove(0,6);
					LcdPutchar(0x20);
					LcdMove(0,9);
					LcdPutchar(0x20);
					//�������� ������
					emergency_state=0;
				}
				else{
					PORTB=PORTB & 0x9F; // 0<<PB6 0<<PB5 Ŵ
					LcdMove(0,6);
					LcdPuts(emergency_ment);
					// <-  -> ���
					emergency_state=1;
				}
			}
		}
	}
	/***************������ ������ ��****************/
	else{
		index=0; // ������ ���鼭 0���� �ʱ�ȭ
		type=0;  // ������ ���鼭 0���� �ʱ�ȭ
		PORTA = 0xFF; //Dot matrix ���� ����
		PORTE = 0xFF; //Segment ���� ����
	}
	TCNT2 = 6; //1msec 

}

/***********************���� �Լ�*********************/
int main()
{
	/*****************���� ����*****************/
	static volatile unsigned char duty = 5;
	unsigned char key;

	unsigned short right_motion=0;
	unsigned short left_motion=0;
	unsigned short emergency_motion=0;
	unsigned short headlight_motion=0;
	unsigned short break_motion=0;
	unsigned short original_velocity=0;
	unsigned short reduced_velocity=0;

	/*****************��Ʈ ����*****************/
	PORTB = 0xFF;
	PORTD = 0x00;
	PORTA = 0x00;
	PORTE = 0xFF;
	//���� �ʱⰪ�� ���� ����(PB5~7�� 1�� �����ؾ� ����)
	// PORTE�� 0�̸� ������ ������ 1�� �Է�
	DDRB = 0xFF;
	DDRA = 0xFF;
	DDRD = 0xFF;
	DDRE = 0xFF;
	//B, D, A���� ������� ���
	
	/**************���ͷ�Ʈ ����****************/
	TCCR0=(1<<WGM01)|(1<<WGM00)|(2<<COM00); //PWM ��� ����
	TCCR0 |=(0x02<<CS00); //���ֺ� 1024
	//PWM ����ϱ� ���� ����

	TCCR2 = 0x00; // Normal ���� �⺻���� 0
	TCCR2 |=(3 << CS20); //���ֺ� 64
	TIMSK = (1 << TOIE2); // Ÿ�̸�2 �����÷ο� ���ͷ�Ʈ ���
	TCNT2 = 6; //1msec[256-1*16*1000/64]
	//Normal mode Ÿ�̸�2 ���ͷ�Ʈ ����ϱ� ���� ����

	sei();

	/****************�ʱ�ȭ �Լ�*****************/

	KeyInit(); //Keypad �ʱ�ȭ �Լ�
	LcdInit(); //LCD �ʱ�ȭ �Լ�

	/****************���� �����*****************/

	LcdNewchar(0, font1);
	LcdNewchar(1, font2);
	LcdNewchar(2, font3);
	LcdNewchar(3, font4);
	LcdNewchar(4, font5);
	// ������� ��, ��, ��, ��, �� ���� ���

	start_ment[0]= 8;
	start_ment[1]= 1;
	start_ment[2]= ' ';
	start_ment[3]= 2;
	start_ment[4]= '\0';
	//�õ���

	end_ment[0]= 8;
	end_ment[1]= 3;
	end_ment[2]= 4;
	end_ment[3]= ' ';
	end_ment[4]= 'o';
	end_ment[5]= 'f';
	end_ment[6]= 'f';
	end_ment[7]= '\0';
	// �ý��� off

	emergency_ment[0]=0x7F;
	emergency_ment[1]=' ';
	emergency_ment[2]=' ';
	emergency_ment[3]=0x7E;
	emergency_ment[4]='\0';
	// <-  ->

	velocity_unit[0]=' ';
	velocity_unit[1]='k';
	velocity_unit[2]='m';
	velocity_unit[3]='/';
	velocity_unit[4]='h';
	velocity_unit[5]='\0';
	// km/h

	for (int i=0; i<3; i++)
	{
		present_velocity[i]='0';
	}
	present_velocity[3]='\0';
	// 000�ӵ�����ǥ��

	/***************�ý��� ����****************/
	while(1){
		/*********Ű �Է� �κ�*********/
		key = KeyInput(); // �Էµ� Ű ������ ����
		switch(key){
			case SW3: //�õ�off
				power=0;
				break;
			case SW4: //��ȸ��
				left_motion = 1;
				break;
			case SW6: //��ȸ��
				right_motion = 1;
				break;
			case SW7: //����
				emergency_motion = 1;
				break;
			case SW11: //������
				headlight_motion = 1;
				break;
			default:
				break;
		}

		key = KeypadInput_Press(); //������ �극��ũ�� Ű�� ���������� ����
		switch(key){
			case SW1: //����
				duty +=5;
				if(duty >250) duty=250;
				break;
			case SW9: //�극��ũ
				duty -=10;
				if(duty <10) duty=10;
				break_motion=1;
				break;
			default: // ������ ��� �ƹ� �͵� �Է����� �ʾ��� �� ������ �ӵ��� �پ��
				duty -=1;
				if(duty <15) duty=15;
				break;
		}

		/*********�õ� off��*********/
		if(present_state == 1 && power == 0){ //���� ���¿��� power = 0 �� �Ǹ� ����
			present_state = 0; // �ý��� off
			duty=5;
			OCR0 = duty; 
			//DC motor ����

			PORTB = 0xF0;
			//LED�� Segment ��

			PORTD = 0x00;
			PORTA = 0x00;
			//Dot Matrix ��
			
			emergency_on_off_test=0; //���� off
			on_off_test=0; //������ off

			LcdCommand(ALLCLR);
			LcdMove(0,5);
			LcdPuts(end_ment);
			//�ʱ�ȭ �� "�ý��� off" ���

			for (int i=0; i<3; i++) //1�� ���� ������(�ٸ� Ű �Էµ��� ����)
			{
				msec_delay(1000);
				LcdCommand(DISP_OFF);
				msec_delay(1000);
				LcdCommand(DISP_ON);
			}

			msec_delay(1000);
			LcdCommand(ALLCLR);
			LcdCommand(DISP_OFF);
			//LCDȭ�� off
			
		}

		/**********�õ� ��*********/
		if(present_state == 0 && power == 1){ //���� ���¿��� power = 1 �� �Ǹ� ����
			LcdMove(0,6);
			LcdPuts(start_ment);
			//"�õ���" ���

			for (int i=0; i<3; i++) //1�� ���� ������(�ٸ� Ű �Էµ��� ����)
			{
				msec_delay(1000);
				LcdCommand(DISP_OFF);
				msec_delay(1000);
				LcdCommand(DISP_ON);
			}
			msec_delay(1000);
			LcdCommand(ALLCLR);
			//ȭ�� �ʱ�ȭ
			LcdMove(1,7);
			LcdPuts(velocity_unit);
			//LCD km/h ǥ��
			present_state = 1;//�ý��� on
		}

		/********�õ� �Ϸ� ����*******/
		if(present_state){ 
			OCR0 = duty; //duty���� ���� ����ġ ���ͷ�Ʈ
			lcd_seg_velocity(duty); //LCD�� Segment�� ���� �ӵ��� RPM�� ����

			LcdMove(1,4);
			LcdPuts(present_velocity); //���� �ӵ� ǥ��
			//LCD �ӵ� ǥ��

			/**********������ �۵� ��*********/
			if(headlight_motion){
				if(on_off_test){ //on_off_test 0�� 1�� �����ư��� �ٲ�
					PORTB = PORTB | 0x80; // 1<<PB7 ��
					on_off_test=0;
					LcdMove(0,15);
					LcdPutchar(0x20);
					//���� ���
				}
				else{
					PORTB = PORTB & 0x7F; // 0<<PB7 Ŵ
					on_off_test=1;
					LcdMove(0,15);
					LcdPutchar(0xD6);
					 //���� ���

			    }
				headlight_motion=0;
			}

			/**********�극��ũ �۵� ��*********/
			if(break_motion){
				emergency_on_off_test=0;
				
				LcdMove(0,6);
				LcdPuts(emergency_ment);
				// "<-  ->" ���

				PORTB=PORTB & 0x9F; // 0<<PB6 0<<PB5 Ŵ
				break_motion=0;
			}
			else{
				if(!emergency_on_off_test){
					LcdMove(0,6);
					LcdPutchar(0x20); // ���� ���
					LcdMove(0,9);
					LcdPutchar(0x20); // ���� ���
					PORTB=PORTB | 0x60;  // 1<<PB6 1<<PB5 ��
				}
			}

			/**********���� �۵� ��*********/
			if(emergency_motion){
				if(emergency_on_off_test){
					emergency_on_off_test=0;

					LcdMove(0,6);
					LcdPutchar(0x20); // ���� ���
					LcdMove(0,9);
					LcdPutchar(0x20); // ���� ���

					PORTB=PORTB | 0x60; // 1<<PB6 1<<PB5 ��
				}
				else{
					emergency_on_off_test=1;
				}
				emergency_motion=0;
			}

			/**********��ȸ�� �۵� ��*********/
			if(right_motion){
				
				emergency_on_off_test=0; //���� off

				LcdMove(0,6);
				LcdPutchar(0x20);
				LcdMove(0,9);
				LcdPutchar(0x20);
				 // ���� ��� "<-  ->" �����

				PORTB=PORTB | 0x40; // 1<<PB6 ��

				original_velocity=duty; //���� �ӵ� ����
				reduced_velocity=duty*0.8; //80% ��ŭ ���ӵ� �ӵ� ����

				/*------ ��ȸ��� �ϱ� �� ���� ------*/
				while(duty>=reduced_velocity){ //���ӵ� �ӵ� ��ŭ duty�� ����
					OCR0 = duty; //duty���� ���� ����ġ ���ͷ�Ʈ
					lcd_seg_velocity(duty); //LCD�� Segment�� ���� �ӵ��� RPM�� ����
					
					LcdMove(1,4);
					LcdPuts(present_velocity);
					//���� �ӵ� ���
					msec_delay(20);
					duty--;
				}
				duty=reduced_velocity;

				/*------ ��ȸ� �� ------*/
				for (int i=0; i<5; i++)
				{
					LcdMove(0,9);
					LcdPutchar(0x7E);
					//"->" ���
					PORTB = PORTB & 0xBF;// 0<<PB6 Ŵ
					msec_delay(1000);

					LcdMove(0,9);
					LcdPutchar(0x20);
					// ���� ���
					PORTB = PORTB | 0x40;// 1<<PB6 ��
					msec_delay(1000);
				}

				/*------ ��ȸ��� �� �� �����ӵ��� ���� ------*/
				while(duty<=original_velocity){//���� �ӵ� ���� duty�� ����
					OCR0 = duty; //duty���� ���� ����ġ ���ͷ�Ʈ
					lcd_seg_velocity(duty); //LCD�� Segment�� ���� �ӵ��� RPM�� ����
					
					LcdMove(1,4);
					LcdPuts(present_velocity);
					//���� �ӵ� ���
					msec_delay(20);
					duty++;
				}
				duty=original_velocity;

				right_motion=0;
			}

			/**********��ȸ�� �۵� ��*********/
			if(left_motion){
				emergency_on_off_test=0;
				
				LcdMove(0,6);
				LcdPutchar(0x20);
				LcdMove(0,9);
				LcdPutchar(0x20);
				// ���� ��� "<-  ->" �����

				PORTB=PORTB | 0x20; // 1<<PB5 ��
				
				original_velocity=duty; //���� �ӵ� ����
				reduced_velocity=duty*0.8; //80% ��ŭ ���ӵ� �ӵ� ����

				/*------ ��ȸ��� �ϱ� �� ���� ------*/
				while(duty>=reduced_velocity){ //���ӵ� �ӵ� ��ŭ duty�� ����
					OCR0 = duty; //duty���� ���� ����ġ ���ͷ�Ʈ
					lcd_seg_velocity(duty); //LCD�� Segment�� ���� �ӵ��� RPM�� ����
					
					LcdMove(1,4);
					LcdPuts(present_velocity);
					//���� �ӵ� ���
					msec_delay(20);
					duty--;
				}
				duty=reduced_velocity;

				/*------ ��ȸ� �� ------*/
				for (int i=0; i<5; i++)
				{
					LcdMove(0,6);
					LcdPutchar(0x7F);
					//"<-" ���
					PORTB = PORTB & 0xDF; // 0<<PB5 Ŵ
					msec_delay(1000);

					LcdMove(0,6);
					LcdPutchar(0x20);
					// ���� ���
					PORTB = PORTB | 0x20; // 1<<PB5 ��
					msec_delay(1000);
				}

				/*------ ��ȸ��� �� �� �����ӵ��� ���� ------*/
				while(duty<=original_velocity){//���� �ӵ� ���� duty�� ����
					OCR0 = duty; //duty���� ���� ����ġ ���ͷ�Ʈ
					lcd_seg_velocity(duty); //LCD�� Segment�� ���� �ӵ��� RPM�� ����
					
					LcdMove(1,4);
					LcdPuts(present_velocity);
					//���� �ӵ� ���
					msec_delay(20);
					duty++;
				}
				duty=original_velocity;

				left_motion = 0;
			}

		}	

		/********�õ��� ������ �� SW3Ű�� ���� �� ����********/
		while(!present_state){
			
			 key= KeyInput(); //�Էµ� key ����
			switch(key){
				case SW3: //�õ�on
					power=1;
					break;
				default:
					break;
			}
			if(power) break;
		}

	}

}

/*************lcd�� segment ���� ǥ�� �Լ� ����************/
void lcd_seg_velocity(short value){
	unsigned short number = 0;
	value/=2;
	if(value<=7) value=0;
	//value���� 7���ϸ� �׳� 0���� ���(10�� �ǵ� ���Ͱ� ���� ����)
	
	ISeg7DispNum(70*value,10); //Segment rpm�� ����
	//�Ϲ������� ���ָ� �¿����� ��� 8~9000rpm���� ����
	
	while(1){
		present_velocity[2-number]='0'+value%10;
		//�Էµ� ������ �� �ڸ����� �ݴ�� �迭�� ����
		value/=10;
		if(!value){ 
			for (int i=number+1; i<=2; i++) //���� ������ �ڸ��� 0�� �Է�����
			{
				present_velocity[2-i]='0';
			}
			break; //value���� 0 �̶�� �ݺ����� ����
		}
		number++;
	}
}
	
/*******************������ �Լ� ����********************/
void msec_delay(int n)
{
	for(; n>0; n--)
	_delay_ms(1);
}

/************Segment RPM�� ���� �Լ� ����***************/
void ISeg7DispNum(unsigned short num, unsigned short radix)
{
	int j;

	TIMSK &= ~(1<<TOIE2); //Ÿ�̸�2 �����÷ο� ���ͷ�Ʈ ����
	for (j = 1; j < N_BUF; j++) cnumber[j] =16;

	j=0;
	do{ // �Էµ� ���ڸ� cnumber�� ����
		cnumber[j++]=num%radix;
		num/=radix;
	}while(num); //num�� 0�̸� while���� ����


	TIMSK |= (1<<TOIE2); //Ÿ�̸�2 �����÷ο� ���ͷ�Ʈ ���
}