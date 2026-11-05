#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static void die(const char *s) {
  fprintf(stderr, "%s\n", s);
  exit(1);
}

int N = 801;
int **grid;
static int *row_begin;
static int *row_end;

static void init_row_bounds(void) {
  row_begin = malloc((size_t)N * sizeof(int));
  row_end = malloc((size_t)N * sizeof(int));
  if (!row_begin || !row_end)
    die("malloc failed for row bounds");

  int64_t radius = N / 2;
  for (int i = 0; i < N; ++i) {
    row_begin[i] = 0;
    row_end[i] = -1;
    int64_t dy = i - radius;

    for (int j = 0; j <= MAX(i, N - 1 - i); ++j) {
      int64_t dx = j - radius;
      if (dy * dy + dx * dx <= radius * radius) {
        if (row_end[i] == -1)
          row_begin[i] = j;
        row_end[i] = j;
      }
    }
  }
}




// you can also use this function to debug print your sandpile
static void render(char *filename) {
  int chan = 3;
  unsigned char *pixels = (unsigned char *)malloc(N * N * chan * sizeof(char));
  int stride = N * chan;
  if (!pixels)
    die("malloc failed for PNG pixel buffer");
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      unsigned char r = 0, g = 0, b = 0;
      switch (grid[i][j]) {
      case 0: // obsidian
        r = 15;
        g = 15;
        b = 25;
        break;
      case 1: // electric cyan
        r = 0;
        g = 210;
        b = 255;
        break;
      case 2: // vibrant magenta
        r = 230;
        g = 30;
        b = 130;
        break;
      case 3: // bright gold
        r = 255;
        g = 200;
        b = 0;
        break;
      default: // white
        r = 0;
        g = 0;
        b = 0;
      }

      int pid = (i * N + j) * chan;
      pixels[pid] = r;
      pixels[pid + 1] = g;
      pixels[pid + 2] = b;
    }
  }
  if (stbi_write_png(filename, N, N, chan, pixels, stride)) {
    printf("Successfully saved sandpile identity to: %s\n", filename);
  } else {
    die("Error: stb_image_write failed to save png file\n");
  }
  free(pixels);
}


static void stabilize() {
  int n = N;
  int rows = n % 2 ? n / 2 + 1 : n;
  int changed = 0;
  do {
    changed = 0;
    for (int phase = 0; phase < 3; ++phase) {
#pragma omp parallel for schedule(static,4) reduction(| : changed)
      for (int i = phase; i < rows; i += 3) {
        int *row = grid[i];
        int *up = i > 0 ? grid[i - 1] : NULL;
        int *down = i + 1 < rows ? grid[i + 1] : NULL;
        int down_grains = n % 2 && i == n / 2 - 1 ? 2 : 1;
        int end = row_end[i];

        for (int j = row_begin[i]; j <= end; ++j) {
          if (row[j] < 4)
            continue;
          changed = 1;
          row[j] -= 4;
          if (up)
            up[j]++;
          if (j > 0)
            row[j - 1]++;
          if (down)
            down[j] += down_grains;
          if (j < n - 1)
            row[j + 1]++;
        }
      }
    }
  } while (changed);

  if (n % 2) {
    for (int i = 0; i < n / 2; ++i)
      memcpy(grid[n - 1 - i], grid[i], (size_t)n * sizeof(int));
  }
}


void find_identity() {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      grid[i][j] = 6;
    }
  }
  stabilize();
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      grid[i][j] = 6 - grid[i][j];
    }
  }
  stabilize();
}

int main(int argc, char **argv) {
  if (argc < 2 || argc > 2) {
    fprintf(stderr, "Usage: %s N\n", argv[0]);
    return EXIT_FAILURE;
  }
  char *end;
  uintmax_t parsed = strtoumax(argv[1], &end, 10);
  if (errno || argv[1][0] < '0' || argv[1][0] > '9' || *end || !parsed ||
      parsed > INT32_MAX)
    die("grid size must be a positive int");
  N = (int)parsed;

  grid = (int **)malloc(N * sizeof(int *));
  for (int i = 0; i < N; ++i) {
    grid[i] = (int *)malloc(N * sizeof(int));
  }

  init_row_bounds();
  find_identity();

  render("identity.png");
  for (int i = 0; i < N; ++i) {
    free(grid[i]);
  }
  free(grid);
  free(row_begin);
  free(row_end);
  return EXIT_SUCCESS;
}
