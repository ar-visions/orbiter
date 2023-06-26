#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

#include <stdarg.h>
#include <ctype.h>

#include <vke/vke.h>
#include <vkg/vkg.h>

#include "vectors.h"

#include "tessellate.h"

static VkgDevice dev;
static VkgSurface svgSurf = NULL;
static VkSampleCountFlags samples = VK_SAMPLE_COUNT_8_BIT;
static uint32_t width=1024, height=768;
static bool paused = false, refresh = true;
static float scale = 1.0f;
static int currentTri = -1;

#define NORMAL_COLOR  "\x1B[0m"
#define GREEN  "\x1B[32m"
#define BLUE  "\x1B[34m"

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE)
		return;
	switch (key) {
	case GLFW_KEY_SPACE:
		paused = !paused;
		break;
	case GLFW_KEY_ESCAPE :
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	case GLFW_KEY_KP_ADD:
		currentTri++;
		break;
	case GLFW_KEY_KP_SUBTRACT:
		if (currentTri >= 0)
			currentTri--;
		break;
	}
	refresh = true;
}

void print_help_and_exit () {
	printf("VKVG glutess test\n");
	exit(-1);
}

#define white	1,1,1
#define red		1,0,0
#define green	0,1,0
#define blue	0,0,1

const float arrow_size = 10;
const float arrow_hwidth = 4;
const float point_label_delta = -10;

void draw_arrow (VkgContext ctx, float x, float y) {
	vec2 p0;
	vec2 p1 = {x,y};
	vkg_get_current_point(ctx, &p0.x, &p0.y);

	vec2 dir = vec2f_sub(p0,p1);
	vec2 n = vec2f_norm (dir);
	vec2 perp = vec2f_scale (vec2f_perp(n), arrow_hwidth);

	vkg_line_to(ctx, x, y);
	vkg_stroke(ctx);
	vkg_move_to(ctx, x, y);
	vec2 p = vec2f_add(p1, vec2f_scale (n, arrow_size));
	vec2 a = vec2f_add(p, perp);
	vkg_line_to(ctx, a.x, a.y);
	a = vec2f_add(p, vec2f_scale (perp, -1));
	vkg_line_to(ctx, a.x, a.y);
	vkg_close_path(ctx);
	vkg_fill(ctx);
	vkg_move_to(ctx,x,y);
}

void test (VkgContext ctx) {
	double vertices_array[] = { 100, 100, 400, 100, 400, 400, 100, 400,
								300, 200, 200, 200, 200, 300, 300, 300 };
	const double *contours_array[] = {vertices_array, vertices_array+8, vertices_array+16};
	int contours_size = 3;

	double *coordinates_out;
	int *tris_out;
	int nverts, ntris;

	const double *p = vertices_array;

	tessellate(&coordinates_out, &nverts,
			   &tris_out, &ntris,
			   contours_array, contours_array + contours_size);

	vkg_set_line_width(ctx, 2);
	vkg_set_source_rgb(ctx, red);
	for (int i=0; i < contours_size-1; i++) {
		p = contours_array[i];
		vkg_move_to (ctx, p[0]*scale, p[1]*scale);
		p+=2;
		while (p<contours_array[i+1]) {
			draw_arrow (ctx, p[0]*scale, p[1]*scale);
			p+=2;
		}
		vkg_stroke(ctx);
	}

	vkg_set_source_rgb(ctx, white);
	for (int i=0; i < contours_size-1; i++) {
		p = contours_array[i];
		while (p<contours_array[i+1]) {
			vkg_arc(ctx, p[0]*scale, p[1]*scale, 3, 0, M_PI*2);
			vkg_fill(ctx);
			p+=2;
		}
	}
	//result
	vkg_translate(ctx,400,0);

	vkg_set_source_rgba(ctx, 0.5,0.5,1,0.3);

	if (currentTri >= 0 && currentTri < ntris) {
		const int* indices = &tris_out[currentTri*3];
		p = &coordinates_out[indices[0]*2];
		vkg_move_to(ctx,p[0]*scale, p[1]*scale);
		p = &coordinates_out[indices[1]*2];
		vkg_line_to(ctx,p[0]*scale, p[1]*scale);
		p = &coordinates_out[indices[2]*2];
		vkg_line_to(ctx,p[0]*scale, p[1]*scale);
		vkg_close_path(ctx);

		vkg_fill_preserve(ctx);
		vkg_stroke(ctx);
	} else {

		for (int i=0; i<ntris; i++) {
			const int* indices = &tris_out[i*3];
			p = &coordinates_out[indices[0]*2];
			vkg_move_to(ctx,p[0]*scale, p[1]*scale);
			p = &coordinates_out[indices[1]*2];
			vkg_line_to(ctx,p[0]*scale, p[1]*scale);
			p = &coordinates_out[indices[2]*2];
			vkg_line_to(ctx,p[0]*scale, p[1]*scale);
			vkg_close_path(ctx);

			vkg_fill_preserve(ctx);
			vkg_stroke(ctx);
		}
	}

	vkg_set_source_rgb(ctx, green);

	char txt[10];

	for (int i=0; i<nverts; ++i) {
		p = &coordinates_out[i*2];
		vkg_set_source_rgb(ctx, green);
		vkg_new_path(ctx);
		vkg_arc(ctx, p[0]*scale, p[1]*scale, 3, 0, M_PI*2);
		vkg_fill(ctx);
		vkg_set_source_rgb(ctx, white);
		vkg_move_to(ctx, p[0]*scale + point_label_delta, p[1]*scale+point_label_delta);
		sprintf(txt, "%d", i);
		vkg_show_text(ctx, txt);
	}

	free(coordinates_out);
	if (tris_out)
		free(tris_out);
}


int main (int argc, char *argv[]){
	int i = 1;

	while (i < argc) {
		int argLen = strlen(argv[i]);
		if (argv[i][0] == '-') {

			if (argLen < 2)	print_help_and_exit ();

			switch (argv[i][1]) {
			case 'w':
				if (argc < ++i + 1) print_help_and_exit();
				width = atoi(argv[i]);
				break;
			case 'h':
				if (argc < ++i + 1) print_help_and_exit();
				height = atoi(argv[i]);
				break;
			case 's':
				if (argc < ++i + 1) print_help_and_exit();
				samples = (VkSampleCountFlags)atoi(argv[i]);
				break;
			default:
				print_help_and_exit();
			}
		}
		i++;
	}
	VkEngine e = vkengine_create (VK_PRESENT_MODE_FIFO_KHR, width, height);
	vkengine_set_key_callback (e, key_callback);

	dev = vkg_device_create_from_vk_multisample (vke_app_get_inst(e->app),
												  vkengine_get_physical_device(e), vkengine_get_device(e), vkengine_get_queue_fam_idx(e), 0, samples, false);

	VkgSurface surf = vkg_surface_create(dev, width, height);

	vke_presenter_build_blit_cmd (e->renderer, vkg_surface_get_vk_image(surf), width, height);

	while (!vkengine_should_close (e)) {

		if (refresh) {
			refresh = false;
			VkgContext ctx = vkg_create(surf);
			vkg_set_source_rgb(ctx,0.1,0.1,0.1);
			vkg_paint(ctx);
			test (ctx);
			io_drop(vkg_context, ctx);
		}

		glfwPollEvents();

		if(!vke_presenter_draw (e->renderer)){
			vke_presenter_get_size (e->renderer, &width, &height);
			vkg_surface_drop(surf);
			surf = vkg_surface_create(dev, width, height);
			vke_presenter_build_blit_cmd (e->renderer, vkg_surface_get_vk_image(surf), width, height);
			vkengine_wait_idle(e);
			refresh = true;
			continue;
		}

	}
	
	vkengine_wait_idle(e);
	
	if (svgSurf)
		vkg_surface_drop(svgSurf);
	vkg_surface_drop(surf);
	vkg_device_drop(dev);
	vkengine_destroy(e);
}
