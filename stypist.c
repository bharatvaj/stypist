#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#define STYPIST_BUFFER_SIZE 10

// Evaluate the user's score
size_t evaluate() {
	printf("Thanks for taking the test\n");
	return 0;
}

size_t user_test(char* text_buffer, size_t* text_buffer_len) {
	char* key_buffer = NULL;
	size_t key_buffer_len = 0;
	int n = -1;
	char k;
	FILE* stdinfd = fopen("/dev/tty", "r");
	int fdfileno = fileno(stdinfd);
	struct termios t;
	tcgetattr(fdfileno, &t);
	t.c_lflag &= (~ICANON & ~ECHO);
	tcsetattr(fdfileno, TCSANOW, &t);
	setbuf(stdout, NULL);
	// show hint
	fprintf(stdout, "\n%s", text_buffer);
	while((n = fread(&k, 1, 1, stdinfd))) {
		// TODO use feof and ferror to distinguish errors and eof
		if(n == EOF) {
			// eof or error
			printf("End of evaluation");
			evaluate();
			break;
		}
		if(k == 127) {
			printf("\b \b");
			key_buffer_len--;
			continue;
		}
		if(k == '\n') {
			break;
		}
		key_buffer_len++;
		char t = text_buffer[key_buffer_len - 1];
		// FIXME this will fail if the user makes mistake, buffer overflow possibility, maintain a HEAD int that will maintain the cursor position.
		if(k == t) {
		/* setvbuf(stdout, NULL, _IONBF, 0); */
			fprintf(stdout, "\e[00m%c", k);
		} else {
			fprintf(stdout, "\e[00;31m%c", k);
			fprintf(stdout, "\e[00m");
		}
		if (key_buffer_len == *text_buffer_len) {
			break;
		}
	}
	if (key_buffer != NULL) {
		/* free(key_buffer); */
	}
	return key_buffer_len;
}

int main(int argc, char* argv[]) {
	/// @todo Use a constant
	size_t max_buf_len = STYPIST_BUFFER_SIZE;
	char** text_buffers = (char**)malloc(max_buf_len * sizeof(char*));
	size_t* text_buffer_lens = (size_t*)malloc(max_buf_len * sizeof(size_t));
	size_t text_buffer_lines = 0;
	size_t n = 0;
	char* input_buffer = NULL;
	size_t input_buffer_len = 0;

	// Read till stdin is exhausted
	while(1) {
		n = getline(&input_buffer , &input_buffer_len , stdin);
		if(n == -1) {
			// stdin is exhausted
			break;
		}
		text_buffers[text_buffer_lines] = input_buffer;
		text_buffer_lens[text_buffer_lines] = input_buffer_len;
		// prevents invalidation of the already allocated buffer
		input_buffer = NULL;
		text_buffer_lines++;
		/* printf("Another one: %s", text_buffers[text_buffer_lines -1]); */
		if(text_buffer_lines == max_buf_len) {
			/// @todo Use a constant
			max_buf_len += 10;
			text_buffers = (char**)realloc((void*)text_buffers, max_buf_len * sizeof(char*));
			text_buffer_lens = (size_t*)realloc((void*)text_buffer_lens, max_buf_len * sizeof(size_t));
		}
	}
	for (int i = 0; i < text_buffer_lines; i++) {
		char* text_buffer = text_buffers[i];
		size_t text_buffer_len = text_buffer_lens[i];
		user_test(text_buffer, &text_buffer_len);
	}
	/* free(text_buffer); */
	return 0;
}

