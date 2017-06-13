#ifndef _MAIN_H_
#define _MAIN_H_

#define VERSION "1.47-WIP"

#include <boost/property_tree/ptree.hpp>
#include <SDL2/SDL.h>
#include "core/api/NstApiEmulator.hpp"
#include "core/api/NstApiSound.hpp"
#include "core/api/NstApiInput.hpp"
#include "core/api/NstApiVideo.hpp"
#include "core/api/NstApiNsf.hpp"

using namespace Nes::Api;

void audio_init();
void audio_deinit();
void audio_play();
void audio_pause();
void audio_unpause();
void audio_set_params(Sound::Output *soundoutput);
void audio_adj_volume();

bool timing_frameskip();
void timing_set_default();
void timing_set_altspeed();

typedef struct {
	char nstdir[256];
	char savedir[256];
	char gamename[256];
	char savename[512];
	char fdssave[512];
	char statepath[512];
	char cheatpath[512];
} nstpaths_t;

long video_lock_screen(void*& ptr);
bool nst_archive_checkext(const char *filename);
bool nst_archive_handle(const char *filename, char **rom, int *romsize, const char *reqfile);
bool nst_find_patch(char *filename);
void nst_load_db();
void nst_load_fds_bios();
void nst_load(const char *filename);
void nst_play();
void nst_pause();
void nst_reset(bool hardreset);
void nst_schedule_quit();
void nst_set_dirs();
void nst_set_region();
void nst_set_rewind(int direction);

void nst_set_paths(const char *filename);

void nst_state_save(char *filename);
void nst_state_load(char *filename);
void nst_state_quicksave(int isvst);
void nst_state_quickload(int isvst);

void nst_movie_save(char *filename);
void nst_movie_load(char *filename);
void nst_movie_stop();

void nst_fds_info();
void nst_flip_disk();
void nst_switch_disk();

void nst_dipswitch();

void video_init();
void video_set_filter();
void video_clear_buffer();
typedef struct {
	int w;
	int h;
} dimensions_t;
#define TV_WIDTH 292
#define OVERSCAN_LEFT 0
#define OVERSCAN_RIGHT 0
#define OVERSCAN_BOTTOM 8
#define OVERSCAN_TOP 8


typedef struct {
	
	// Video
	int video_filter;
	int video_scale_factor;
	int video_palette_mode;
	int video_decoder;
	int video_brightness;
	int video_saturation;
	int video_contrast;
	int video_hue;
	int video_ntsc_mode;
	int video_xbr_corner_rounding;
	bool video_linear_filter;
	bool video_tv_aspect;
	bool video_unmask_overscan;
	bool video_fullscreen;
	bool video_stretch_aspect;
	bool video_unlimited_sprites;
	bool video_xbr_pixel_blending;
	
	// Audio
	int audio_api;
	bool audio_stereo;
	int audio_sample_rate;
	int audio_volume;
	int audio_vol_sq1;
	int audio_vol_sq2;
	int audio_vol_tri;
	int audio_vol_noise;
	int audio_vol_dpcm;
	int audio_vol_fds;
	int audio_vol_mmc5;
	int audio_vol_vrc6;
	int audio_vol_vrc7;
	int audio_vol_n163;
	int audio_vol_s5b;
	
	// Timing
	int timing_speed;
	int timing_altspeed;
	int timing_turbopulse;
	bool timing_vsync;
	bool timing_limiter;
	
	// Misc
	//int misc_video_region;
	int misc_default_system;
	bool misc_soft_patching;
	//bool misc_suppress_screensaver;
	bool misc_genie_distortion;
	bool misc_disable_gui;
	bool misc_config_pause;
} settings_t;

void config_set_default();

typedef struct {
	unsigned char player;
	unsigned char nescode;
	unsigned char pressed;
	unsigned char turboa;
	unsigned char turbob;
} nesinput_t;

typedef struct {
	int p1a;
	int p1b;
	int p2a;
	int p2b;
} turbo_t;

typedef struct {
  unsigned char player;
  unsigned int u:1;
  unsigned int d:1;
  unsigned int l:1;
  unsigned int r:1;
  unsigned int select:1;
  unsigned int start:1;
  unsigned int a:1;
  unsigned int b:1;
  unsigned int ta:1;
  unsigned int tb:1;
  unsigned int altspeed:1;
  unsigned int insertcoin1:1;
  unsigned int insertcoin2:1;
  unsigned int fdsflip:1;
  unsigned int fdsswitch:1;
  unsigned int qsave1:1;
  unsigned int qsave2:1;
  unsigned int qload1:1;
  unsigned int qload2:1;
  unsigned int screenshot:1;
  unsigned int reset:1;
  unsigned int rwstart:1;
  unsigned int rwstop:1;
  unsigned int fullscreen:1;
  unsigned int filter:1;
  unsigned int scalefactor:1;
  unsigned int quit:1;
} networkinput_t;
typedef struct {
	SDL_Scancode u;
	SDL_Scancode d;
	SDL_Scancode l;
	SDL_Scancode r;
	SDL_Scancode select;
	SDL_Scancode start;
	SDL_Scancode a;
	SDL_Scancode b;
	SDL_Scancode ta;
	SDL_Scancode tb;
	
	SDL_Event ju;
	SDL_Event jd;
	SDL_Event jl;
	SDL_Event jr;
	SDL_Event jselect;
	SDL_Event jstart;
	SDL_Event ja;
	SDL_Event jb;
	SDL_Event jta;
	SDL_Event jtb;
} gamepad_t;

typedef struct {
	SDL_Scancode qsave1;
	SDL_Scancode qsave2;
	SDL_Scancode qload1;
	SDL_Scancode qload2;
	
	SDL_Scancode screenshot;
	
	SDL_Scancode fdsflip;
	SDL_Scancode fdsswitch;
	
	SDL_Scancode insertcoin1;
	SDL_Scancode insertcoin2;
	
	SDL_Scancode reset;
	
	SDL_Scancode altspeed;
	SDL_Scancode rwstart;
	SDL_Scancode rwstop;
	
	SDL_Scancode fullscreen;
	SDL_Scancode filter;
	SDL_Scancode scalefactor;
} uiinput_t;

void input_init();
void input_set_default();
void input_pulse_turbo(Input::Controllers *controllers);
SDL_Event input_translate_string(char *string);
void input_match_network(Input::Controllers*, boost::property_tree::ptree);
void input_inject(Input::Controllers *controllers, nesinput_t input);

#define NUMGAMEPADS 2
#define NUMBUTTONS 10
#define TOTALBUTTONS (NUMGAMEPADS*NUMBUTTONS)
#define DEADZONE (32768/3)
#endif
