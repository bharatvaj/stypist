#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "config.h"


static int enable_raw_mode = 0;

static struct termios og_term;
static struct termios term;
static FILE* stdinf = NULL;
static int stdinfd = -1;

// per character time
static time_t sch_time;
static time_t ech_time;
static time_t dch_time;
// Holds the average of time taken
static double wpm = 0;
static double reference_text_length = 0;
static double input_error_count = 0;
// last input character is an error character
static int input_ignore_character = 0;
// 0=no_error, 1=error
static uint8_t* input_error_indices;
static size_t input_error_indices_len;
static double accuracy = 0;

static void debug(const char* msg) {
#ifdef ENABLE_DEBUG
		fprintf(stderr, "%s\n", msg);
#endif
}

// Evaluate the input characters typed by the user
static void evaluate(int sig) {
		if (reference_text_length == 0) {
			return;
		}
		printf("\n\n");
		accuracy = 1 - (input_error_count / reference_text_length);
		printf("input_error_count: %f\n", input_error_count);
		printf("reference_text_length: %f\n", reference_text_length);
		printf("%fwpm %f%% accuracy\n", wpm, accuracy * 100);
		printf("\n");
}

void handle_interrupt(int signum) {
	evaluate(signum);
	tcsetattr(stdinfd, TCSANOW, &og_term);
	exit(signum);
}

static double show_hint(const char* text_buffer, size_t text_buffer_len) {
	char* hint_string = (char*) malloc(text_buffer_len * sizeof(char));
	size_t hint_string_len = text_buffer_len;
	memcpy(hint_string, text_buffer, text_buffer_len);
	if (hint_string[hint_string_len - 1] == '\n') {
			hint_string[--hint_string_len] = '$';
	}
	return fprintf(stdout, STYPIST_HINT "%s\n" STYPIST_HINT_RESET, hint_string);
	free(hint_string);
}

/**
 * Compare a reference text with character input
 * Update the input time for the reference text in an time_taken
 * Returns the time taken for the user to complete the text
 * Returns -1 on interrupt
 */
static double line_test(char* text_buffer, size_t text_buffer_len) {
	char* key_buffer = NULL;
	size_t key_buffer_len = 0;
	int n = -1;
	// input character
	int ic = -25;
	stdinf = fopen("/dev/tty", "r");
	stdinfd = fileno(stdinf);
	tcgetattr(stdinfd, &og_term);
	memcpy(&term, &og_term, sizeof(struct termios));
	// disable immediate printing
	term.c_lflag = (~ECHO & ISIG & VEOF & VINTR);
	tcsetattr(stdinfd, TCSANOW, &term);
	show_hint(text_buffer, text_buffer_len);
	if (enable_raw_mode != 1) {
		text_buffer[--text_buffer_len] = '\0';
	}
	// TODO optimize for size, text_buffer_len
	// size is most often not needed
	// also this can be combined with input_error_count
	input_error_indices_len = text_buffer_len;
	input_error_count = 0;
	input_error_indices = (uint8_t*)malloc(input_error_indices_len * sizeof(uint8_t));
	if (input_error_indices == NULL) {
			debug("Not able to allocate memory for input_error_indices");
	}
	// assume '\n' at end
	reference_text_length = text_buffer_len;
	// read input from user until we hit EOF
	// or the user input matches the reference input
	while(key_buffer_len < text_buffer_len) {
		// reference character
		char rc = text_buffer[key_buffer_len];
		// ignore '\n', '\r' when enable_raw_mode is disabled
		/* if(enable_raw_mode != 1 && (rc == '\n' || rc == '\r')) { */
		/* 	continue; */
		/* } */
		sch_time = time(NULL);
		ic = fgetc(stdinf);
		ech_time = time(NULL);
		dch_time = difftime(ech_time, sch_time);
		wpm += dch_time;
		wpm /= 2;
		if(ic == 127 && key_buffer_len > 0) {
			printf("\b \b");
			if (input_error_indices[--key_buffer_len] == 1) {
					input_error_count--;
			}
			continue;
		}
		if (ic == 003 || ic == 004) {
				// handle ctrl-c and ctrl-d
				evaluate(-1);
				return -1;
		} else if(ic == rc) {
			// handle correct input
			fprintf(stdout, STYPIST_CORRECT_INPUT "%c" STYPIST_CORRECT_INPUT_RESET, ic);
			input_error_indices[key_buffer_len] = 0;
		} else {
			// handle wrong inputs
			if(ic == '\n') {
				fprintf(stdout, STYPIST_WRONG_INPUT "$" STYPIST_WRONG_INPUT_RESET);
			} else {
				fprintf(stdout, STYPIST_WRONG_INPUT "%c" STYPIST_WRONG_INPUT_RESET, ic);
			}
		    input_error_count++;
			input_error_indices[key_buffer_len] = 1;
		}
		key_buffer_len++;
	}
	free(input_error_indices);
	input_error_indices_len = -1;
	debug("End of evaluation");
	evaluate(1);
	return key_buffer_len;
}

int main(int argc, char* argv[]) {
	/// @todo Use a constant
	/* signal(SIGSEGV, handle_interrupt); */
	signal(SIGINT, handle_interrupt);
	size_t max_buf_len = STYPIST_BUFFER_SIZE;
	char** text_buffers = (char**)malloc(max_buf_len * sizeof(char*));
	size_t* text_buffer_lens = (size_t*)malloc(max_buf_len * sizeof(size_t));
	size_t text_buffer_lines = 0;
	size_t n = 0;
	char* input_buffer = NULL;
	size_t input_buffer_len = 0;

	// Read till stdin is exhausted
	// set stdin to unbuffered mode
	FILE* input_file = stdin;
	if (input_file == NULL) {
		fprintf(stderr, "failed to open input_file\n");
		return -1;
	}
	while((n = getline(&input_buffer , &input_buffer_len, input_file)) != -1) {
		text_buffers[text_buffer_lines] = input_buffer;
		text_buffer_lens[text_buffer_lines] = n;
		// prevents invalidation of the already allocated buffer
		input_buffer = NULL;
		text_buffer_lines++;
		if(text_buffer_lines == max_buf_len) {
			/// @todo Use a constant
			max_buf_len += STYPIST_BUFFER_SIZE;
			text_buffers = (char**)realloc((void*)text_buffers, max_buf_len * sizeof(char*));
			text_buffer_lens = (size_t*)realloc((void*)text_buffer_lens, max_buf_len * sizeof(size_t));
		}
	}
	for (int i = 0; i < text_buffer_lines; i++) {
		char* text_buffer = text_buffers[i];
		size_t text_buffer_len = text_buffer_lens[i];
		if (line_test(text_buffer, text_buffer_len) == -1) {
				break;
		}
	}
	free(text_buffers);
	tcsetattr(stdinfd, TCSANOW, &og_term);
	return 0;
}

