/*
 * main.c
 *
 * Created: 05/03/2019 18:00:58
 *  Author: eduardo
 */ 

#include <asf.h>
#include "tfont.h"
#include "sourcecodepro_28.h"
#include "calibri_36.h"
#include "arial_72.h"
#include "math.h"

//Definindo horario
#define YEAR        2019
#define MOUNTH      4
#define DAY         8
#define WEEK        12
#define HOUR        15
#define MINUTE      45
#define SECOND      0

#define PI 3.14

//Definindo botao Oled
#define EBUT1_PIO PIOD //start EXT 9 PD28
#define EBUT1_PIO_ID 16
#define EBUT1_PIO_IDX 28
#define EBUT1_PIO_IDX_MASK (1u << EBUT1_PIO_IDX)

#define BUT_PIO      PIOA
#define BUT_PIO_ID   ID_PIOA
#define BUT_IDX  11
#define BUT_IDX_MASK (1 << BUT_IDX)

// Flags para velocidade inst, tempo total, distancia total variaveis globais

volatile Bool f_rtt_alarme = false;
volatile Bool but_flag;
volatile Bool but_start = false;
volatile int pulsos;
volatile int dT;
volatile int omega;
volatile int velocidade;
volatile int distanciaTotal;
	

void pin_toggle(Pio *pio, uint32_t mask);
void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);


void but_flag_callback(void){
	but_flag = true;
}
void but_start_callback(void){
	but_start = true;
}

void io_init(void){
	
	board_init();
	sysclk_init();
	
	//Configura botao oled
	pmc_enable_periph_clk(BUT_PIO_ID);
	//Configura botoes do oled como input
	pio_set_input(BUT_PIO,BUT_IDX_MASK,PIO_DEFAULT);
	pio_pull_up(BUT_PIO,BUT_IDX_MASK,PIO_PULLUP);
	//Configura a interrupcao
	pio_handler_set(EBUT1_PIO,
	BUT_PIO_ID,
	BUT_IDX,
	PIO_IT_FALL_EDGE,
	but_start);
	//Ativa a interrupcao
	pio_enable_interrupt(EBUT1_PIO, EBUT1_PIO_IDX_MASK);
	//Configura o NVIC para receber interrupcoes
	NVIC_EnableIRQ(BUT_PIO_ID);
	NVIC_SetPriority(BUT_PIO_ID, 0);
	//delay_init()
	//WDT -> WDT_MR_WDDIS;
}

	void TC_init(Tc * TC, int ID_TC, int TC_CHANNEL, int freq){
	uint32_t ul_div;
	uint32_t ul_tcclks;
	uint32_t ul_sysclk = sysclk_get_cpu_hz();

	uint32_t channel = 1;

	pmc_enable_periph_clk(ID_TC);

	/** Configura o TC para operar em  4hz e interrup?c?o no RC compare */
	tc_find_mck_divisor(freq, ul_sysclk, &ul_div, &ul_tcclks, ul_sysclk);
	tc_init(TC, TC_CHANNEL, ul_tcclks | TC_CMR_CPCTRG);
	tc_write_rc(TC, TC_CHANNEL, (ul_sysclk / ul_div) / freq);

	/* Configura e ativa interrup?c?o no TC canal 0 */
	/* Interrup??o no C */
	NVIC_EnableIRQ((IRQn_Type) ID_TC);
	tc_enable_interrupt(TC, TC_CHANNEL, TC_IER_CPCS);

	/* Inicializa o canal 0 do TC */
	tc_start(TC, TC_CHANNEL);
}
void pin_toggle(Pio *pio, uint32_t mask){
	if(pio_get_output_data_status(pio, mask))
	pio_clear(pio, mask);
	else
	pio_set(pio,mask);
}
volatile uint8_t flag_start = 1;

// Interrupcoes
void TC1_Handler(void){
	volatile uint32_t ul_dummy;

	/****************************************************************
	* Devemos indicar ao TC que a interrup??o foi satisfeita.
	******************************************************************/
	ul_dummy = tc_get_status(TC0, 1);

	/* Avoid compiler warning */
	UNUSED(ul_dummy);

	
}

void RTT_Handler(void)
{
	uint32_t ul_status;

	/* Get RTT status */
	ul_status = rtt_get_status(RTT);

	/* IRQ due to Time has changed */
	if ((ul_status & RTT_SR_RTTINC) == RTT_SR_RTTINC) {  }

	/* IRQ due to Alarm */
	if(but_start == 1){
	if ((ul_status & RTT_SR_ALMS) == RTT_SR_ALMS) {
		 //tc_start(Tc * TC, int ID_TC, int TC_CHANNEL, int freq);
		 f_rtt_alarme = true;
		 pulsos += 1;
	}
	}
}

struct ili9488_opt_t g_ili9488_display_opt;

void configure_lcd(void){
	/* Initialize display parameter */
	g_ili9488_display_opt.ul_width = ILI9488_LCD_WIDTH;
	g_ili9488_display_opt.ul_height = ILI9488_LCD_HEIGHT;
	g_ili9488_display_opt.foreground_color = COLOR_CONVERT(COLOR_WHITE);
	g_ili9488_display_opt.background_color = COLOR_CONVERT(COLOR_WHITE);

	/* Initialize LCD */
	ili9488_init(&g_ili9488_display_opt);
	ili9488_draw_filled_rectangle(0, 0, ILI9488_LCD_WIDTH-1, ILI9488_LCD_HEIGHT-1);
	
}


void font_draw_text(tFont *font, const char *text, int x, int y, int spacing) {
	char *p = text;
	while(*p != NULL) {
		char letter = *p;
		int letter_offset = letter - font->start_char;
		if(letter <= font->end_char) {
			tChar *current_char = font->chars + letter_offset;
			ili9488_draw_pixmap(x, y, current_char->image->width, current_char->image->height, current_char->image->data);
			x += current_char->image->width + spacing;
		}
		p++;
	}	
}


int main(void) {
	board_init();
	sysclk_init();	
	configure_lcd();
	
	while(1) {
		if (f_rtt_alarme){
      
      /*
       * O clock base do RTT ? 32678Hz
       * Para gerar outra base de tempo ? necess?rio
       * usar o PLL pre scale, que divide o clock base.
       *
       * Nesse exemplo, estamos operando com um clock base
       * de pllPreScale = 32768/32768/2 = 2Hz
       *
       * Quanto maior a frequ?ncia maior a resolu??o, por?m
       * menor o tempo m?ximo que conseguimos contar.
       *
       * Podemos configurar uma IRQ para acontecer quando 
       * o contador do RTT atingir um determinado valor
       * aqui usamos o irqRTTvalue para isso.
       * 
       * Nesse exemplo o irqRTTvalue = 8, causando uma
       * interrup??o a cada 2 segundos (lembre que usamos o 
       * pllPreScale, cada incremento do RTT leva 500ms (2Hz).
       */
      uint16_t pllPreScale = (int) (((float) 32768) / 2.0);
      uint32_t irqRTTvalue  = 4;
	  // Quando o botao for apertado
	  
	  char velocidadeBuffer[32];
	  char distanciaTotalBuffer[32];
	  char tempoTrechoTotalBuffer[32];
	  char omegaBuffer[32];
 
		  
		  int (omega)          = 2 * PI * pulsos/dT;  
		  int (velocidade)     = omega * 0.650;
		  int (distanciaTotal) = 2 * PI * 0.650 * pulsos;
		  
		  sprintf(omegaBuffer,"%d",omega);
		  sprintf(velocidadeBuffer,"%d",velocidade);
		  sprintf(distanciaTotalBuffer,"%d",distanciaTotal);
		  
		  font_draw_text(&omega,omega, 50, 100, 1);
		  font_draw_text(&velocidade, velocidade, 50, 100, 1);
		  font_draw_text(&distanciaTotal,distanciaTotal, 50, 100, 2); 			  
		  

      // reinicia RTT para gerar um novo IRQ
      //RTT_init(pllPreScale, irqRTTvalue);       
      
     /*
      * caso queira ler o valor atual do RTT, basta usar a funcao
      *   rtt_read_timer_value()
      */
      /*
       * CLEAR FLAG
       */
      f_rtt_alarme = false;
		}
	}	
}