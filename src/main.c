
#define _POSIX_C_SOURCE 200809L
#include "wlr-gamma-control-unstable-v1-client-protocol.h"
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

#define BINARY_NAME "wlr-gammactl-fzn"

// wayland specifics
static struct zwlr_gamma_control_manager_v1 *gamma_control_manager =
    NULL; // handle to the registry global for gamma management
struct wl_display *display = NULL; // handle to wayland connection
static struct wl_list outputs;     // list for all present wayland outputs

struct output {
  struct wl_output *wl_output;
  struct zwlr_gamma_control_v1 *gamma_control;
  uint32_t ramp_size;
  int table_fd;
  uint16_t *table;
  struct wl_list link;
};

#define BUFFER_SIZE 50
char buffer[BUFFER_SIZE];

static void fill_gamma_table(uint16_t *table, uint32_t ramp_size,
                             double contrast[3], double brightness[3],
                             double gamma[3]) {
  uint16_t *r = table;
  uint16_t *g = table + ramp_size;
  uint16_t *b = table + 2 * ramp_size;
  for (uint32_t i = 0; i < ramp_size; ++i) {
    double vals[3];
    for (uint32_t j = 0; j < 3; j++) {
      double val = (double)i / (ramp_size - 1);
      val = contrast[j] * pow(val, 1.0 / gamma[j]) + (brightness[j] - 1);
      if (val > 1.0) {
        val = 1.0;
      } else if (val < 0.0) {
        val = 0.0;
      }
      vals[j] = val;
    }

    r[i] = (uint16_t)(UINT16_MAX * vals[0]);
    g[i] = (uint16_t)(UINT16_MAX * vals[1]);
    b[i] = (uint16_t)(UINT16_MAX * vals[2]);
  }
}

static int create_anonymous_file(off_t size) {
  char template[] = "/tmp/wlroots-shared-XXXXXX";
  int fd = mkstemp(template);
  if (fd < 0) {
    return -1;
  }

  int ret;
  do {
    errno = 0;
    ret = ftruncate(fd, size);
  } while (errno == EINTR);
  if (ret < 0) {
    close(fd);
    return -1;
  }

  unlink(template);
  return fd;
}

static int create_gamma_table(uint32_t ramp_size, uint16_t **table) {
  size_t table_size = ramp_size * 3 * sizeof(uint16_t);
  int fd = create_anonymous_file(table_size);
  if (fd < 0) {
    fprintf(stderr, "failed to create anonymous file\n");
    return -1;
  }

  void *data =
      mmap(NULL, table_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    fprintf(stderr, "failed to mmap()\n");
    close(fd);
    return -1;
  }

  *table = data;
  return fd;
}

static void
gamma_control_handle_gamma_size(void *data,
                                struct zwlr_gamma_control_v1 *gamma_control,
                                uint32_t ramp_size) {
  struct output *output = data;
  output->ramp_size = ramp_size;
}

static void
gamma_control_handle_failed(void *data,
                            struct zwlr_gamma_control_v1 *gamma_control) {
  fprintf(stderr, "failed to set gamma table\n");
  exit(EXIT_FAILURE);
}

static const struct zwlr_gamma_control_v1_listener gamma_control_listener = {
    .gamma_size = gamma_control_handle_gamma_size,
    .failed = gamma_control_handle_failed,
};

static void registry_handle_global(void *data, struct wl_registry *registry,
                                   uint32_t name, const char *interface,
                                   uint32_t version) {
  // bind wl_output registry global
  if (strcmp(interface, wl_output_interface.name) == 0) {
    struct output *output = calloc(1, sizeof(struct output));
    output->wl_output =
        wl_registry_bind(registry, name, &wl_output_interface, 1);
    wl_list_insert(&outputs, &output->link);
  } else if (strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) ==
             0) {
    gamma_control_manager = wl_registry_bind(
        registry, name, &zwlr_gamma_control_manager_v1_interface, 1);
  }
}

static void registry_handle_global_remove(void *data,
                                          struct wl_registry *registry,
                                          uint32_t name) {
  // Who cares?
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

void wl_set_cbg(double contrast[3], double brightness[3], double gamma[3]) {
  struct output *output;
  wl_list_for_each(output, &outputs, link) {
    output->table_fd = create_gamma_table(output->ramp_size, &output->table);
    if (output->table_fd < 0) {
      exit(EXIT_FAILURE);
    }
    printf("> Adjusting %d\n", output->ramp_size);

    fill_gamma_table(output->table, output->ramp_size, contrast, brightness,
                     gamma);
    zwlr_gamma_control_v1_set_gamma(output->gamma_control, output->table_fd);
    close(output->table_fd);
  }

  wl_display_roundtrip(display);
}

static const char usage[] =
    "usage: " BINARY_NAME " [options]\n"
    "  -h              show this help message\n"
    "  -c <r>:<g>:<b>  set contrast (default: 1:1:1)\n"
    "  -b <r>:<g>:<b>  set brightness (default: 1:1:1)\n"
    "  -g <r>:<g>:<b>  set gamma (default: 1:1:1)\n"
    "  no options to show the gui\n";

void parse3(char *s, double *vals) {
  char *token = strtok(s, ":");
  int i = 0;
  while (token != NULL) {
    vals[i] = strtod(token, NULL);
    i += 1;
    token = strtok(NULL, ":");
  }
}

void print3(double *vals) {
  for (int i = 0; i < 3; i++) {
    printf("%.2f ", vals[i]);
  }
}

int run_cmdline(int argc, char *argv[]) {
  double contrast[3] = {1, 1, 1}, brightness[3] = {1, 1, 1},
         gamma[3] = {1, 1, 1};
  int opt;
  while ((opt = getopt(argc, argv, "hc:b:g:")) != -1) {
    switch (opt) {
    case 'c':
      parse3(optarg, contrast);
      break;
    case 'b':
      parse3(optarg, brightness);
      break;
    case 'g':
      parse3(optarg, gamma);
      break;
    case 'h':
    default:
      fprintf(stderr, usage);
      return opt == 'h' ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  }

  printf("Contrast: ");
  print3(contrast);
  printf("\n");

  printf("Brightness: ");
  print3(brightness);
  printf("\n");

  printf("Gamma: ");
  print3(gamma);
  printf("\n");

  wl_set_cbg(contrast, brightness, gamma);

  while (wl_display_dispatch(display) != -1) {
    // This space is intentionnally left blank
  }
  return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
  // init the list of outputs
  wl_list_init(&outputs);

  // connect to wayland display
  display = wl_display_connect(NULL);
  if (display == NULL) {
    fprintf(stderr, "cannot connect to display\n");
    exit(EXIT_FAILURE);
  }

  // get handle on the registry
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, NULL);
  // send our messages (async), does this happen anyway??
  wl_display_dispatch(display);
  // block here because we want to wait for our async reply of the server
  wl_display_roundtrip(display);

  // check if we got a handle to the gamma controller global
  if (gamma_control_manager == NULL) {
    fprintf(stderr, "compositor doesn't support "
                    "wlr-gamma-control-unstable-v1\n");
    return EXIT_FAILURE;
  }

  // now we go into the gamma specifics
  // for each output in our stored list:
  //  1. create a gamma controller for this specific output
  //  2. add a listener function
  struct output *output;
  wl_list_for_each(output, &outputs, link) {
    output->gamma_control = zwlr_gamma_control_manager_v1_get_gamma_control(
        gamma_control_manager, output->wl_output);
    zwlr_gamma_control_v1_add_listener(output->gamma_control,
                                       &gamma_control_listener, output);
  }
  // once again, wait until this has been set and is ready
  wl_display_roundtrip(display);

  return run_cmdline(argc, argv);
}
