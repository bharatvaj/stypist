/* See LICENSE file for copyright and license details. */

/* colors */
enum {
	Reset,
	Hint,
	HintSpecial,
	Wrong,
	Correct
};

static const char* colors[][3] = {
	/* */
	 [Reset] = "\033[00m",
	 [Hint] = "\033[00m",
	 [HintSpecial] = "\033[35m",
	 [Wrong] = "\033[37;41m",
	 [Correct] = "\033[37;36m" // FIXME use green fg
};


static void go_up() {
	// TODO not working
	fprintf(stderr, "\033[A"); 
}

/* appearance */
#define special_count 4
static const char* specials[special_count * 2] = {
	/* from    to*/
	" ",        "_",
	"\n",       "$\n",
	"\r",       "$",
	"	",      ">"
};

/* functionality */
#define STYPIST_BUFFER_LEN 1024

/* keys */
static int backspace_key = 127;
