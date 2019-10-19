#define _POSIX_C_SOURCE 199309L
#include <assert.h>
#include <libliftoff.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "libdrm_mock.h"
#include "log.h"

#define PLANES_LEN 5
#define LAYERS_LEN 10

static struct liftoff_layer *add_layer(struct liftoff_output *output,
				       int x, int y, int width, int height)
{
	uint32_t fb_id;
	struct liftoff_layer *layer;

	layer = liftoff_layer_create(output);
	fb_id = liftoff_mock_drm_create_fb(layer);
	liftoff_layer_set_property(layer, "FB_ID", fb_id);
	liftoff_layer_set_property(layer, "CRTC_X", x);
	liftoff_layer_set_property(layer, "CRTC_Y", y);
	liftoff_layer_set_property(layer, "CRTC_W", width);
	liftoff_layer_set_property(layer, "CRTC_H", height);
	liftoff_layer_set_property(layer, "SRC_X", 0);
	liftoff_layer_set_property(layer, "SRC_Y", 0);
	liftoff_layer_set_property(layer, "SRC_W", width << 16);
	liftoff_layer_set_property(layer, "SRC_H", height << 16);

	return layer;
}

int main(int argc, char *argv[])
{
	struct timespec start, end;
	struct liftoff_mock_plane *mock_planes[PLANES_LEN];
	size_t i, j;
	int plane_type;
	int drm_fd;
	struct liftoff_display *display;
	struct liftoff_output *output;
	struct liftoff_layer *layers[LAYERS_LEN];
	drmModeAtomicReq *req;
	bool ok;

	liftoff_log_init(LIFTOFF_SILENT, NULL);

	for (i = 0; i < PLANES_LEN; i++) {
		plane_type = i == 0 ? DRM_PLANE_TYPE_PRIMARY :
				      DRM_PLANE_TYPE_OVERLAY;
		mock_planes[i] = liftoff_mock_drm_create_plane(plane_type);
	}

	drm_fd = liftoff_mock_drm_open();
	display = liftoff_display_create(drm_fd);
	assert(display != NULL);

	output = liftoff_output_create(display, liftoff_mock_drm_crtc_id);

	for (i = 0; i < LAYERS_LEN; i++) {
		layers[i] = add_layer(output, i * 100, i * 100, 100, 100);
		for (j = 0; j < PLANES_LEN; j++) {
			if (j == 1) { /* lowest overlay plane */
				continue;
			}
			liftoff_mock_plane_add_compatible_layer(mock_planes[j],
								layers[i]);
		}
	}

	clock_gettime(CLOCK_MONOTONIC, &start);

	req = drmModeAtomicAlloc();
	ok = liftoff_display_apply(display, req);
	assert(ok);
	drmModeAtomicFree(req);

	clock_gettime(CLOCK_MONOTONIC, &end);

	double dur_ms = ((double)end.tv_sec - (double)start.tv_sec) * 1000 +
			((double)end.tv_nsec - (double)start.tv_nsec) / 1000000;
	printf("Plane allocation took %fms\n", dur_ms);
	printf("Plane allocation performed %zu atomic test commits\n",
	       liftoff_mock_commit_count);
	printf("With 20µs per atomic test commit, plane allocation would take "
	       "%fms\n", dur_ms + liftoff_mock_commit_count * 0.02);

	liftoff_display_destroy(display);
	close(drm_fd);
}
