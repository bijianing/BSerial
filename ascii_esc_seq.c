/*
	Skipt CSI (Control Sequence Introducer) key sequence
	reference: 
		https://en.wikipedia.org/wiki/ANSI_escape_code#CSI_sequences
		For CSI, or "Control Sequence Introducer" commands, the ESC [ is followed by 
		any number (including none) of "parameter bytes" in the range 0x30–0x3F (ASCII 0–9:;<=>?), 
		then by any number of "intermediate bytes" in the range 0x20–0x2F (ASCII space and !"#$%&'()*+,-./), 
		then finally by a single "final byte" in the range 0x40–0x7E (ASCII @A–Z[\]^_`a–z{|}~).
*/



#include <stdio.h>
#include <string.h>


#define ESC		27

/* parameter bytes judgement */
int is_param_byte(char c)
{
	if (c >= 0x30 && c <= 0x3F)
		return 1;

	return 0;
}

/* intermediate bytes judgement */
int is_interm_byte(char c)
{
	if (c >= 0x20 && c <= 0x2F)
		return 1;

	return 0;
}

/* intermediate bytes judgement */
int is_final_byte(char c)
{
	if (c >= 0x40 && c <= 0x7F)
		return 1;

	return 0;
}

char* skip_csi(char* buf_in, char* buf_out, int size)
{
	int i, j, pi = 0, po = 0;
	char* ret = buf_in;

	for (i = 0; i < size; i++) {
		if (buf_in[i] == ESC && buf_in[i + 1] == '[') {

			j = i + 2;
			/* skip parameter bytes */
			while (is_param_byte(buf_in[j]) && j < size) j++;

			/* skip intermediate bytes */
			while (is_interm_byte(buf_in[j]) && j < size) j++;

			/* found CSI key */
			if (is_final_byte(buf_in[j]) && j < size) {
				memcpy(buf_out + po, buf_in + pi, i - pi);
				pi = j + 1;
				po += (i - pi);
			}
		}
	}

	if (pi) {
		memcpy(buf_out + po, buf_in + pi, i - pi);
		ret = buf_out;
	}

	return ret;
}


void main(void)
{
	char buf[1024];
	char buf_out[1024];
	char* p;

	while (gets(buf)) {


	}
}