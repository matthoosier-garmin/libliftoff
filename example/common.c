#include <assert.h>
#include <drm_fourcc.h>
#include <stddef.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include "common.h"

drmModeConnector *pick_connector(int drm_fd, drmModeRes *drm_res)
{
	int i;
	drmModeConnector *connector;

	for (i = 0; i < drm_res->count_connectors; i++) {
		connector = drmModeGetConnector(drm_fd, drm_res->connectors[i]);
		if (connector->connection == DRM_MODE_CONNECTED) {
			return connector;
		}
		drmModeFreeConnector(connector);
	}

	return NULL;
}

drmModeCrtc *pick_crtc(int drm_fd, drmModeRes *drm_res,
		       drmModeConnector *connector)
{
	drmModeEncoder *enc;
	uint32_t crtc_id;

	/* TODO: don't blindly use current CRTC */
	enc = drmModeGetEncoder(drm_fd, connector->encoder_id);
	crtc_id = enc->crtc_id;
	drmModeFreeEncoder(enc);

	return drmModeGetCrtc(drm_fd, crtc_id);
}

void disable_all_crtcs_except(int drm_fd, drmModeRes *drm_res, uint32_t crtc_id)
{
	int i;

	for (i = 0; i < drm_res->count_crtcs; i++) {
		if (drm_res->crtcs[i] == crtc_id) {
			continue;
		}
		drmModeSetCrtc(drm_fd, drm_res->crtcs[i],
			0, 0, 0, NULL, 0, NULL);
	}
}

bool dumb_fb_init(struct dumb_fb *fb, int drm_fd, uint32_t format,
		  uint32_t width, uint32_t height)
{
	int ret;
	uint32_t fb_id;

	assert(format == DRM_FORMAT_ARGB8888 || format == DRM_FORMAT_XRGB8888);

	struct drm_mode_create_dumb create = {
		.width = width,
		.height = height,
		.bpp = 32,
		.flags = 0,
	};
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		return false;
	}

	uint32_t handles[4] = { create.handle };
	uint32_t strides[4] = { create.pitch };
	uint32_t offsets[4] = { 0 };
	ret = drmModeAddFB2(drm_fd, width, height, format, handles, strides,
			    offsets, &fb_id, 0);
	if (ret < 0) {
		return false;
	}

	fb->width = width;
	fb->height = height;
	fb->stride = create.pitch;
	fb->size = create.size;
	fb->handle = create.handle;
	fb->id = fb_id;
	return true;
}

void *dumb_fb_map(struct dumb_fb *fb, int drm_fd)
{
	int ret;

	struct drm_mode_map_dumb map = { .handle = fb->handle };
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
	if (ret < 0) {
		return MAP_FAILED;
	}

	return mmap(0, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd,
		    map.offset);
}

void dumb_fb_fill(struct dumb_fb *fb, int drm_fd, uint32_t color)
{
	uint32_t *data;
	size_t i;

	data = dumb_fb_map(fb, drm_fd);
	if (data == MAP_FAILED) {
		return;
	}

	for (i = 0; i < fb->size / sizeof(uint32_t); i++) {
		data[i] = color;
	}

	munmap(data, fb->size);
}
