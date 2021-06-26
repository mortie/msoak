#include <pty.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>
#include <signal.h>

static void color(int depth, FILE *out1, FILE *out2) {
	static char *colors[] = {
		"\x1b[31m",
		"\x1b[32m",
		"\x1b[33m",
		"\x1b[34;1m",
		"\x1b[35m",
		"\x1b[36m",
	};

	if (depth == 0) {
		fprintf(out1, "\x1b[39m");
		fprintf(out2, "\x1b[39m");
	} else {
		char *color = colors[depth % (sizeof(colors) / sizeof(*colors))];
		fprintf(out1, "%s", color);
		fprintf(out2, "%s", color);
	}
}

static void process(char c, FILE *out1, FILE *out2) {
	static int depth = 0;
	static int prev_was_cr = 0;

	if (c == '\n') {
		color(depth = 0, out1, out2);
		putc(c, out1);

		// If the previous character was a cr, we already gave less
		// a newline. Don't give it another one.
		if (!prev_was_cr)
			putc(c, out2);
	} else if (c == '<') {
		color(++depth, out1, out2);
		putc(c, out1);
		putc(c, out2);
	} else if (c == '>' && depth > 0) {
		putc(c, out1);
		putc(c, out2);
		color(--depth, out1, out2);
	} else if (c == '\r') {
		putc(c, out1);

		// Less doesn't really handle carriage return well
		putc('\n', out2);
	} else {
		putc(c, out1);
		putc(c, out2);
	}

	if (c == '\r')
		prev_was_cr = 1;
	else
		prev_was_cr = 0;
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Usage: %s <command...>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *pager = getenv("MSOAK_PAGER");
	if (pager == NULL)
		pager = "less --RAW-CONTROL-CHARS";

	FILE *output = stderr;
	FILE *outfile = tmpfile();
	if (outfile == NULL) {
		perror("tmpfile()");
		return EXIT_FAILURE;
	}

	int amaster;
	pid_t child = forkpty(&amaster, NULL, NULL, NULL);
	if (child < 0) {
		perror("forkpty()");
		return EXIT_FAILURE;
	}

	if (child == 0) { // Child
		if (execvp(argv[1], argv + 1) < 0) {
			perror("exec()");
			return EXIT_FAILURE;
		}
	}

	char buf[1024];
	while (1) {
		ssize_t len = read(amaster, buf, sizeof(buf));
		if (len < 0) {
			if (errno == EIO)
				break;

			kill(child, SIGKILL);
			perror("read");
			goto cleanup;
		}

		for (ssize_t i = 0; i < len; ++i)
			process(buf[i], output, outfile);
		fflush(output);
	}

	int stat;
cleanup:
	wait(&stat);

	int errored =
		(WIFEXITED(stat) && WEXITSTATUS(stat) != EXIT_SUCCESS) ||
		WIFSIGNALED(stat);

	// Only show pager if we errored
	if (errored) {
		fflush(outfile);
		rewind(outfile);
		FILE *proc = popen(pager, "w");
		while (1) {
			ssize_t len = fread(buf, 1, sizeof(buf), outfile);
			if (len < 0) {
				perror("read");
				return EXIT_FAILURE;
			} else if (len == 0) {
				break;
			}

			fwrite(buf, 1, len, proc);
		}

		pclose(proc);
		wait(NULL);
	}

	if (WIFEXITED(stat))
		return WEXITSTATUS(stat);
	else if (WIFSIGNALED(stat))
		return WTERMSIG(stat) + 128;
	else
		return EXIT_FAILURE;
}
