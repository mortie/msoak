#include <pty.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <math.h>

// Adapted from https://www.programmingalgorithms.com/algorithm/hsv-to-rgb?lang=C
static void hsv_to_rgb(
		double hsv_h, double hsv_s, double hsv_v,
		unsigned char *rgb_r, unsigned char *rgb_g, unsigned char *rgb_b) {
	double r = 0, g = 0, b = 0;

	if (hsv_s == 0) {
		r = hsv_v;
		g = hsv_v;
		b = hsv_v;
	} else {
		int i;
		double f, p, q, t;

		if (hsv_h == 360)
			hsv_h = 0;
		else
			hsv_h = hsv_h / 60;

		i = (int)trunc(hsv_h);
		f = hsv_h - i;

		p = hsv_v * (1.0 - hsv_s);
		q = hsv_v * (1.0 - (hsv_s * f));
		t = hsv_v * (1.0 - (hsv_s * (1.0 - f)));

		switch (i)
		{
		case 0:
			r = hsv_v;
			g = t;
			b = p;
			break;

		case 1:
			r = q;
			g = hsv_v;
			b = p;
			break;

		case 2:
			r = p;
			g = hsv_v;
			b = t;
			break;

		case 3:
			r = p;
			g = q;
			b = hsv_v;
			break;

		case 4:
			r = t;
			g = p;
			b = hsv_v;
			break;

		default:
			r = hsv_v;
			g = p;
			b = q;
			break;
		}
	}

	*rgb_r = r * 255;
	*rgb_g = g * 255;
	*rgb_b = b * 255;
}

static void color(int depth, FILE *out1, FILE *out2) {
	if (depth == 0) {
		fprintf(out1, "\x1b[0m");
		fprintf(out2, "\x1b[0m");
	} else {
		double h = ((double)depth - 0.5) * 60;
		double s = 0.76, v = 1;
		unsigned char r, g, b;
		hsv_to_rgb(h, s, v, &r, &g, &b);

		fprintf(out1, "\x1b[38;2;%i;%i;%im", r, g, b);
		fprintf(out2, "\x1b[38;2;%i;%i;%im", r, g, b);
	}
}

static void process(char c, FILE *out1, FILE *out2) {
	static int depth = 0;

	if (c == '\n') {
		color(depth = 0, out1, out2);
		putc(c, out1);
		putc(c, out2);
	} else if (c == '<') {
		color(++depth, out1, out2);
		putc(c, out1);
		putc(c, out2);
	} else if (c == '>' && depth > 0) {
		putc(c, out1);
		putc(c, out2);
		color(--depth, out1, out2);
	} else {
		putc(c, out1);
		putc(c, out2);
	}
}

int main(int argc, char *argv[]) {
	if (argc == 1) {
		printf("Usage: %s <command...>\n", argv[0]);
		return EXIT_FAILURE;
	}

	const char *pager = getenv("MSOAK_PAGER");
	if (pager == NULL)
		pager = "less --raw-control-chars";

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
