/*********************************************
 * Based on code by Guido Socher
 *
 * Chip type: Atmega168 or Atmega328 or Atmega644 with ENC28J60
 *********************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>

#include "tuxgraphics/ip_arp_udp_tcp.h"
#include "tuxgraphics/websrv_help_functions.h"
#include "tuxgraphics/enc28j60.h"
#include "tuxgraphics/timeout.h"
#include "tuxgraphics/net.h"
#include "tuxgraphics/dnslkup.h"

#include "DS1620.h"

// In this file you define the header used for Pachube authentication in this form:
// #define PACHUBE_HEADER PSTR( "x-pachubeapikey: Af0Ghij----YOUR_PACHUBE_API_KEY------HieW1A" )
#include "../pachube_key.h"

/**********************************
 *	   CONFIGURATION SECTION      *
 **********************************/

// Local MAC address. Must be unique on the local network.
static uint8_t mymac[6] = {0x6a,0x77,0x6a,0x10,0x00,0x29};	// MAC prefix is 'jwj'. Luckily, b2 in first byte (0x6a) is 1, marking this as a locally administered MAC

// Local IP address. Must be unique on the local network.
static uint8_t myip[4] = {10,0,1,99};	// Local gateway DHCP range is 10.0.1.100 - 10.0.1.240 so this address is not in the DHCP range.

// Gateway address for the local network
static uint8_t gwip[4] = {10,0,1,1};

// The name of the virtual host which you want to 
#define WEBSERVER_VHOST "api.pachube.com"


/**********************************
 *	END OF CONFIGURATION SECTION  *
 **********************************/

#define TRANS_NUM_GWMAC 1				// Transaction number for 'lookup gateway MAC' (only MAC lookup we use as it happens)
static uint8_t gwmac[6]; 				// Global variable for gateway MAC address
static uint8_t otherside_www_ip[4]; 	// Remote IP address – will be filled by dnslkup

// State values for DNS lookup
typedef enum 
{
	dns_state_idle,
	dns_state_requestSent,
	dns_state_haveAnswer
} dns_state_enum;
static dns_state_enum dns_state=dns_state_idle;

#define BUFFER_SIZE 650
static uint8_t buf[BUFFER_SIZE+1];
static uint8_t pingsrcip[4];
static uint8_t start_web_client=0;
static uint8_t web_client_attempts=0;
static volatile uint8_t sec=0;
static volatile uint8_t cnt2step=0;

static int8_t gw_arp_state=0;

/* Read the internal temperature sensor using ADC.
 * The temperature is returned as °C * 1000.
 * Note that the temperature is *not* calibrated...
 */
int readTemp()
{
	int temp;
	
	ADCSRA |= (1<<ADSC);			// Start single conversion
	while( ADCSRA & (1<<ADSC) );	// Wait until conversion is done
	temp = ADC;						// Get ADC value and convert to temperature
	return temp;
}

/* setup timer T2 as an interrupt generating time base.
* You must call once sei() in the main program */
void init_cnt2(void)
{
	cnt2step=0;
	PRR &= ~(1<<PRTIM2);	// write power reduction register to zero
	TIMSK2 = (1<<OCIE2A);	// compare match on OCR2A
	TCNT2 = 0;				// init counter
	OCR2A = 156;			// value to compare against
	TCCR2A = (1<<WGM21);	// do not change any output pin, clear at compare match
	
	// divide clock by 1024: 8MHz/1024=12207 Hz
	TCCR2B = (1<<CS22)|(1<<CS21)|(1<<CS20); // clock divider, start counter

	/* Prescaler and OCR is calculated like this:
	 * Frequency (f) is 1/time required (T)
	 * T is time per tick (TPT) * ticks, so
	 * ticks is T / TPT per ticks
	 * TPT is 1 / (Clk / prescaler) => prescaler/Clk, so
	 * ticks is T / (prescaler/clk) => T * clk / prescaler
	 *
	 * For a frequency of 50 Hz (T=0.02) we try the different prescalers
	 * and find that only 1024 will yeild a value for ticks that can fit in 8 bit
	 * ticks = 0.02 * 8000000 / 1024 = 156.25
	 * 
	 * so OCR2A = 156
	 */
	 
}

// called when TCNT2==OCR2A
// that is in 50Hz intervals
ISR( TIMER2_COMPA_vect )
{
	cnt2step++;
	if( cnt2step >= 50 )
	{
		cnt2step=0;
		sec++; // stepped every second
	}
}

// we were ping-ed by somebody, store the ip of the ping sender
// and trigger an upload to http://tuxgraphics.org/cgi-bin/upld
// This is just for testing and demonstration purpose
void ping_callback(uint8_t *ip)
{
	uint8_t i=0;
	// trigger only first time in case we get many ping in a row:
	if (start_web_client==0)
	{
		PORTB |= (1<< PB0);
		_delay_ms( 200 );
		PORTB &= ~(1<< PB0);
		_delay_ms( 200 );

		start_web_client=1;
		// save IP from where the ping came:
		while(i<4){
			pingsrcip[i]=ip[i];
			i++;
		}
	}
}

// the __attribute__((unused)) is a gcc compiler directive to avoid warnings about unsed variables.
void browserresult_callback(uint8_t webstatuscode,uint16_t datapos __attribute__((unused)), uint16_t len __attribute__((unused))){
    int i;
	for( i=0; i<webstatuscode; i++ )
	{
		PORTB |= (1<< PB0);
		_delay_ms( 100 );
		PORTB &= ~(1<< PB0);
		_delay_ms( 50 );
	}

	if( webstatuscode==2 )
	{
		// Request sent ok: clear timer and set mode to "waiting"
		sec = 0;
		start_web_client = 3;
	}
	else
		start_web_client = 0;	// "stopped"
}

// the __attribute__((unused)) is a gcc compiler directive to avoid warnings about unsed variables.
void arpresolver_result_callback(uint8_t *ip __attribute__((unused)),uint8_t transaction_number,uint8_t *mac)
{
	uint8_t i=0;
	if (transaction_number==TRANS_NUM_GWMAC)
	{
		// copy mac address over:
		while(i<6){gwmac[i]=mac[i];i++;}
	}
}

int main(void)
{
	uint16_t dat_p,plen;
	char postdata[30]; 
	
	// Set up analog to digital converter
	// By default set to use AREF and single-conversion mode
	ADCSRA |= (1<< ADPS2) | (1<< ADPS1);				// Set prescaler 64 for 125 kHz @ 8 MHz system clock	
	ADCSRA |= (1<< ADEN);								// Enable ADC
	ADMUX = (1<< REFS1) | (1<< REFS0) | (1<< MUX3);		// Select internal 1.1V voltage reference and ADMUX channel 8 for internal temperature sensor
	
	/* Initialize enc28j60 */
	enc28j60Init( mymac );
//	enc28j60clkout( 2 );	// change clkout from 6.25MHz to 12.5MHz
	_delay_loop_1( 0 );		// 60us
	
	init_cnt2();
	sei();
	
	/* Magjack leds configuration, see enc28j60 datasheet, page 11 */
	// LEDB=yellow LEDA=green
	//
	// 0x476 is PHLCON LEDA=links status, LEDB=receive/transmit
	// enc28j60PhyWrite(PHLCON,0b0000 0100 0111 01 10);
	enc28j60PhyWrite( PHLCON, 0x476 );
	
	DDRB |= (1<<DDB1) | (1<< PB0); // LED, enable PB1, LED as output

	// Init IP interface
//	init_udp_or_www_server(mymac,myip);
//	www_server_port(MYWWWPORT);
	client_ifconfig( myip, NULL );	// Set IP (not specifying netmask, which is only used for the route_via_gw() function anyway)
	init_mac( mymac );				// Initialize MAC

	// register to be informed about incomming ping:
	register_ping_rec_callback( &ping_callback );
	
	while( 1 )
	{
		// handle ping and wait for a tcp packet
		plen = enc28j60PacketReceive( BUFFER_SIZE, buf );
		dat_p = packetloop_arp_icmp_tcp( buf,plen );
		if(plen==0)
		{
			// we are idle here trigger arp and dns stuff here
			if( gw_arp_state == 0 )
			{
				// Lookup MAC address of the gateway
				get_mac_with_arp( gwip, TRANS_NUM_GWMAC, &arpresolver_result_callback );
				gw_arp_state=1;

				PORTB |= (1<< PB0);
				_delay_ms( 100 );
				PORTB &= ~(1<< PB0);
				_delay_ms( 200 );
			}
			if( get_mac_with_arp_wait() == 0 && gw_arp_state == 1)
			{
				// done we have the mac address of the GW
				gw_arp_state=2;
				
				PORTB |= (1<< PB0);
				_delay_ms( 200 );
				PORTB &= ~(1<< PB0);
				_delay_ms( 200 );
			}
			if( dns_state == dns_state_idle && gw_arp_state == 2)
			{
				// Make sure link is up before trying DSN
				if( !enc28j60linkup() )
					continue;
				
				sec = 0;	// Reset timeout counter
				dns_state = dns_state_requestSent;
				dnslkup_request( buf, WEBSERVER_VHOST, gwmac );	 // Lookup remote host
				
				PORTB |= (1<< PB0);
				_delay_ms( 300 );
				PORTB &= ~(1<< PB0);
				_delay_ms( 200 );

				continue;
			}
			if( dns_state == dns_state_requestSent && dnslkup_haveanswer() )
			{
				dns_state = dns_state_haveAnswer;
				dnslkup_get_ip( otherside_www_ip );
				
				PORTB |= (1<< PB0);
				_delay_ms( 400 );
				PORTB &= ~(1<< PB0);
				_delay_ms( 200 );
				
				start_web_client = 1;	// Ready to send
			}
			if( dns_state != dns_state_haveAnswer )
			{
				// retry every minute if dns-lookup failed:
				if (sec > 60){
					dns_state=0;
				}
				// don't try to use web client before
				// we have a result of dns-lookup
				continue;
			}
			//----------
			if( start_web_client == 1 )
			{
				PORTB |= (1<< PB0);
				_delay_ms( 500 );
				PORTB &= ~(1<< PB0);
				_delay_ms( 200 );

				sec=0;					// Reset counter
				start_web_client=2;		// Web client state = request sent
				web_client_attempts++;
				
				// Read temperature and format into string
//				sprintf_P( postdata, PSTR( "temp,%d" ), readTemp() );
				sprintf_P( postdata, PSTR( "temp,%.1f" ), readTempFrom1620() );
				// Send request
				client_http_put( PSTR( "/v2/feeds/47970.csv" ), NULL, WEBSERVER_VHOST, PACHUBE_HEADER, postdata, &browserresult_callback, otherside_www_ip, gwmac );
				
				// Test value with this URL
				// https://api.pachube.com/v2/feeds/47970/datastreams/tst.png?width=700&height=300&colour=%23f15a24&duration=30minutes&show_axis_labels=true&detailed_grid=true&scale=auto&timezone=Copenhagen
			}
			// reset after a delay to prevent permanent bouncing
			if( sec > 60 && start_web_client==2 )
			{
				start_web_client = 0;	// "Stopped"
			}
			
			// If enough time has passed in "waiting" mode, go to "ready to send"
			if( sec >= 10 && start_web_client == 3 )
			{
				start_web_client = 1;		// "Ready to send"
			}
			continue;
		}	// end-if (plen == 0)
		
		if(dat_p==0)
		{ // plen!=0
			// check for incomming messages not processed
			// as part of packetloop_arp_icmp_tcp, e.g udp messages
			udp_client_check_for_dns_answer(buf,plen);
			continue;
		}
	}
	return (0);
}
