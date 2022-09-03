#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

FILE* dbug_file = NULL;

#define dbug(...)                        \
	if (dbug_file != NULL) {             \
		fprintf(dbug_file, __VA_ARGS__); \
		fflush(dbug_file);               \
	}

#define render(...) \
	fprintf(stdout, __VA_ARGS__); \
	fflush(stdout);

// terminal data
static struct termios og_term;
static struct termios term;
static FILE* stdinf = NULL;
static int stdinfd = -1;

// rendering data
int total_line = 0;
int current_line = 0;
int current_column = 0;
int spaces_typed = 0;
int head = 0;

// evaluation data
static time_t start_time;
static time_t tots;

#include "config.h"

char* ref_buffer = 0;
size_t ref_buffer_cap = STYPIST_BUFFER_LEN;
size_t ref_buffer_len = 0;

void restore_terminal() {
	render("%s", *colors[Reset]);
	fflush(stdout);
	tcsetattr(stdinfd, TCSANOW, &og_term);
}

void evaluate() {
	if (spaces_typed == 0) {
		spaces_typed = 1;
	}
	double time_in_mins = tots / 60.00;
	double wpm = (double)spaces_typed / time_in_mins;
	render("%s", *colors[Reset]);
	render("words_typed: %f\n", (float)spaces_typed);
	render("time_in_mins: %f\n", time_in_mins);
	render("wpm: %f\n", wpm);
}

void start_cursor_line() {
	// TODO go back to start
	for(int i = 0; i < total_line; i++) {
	go_up();
	}
	// have to go back `lines`
}

void end() {
	evaluate();
	restore_terminal();
	if (dbug_file != NULL) {
		fclose(dbug_file);
	}
	_Exit(0);
}

void handle_signal(int signum) {
	end();
}

void prepare_terminal() {
	// TODO try using freopen with stdin
	/* stdinf = freopen(NULL, "rw", stdin); */
	stdinf = fopen("/dev/tty", "rw");
	if (stdinf == NULL) {
		fprintf(stderr, "FATAL: Failed to open stdin for the second time\n");
		exit(EXIT_FAILURE);
	}
	stdinfd = fileno(stdinf);
	if (stdinfd == -1) {
		fprintf(stderr, "FATAL: Failed to get stdinfd of /dev/tty\n");
		exit(EXIT_FAILURE);
	}
	tcgetattr(stdinfd, &og_term);
	memcpy(&term, &og_term, sizeof(struct termios));
	// disable immediate printing
	term.c_lflag = (~ECHO & ISIG & VEOF & VINTR);
	tcsetattr(stdinfd, TCSANOW, &term);
}

void clear_hint() {
	// go back `lines`
	fputs(*colors[Reset], stdout);
}

void print_hint() {
	bool is_special = false;
	for (int ref_head = 0; ref_head < ref_buffer_len; ref_head++) {
		char ref_char = ref_buffer[ref_head];
		if (ref_char == '\n') {
			total_line += 1;
		}
		for (int special_head = 0; special_head < special_count; special_head += 2) {
			if (specials[special_head][0] == ref_char) {
				render("%s%s%s", *colors[HintSpecial], specials[special_head + 1]);
				is_special = true;
				break;
			}
		}
		if (is_special == false) {
			render("%s%c", *colors[Hint], ref_char);
		}
		is_special = false;
	}
}


int load_from_pipe() {
	ref_buffer = (char*)malloc(ref_buffer_cap);

	// Read till stdin is exhausted
	// set stdin to unbuffered mode
	FILE* input_file = stdin;
	if (input_file == NULL) {
		fprintf(stderr, "failed to open input_file\n");
		return -1;
	}
	size_t bytes_read = 0;
	while((bytes_read = fread(ref_buffer + ref_buffer_len, 1, STYPIST_BUFFER_LEN, input_file)) != 0) {
		ref_buffer_len += bytes_read;
		// we need one byte for '\0', so check a character before
		if (ref_buffer_len == ref_buffer_cap - 1) {
			ref_buffer_cap += STYPIST_BUFFER_LEN;
			ref_buffer = (char*)realloc(ref_buffer, STYPIST_BUFFER_LEN);
		}
		render("test\n");
	}
	ref_buffer[ref_buffer_len] = '\0';
	return 0;
}

int get_next_char() {
	int buffer = -1;
	fread(&buffer, 1, 1, stdinf);
	if (ferror(stdinf)) {
		fprintf(stderr, "ERror reading from stdinf\n");
		exit(EXIT_FAILURE);
	}
	if (feof(stdinf)) {
		buffer = -1;
		fprintf(stderr, "EOF is set\n");
	}
	return buffer;
}

void process_input() {
	for (head = 0; head < ref_buffer_len; head++, current_column++) {
		dbug("%d: %c: ", head, ref_buffer[head]);
		if (ref_buffer[head] == '\n') {
			current_line += 1;
			current_column = 0;
			fputs("\n", stdout);
			continue;
		}
		/* time current = get_current_time(); */
		start_time = time(NULL);
		char user_char = get_next_char();
		tots += difftime(time(NULL), start_time);
		dbug("%c\n", user_char);
		if (user_char == -1 || user_char == 003 || user_char == 004) {
			end();
		}

		if (user_char == backspace_key) {
			if (current_column > 0) {
				if (ref_buffer[head - 1] == ' ') {
					spaces_typed -= 1;
				}
				// already incremented in 'for' loop
				// so decrement it two times
				current_column -= 2;
				head -= 2;
				fputs("\b \b", stdout);
			}
			else if (current_column == 0) {
				head--;
				current_column--;
			}
			continue;
		}

		if (user_char == ' ') {
			spaces_typed += 1;
		}

		if (user_char < 45 && user_char > 45) {
			/* user_char =  */
		}

		if (user_char == ref_buffer[head]) {
			render("%s%c", *colors[Correct], user_char, *colors[Reset]);
		} else {
			render("%s%c%s", *colors[Wrong], user_char, *colors[Reset]);
		}
	}
}

void draw() {
	clear_hint();
	print_hint();
	process_input();
}

int main(int argc, const char* argv[]) {
	signal(SIGINT, handle_signal);
	if (load_from_pipe() == -1) {
		return -1;
	}
	prepare_terminal();
	start_cursor_line();
	draw();
	end();
	return 0;
}
