#include <assert.h>
#include <unistd.h>
#include <libliftoff.h>
#include <stdio.h>
#include <string.h>
#include "libdrm_mock.h"

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

int main(int argc, char *argv[]) {
	const char *test_name;
	struct liftoff_mock_plane *mock_plane;
	int drm_fd;
	struct liftoff_display *display;
	struct liftoff_output *output;
	struct liftoff_layer *layer;
	drmModeAtomicReq *req;
	bool ok;
	size_t commit_count;
	bool want_reuse_prev_alloc;

	liftoff_log_init(LIFTOFF_DEBUG, NULL);

	if (argc != 2) {
		fprintf(stderr, "usage: %s <test-name>\n", argv[0]);
		return 1;
	}
	test_name = argv[1];

	mock_plane = liftoff_mock_drm_create_plane(DRM_PLANE_TYPE_PRIMARY);
	/* Plane incompatible with all layers */
	liftoff_mock_drm_create_plane(DRM_PLANE_TYPE_CURSOR);

	drm_fd = liftoff_mock_drm_open();
	display = liftoff_display_create(drm_fd);
	assert(display != NULL);

	output = liftoff_output_create(display, liftoff_mock_drm_crtc_id);
	layer = add_layer(output, 0, 0, 1920, 1080);
	/* Layer incompatible with all planes */
	add_layer(output, 0, 0, 256, 256);

	liftoff_mock_plane_add_compatible_layer(mock_plane, layer);

	req = drmModeAtomicAlloc();
	ok = liftoff_output_apply(output, req);
	assert(ok);
	assert(liftoff_mock_plane_get_layer(mock_plane, req) == layer);
	drmModeAtomicFree(req);

	commit_count = liftoff_mock_commit_count;
	/* We need to check whether the library can re-use old configurations
	 * with a single atomic commit. If we don't have enough planes/layers,
	 * the library will find a plane allocation in a single commit and we
	 * won't be able to tell the difference between a re-use and a complete
	 * run. */
	assert(commit_count > 1);

	req = drmModeAtomicAlloc();

	want_reuse_prev_alloc = true;
	if (strcmp(test_name, "same") == 0) {
		// This space is intentionally left blank
	} else if (strcmp(test_name, "fb") == 0) {
		liftoff_layer_set_property(layer, "FB_ID",
					   liftoff_mock_drm_create_fb(layer));
	} else if (strcmp(test_name, "new-layer") == 0) {
		add_layer(output, 0, 0, 256, 256);
		want_reuse_prev_alloc = false;
	} else {
		fprintf(stderr, "no such test: %s\n", test_name);
		return 1;
	}

	ok = liftoff_output_apply(output, req);
	assert(ok);
	assert(liftoff_mock_plane_get_layer(mock_plane, req) == layer);
	if (want_reuse_prev_alloc) {
		assert(liftoff_mock_commit_count == commit_count + 1);
	} else {
		assert(liftoff_mock_commit_count > commit_count + 1);
	}

	drmModeAtomicFree(req);

	liftoff_display_destroy(display);
	close(drm_fd);

	return 0;
}
