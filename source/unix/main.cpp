/*
 * Nestopia UE
 * 
 * Copyright (C) 2007-2008 R. Belmont
 * Copyright (C) 2012-2015 R. Danbrook
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 */
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <cassert>
#include <exception>
#include <sstream>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <memory>

#include <iomanip>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <archive.h>
#include <archive_entry.h>

#include "core/api/NstApiEmulator.hpp"
#include "core/api/NstApiVideo.hpp"
#include "core/api/NstApiSound.hpp"
#include "core/api/NstApiInput.hpp"
#include "core/api/NstApiMachine.hpp"
#include "core/api/NstApiUser.hpp"
#include "core/api/NstApiFds.hpp"
#include "core/api/NstApiDipSwitches.hpp"
#include "core/api/NstApiRewinder.hpp"
#include "core/api/NstApiCartridge.hpp"
#include "core/api/NstApiMovie.hpp"
#include "core/api/NstApiNsf.hpp"

#include "main.h"

//#include <grpc/grpc.h>                                                                                                                                                                                              
//#include <grpc++/server.h>                                                                                                                                                 
//#include <grpc++/server_builder.h>                                                                                                                                         
//#include <grpc++/server_context.h>                                                                                                                                         
//#include <grpc++/security/server_credentials.h>                                                                                                                            
//#include "deep_thought.grpc.pb.h"

using namespace Nes::Api;


grpcNESEmulator::grpcNESEmulator (int sfd) {
  sockfd = sfd;
}

// called right before Nestopia is about to write pixels
bool NST_CALLBACK VideoLock(grpcNESEmulator* self, void* userData, Video::Output& video) {
	video.pitch = self->video_lock_screen(video.pixels);
	return true; // true=lock success, false=lock failed (Nestopia will carry on but skip video)
}

// called right after Nestopia has finished writing pixels (not called if previous lock failed)
void NST_CALLBACK VideoUnlock(grpcNESEmulator& self, void* userData, Video::Output& video) {
  self.videoStream = video;
	//video_unlock_screen(video.pixels);
}

bool NST_CALLBACK SoundLock(void* userData, Sound::Output& sound) {
	return true;
}

void NST_CALLBACK SoundUnlock(void* userData, Sound::Output& sound) {
	// Do Nothing
}

void NST_CALLBACK nst_cb_event(void *userData, User::Event event, const void* data) {
	// Handle special events
	switch (event) {
		case User::EVENT_CPU_JAM:
			fprintf(stderr, "Cpu: Jammed\n");
			break;
		case User::EVENT_CPU_UNOFFICIAL_OPCODE:
			fprintf(stderr, "Cpu: Unofficial Opcode %s\n", (const char*)data);
			break;
		case User::EVENT_DISPLAY_TIMER:
			fprintf(stderr, "\r%s", (const char*)data);
			//snprintf(timebuf, sizeof(timebuf), "%s", (const char*)data + strlen((char*)data) - 5);
			//drawtime = true;
			break;
		default: break;
	}
}

void NST_CALLBACK nst_cb_log(void *userData, const char *string, unsigned long int length) {
	// Print logging information to stderr
	fprintf(stderr, "%s", string);
}

/*
void NST_CALLBACK nst_cb_file(grpcNESEmulator& self, void *userData, User::File& file) {
	unsigned char *compbuffer;
	int compsize, compoffset;
	char *filename;
	
	switch (file.GetAction()) {
		case User::File::LOAD_ROM:
			// Nothing here for now			
			break;

		case User::File::LOAD_SAMPLE:
		case User::File::LOAD_SAMPLE_MOERO_PRO_YAKYUU:
		case User::File::LOAD_SAMPLE_MOERO_PRO_YAKYUU_88:
		case User::File::LOAD_SAMPLE_MOERO_PRO_TENNIS:
		case User::File::LOAD_SAMPLE_TERAO_NO_DOSUKOI_OOZUMOU:
		case User::File::LOAD_SAMPLE_AEROBICS_STUDIO:
			// Nothing here for now
			break;

		case User::File::LOAD_BATTERY: // load in battery data from a file
		case User::File::LOAD_EEPROM: // used by some Bandai games, can be treated the same as battery files
		case User::File::LOAD_TAPE: // for loading Famicom cassette tapes
		case User::File::LOAD_TURBOFILE: // for loading turbofile data
		{		
			std::ifstream batteryFile(self.nstpaths.savename, std::ifstream::in|std::ifstream::binary);
			
			if (batteryFile.is_open()) { file.SetContent(batteryFile); }
			break;
		}
		
		case User::File::SAVE_BATTERY: // save battery data to a file
		case User::File::SAVE_EEPROM: // can be treated the same as battery files
		case User::File::SAVE_TAPE: // for saving Famicom cassette tapes
		case User::File::SAVE_TURBOFILE: // for saving turbofile data
		{
			std::ofstream batteryFile(nstpaths.savename, std::ifstream::out|std::ifstream::binary);
			const void* savedata;
			unsigned long savedatasize;

			file.GetContent(savedata, savedatasize);

			if (batteryFile.is_open()) { batteryFile.write((const char*) savedata, savedatasize); }

			break;
		}

		case User::File::LOAD_FDS: // for loading modified Famicom Disk System files
		{
			char fdsname[512];

			snprintf(fdsname, sizeof(fdsname), "%s.ups", nstpaths.fdssave);
			
			std::ifstream batteryFile( fdsname, std::ifstream::in|std::ifstream::binary );

			// no ups, look for ips
			if (!batteryFile.is_open())
			{
				snprintf(fdsname, sizeof(fdsname), "%s.ips", nstpaths.fdssave);

				std::ifstream batteryFile( fdsname, std::ifstream::in|std::ifstream::binary );

				if (!batteryFile.is_open())
				{
					return;
				}

				file.SetPatchContent(batteryFile);
				return;
			}

			file.SetPatchContent(batteryFile);
			break;
		}

		case User::File::SAVE_FDS: // for saving modified Famicom Disk System files
		{
			char fdsname[512];

			snprintf(fdsname, sizeof(fdsname), "%s.ups", nstpaths.fdssave);

			std::ofstream fdsFile( fdsname, std::ifstream::out|std::ifstream::binary );

			if (fdsFile.is_open())
				file.GetPatchContent( User::File::PATCH_UPS, fdsFile );

			break;
		}
	}
}
*/

SDL_Event grpcNESEmulator::input_translate_string(char *string) {
	// Translate an inputcode to an SDL_Event
	SDL_Event event;
	
	int type, which, axis, value;
	
	if ((unsigned char)string[2] == 0x61) { // Axis
		which = string[1] - '0';
		axis = string[3] - '0';
		value = string[4] - '0';
		event.type = SDL_JOYAXISMOTION;
		event.jaxis.which = which;
		event.jaxis.axis = axis;
		event.jaxis.value = value;
	}
	else if ((unsigned char)string[2] == 0x62) { // Button
		which = string[1] - '0';
		value = string[3] - '0';
		event.type = SDL_JOYBUTTONDOWN;
		event.jbutton.which = which;
		event.jbutton.button = value;
		
	}
	else if ((unsigned char)string[2] == 0x68) { // Hat
		which = string[1] - '0';
		axis = string[3] - '0';
		value = string[4] - '0';
		event.type = SDL_JOYHATMOTION;
		event.jhat.which = which;
		event.jhat.hat = axis;
		event.jhat.value = value;
	}
	else {
		fprintf(stderr, "Malformed inputcode: %s\n", string);
	}
	
	return event;
}

void grpcNESEmulator::input_pulse_turbo(Input::Controllers *controllers) {
	// Pulse the turbo buttons if they're pressed
	if (turbostate.p1a) {
		turbotoggle.p1a++;
		if (turbotoggle.p1a >= conf.timing_turbopulse) {
			turbotoggle.p1a = 0;
			controllers->pad[0].buttons &= ~Input::Controllers::Pad::A;
		}
		else { controllers->pad[0].buttons |= Input::Controllers::Pad::A; }
	}
	
	if (turbostate.p1b) {
		turbotoggle.p1b++;
		if (turbotoggle.p1b >= conf.timing_turbopulse) {
			turbotoggle.p1b = 0;
			controllers->pad[0].buttons &= ~Input::Controllers::Pad::B;
		}
		else { controllers->pad[0].buttons |= Input::Controllers::Pad::B; }
	}
	
	if (turbostate.p2a) {
		turbotoggle.p2a++;
		if (turbotoggle.p2a >= conf.timing_turbopulse) {
			turbotoggle.p2a = 0;
			controllers->pad[1].buttons &= ~Input::Controllers::Pad::A;
		}
		else { controllers->pad[1].buttons |= Input::Controllers::Pad::A; }
	}
	
	if (turbostate.p2b) {
		turbotoggle.p2b++;
		if (turbotoggle.p2b >= conf.timing_turbopulse) {
			turbotoggle.p2b = 0;
			controllers->pad[1].buttons &= ~Input::Controllers::Pad::B;
		}
		else { controllers->pad[1].buttons |= Input::Controllers::Pad::B; }
	}
}
void grpcNESEmulator::input_set_default() {
	// Set default input config
	
	ui.qsave1 = SDL_GetScancodeFromName("F5");
	ui.qsave2 = SDL_GetScancodeFromName("F6");
	ui.qload1 = SDL_GetScancodeFromName("F7");
	ui.qload2 = SDL_GetScancodeFromName("F8");
	
	ui.screenshot = SDL_GetScancodeFromName("F9");
	
	ui.fdsflip = SDL_GetScancodeFromName("F3");
	ui.fdsswitch = SDL_GetScancodeFromName("F4");
	
	ui.insertcoin1 = SDL_GetScancodeFromName("F1");
	ui.insertcoin2 = SDL_GetScancodeFromName("F2");
	
	ui.reset = SDL_GetScancodeFromName("F12");
	
	ui.altspeed = SDL_GetScancodeFromName("`");
	ui.rwstart = SDL_GetScancodeFromName("Backspace");
	ui.rwstop = SDL_GetScancodeFromName("\\");
	
	ui.fullscreen = SDL_GetScancodeFromName("F");
	ui.filter = SDL_GetScancodeFromName("T");
	ui.scalefactor = SDL_GetScancodeFromName("G");
	
	player[0].u = SDL_GetScancodeFromName("Up");
	player[0].d = SDL_GetScancodeFromName("Down");
	player[0].l = SDL_GetScancodeFromName("Left");
	player[0].r = SDL_GetScancodeFromName("Right");
	player[0].select = SDL_GetScancodeFromName("Right Shift");
	player[0].start = SDL_GetScancodeFromName("Right Ctrl");
	player[0].a = SDL_GetScancodeFromName("Z");
	player[0].b = SDL_GetScancodeFromName("A");
	player[0].ta = SDL_GetScancodeFromName("X");
	player[0].tb = SDL_GetScancodeFromName("S");

	player[0].ju = input_translate_string("j0h01");
	player[0].jd = input_translate_string("j0h04");
	player[0].jl = input_translate_string("j0h08");
	player[0].jr = input_translate_string("j0h02");
	player[0].jselect = input_translate_string("j0b8");
	player[0].jstart = input_translate_string("j0b9");
	player[0].ja = input_translate_string("j0b1");
	player[0].jb = input_translate_string("j0b0");
	player[0].jta = input_translate_string("j0b2");
	player[0].jtb = input_translate_string("j0b3");
	
	player[1].u = SDL_GetScancodeFromName("I");
	player[1].d = SDL_GetScancodeFromName("K");
	player[1].l = SDL_GetScancodeFromName("J");
	player[1].r = SDL_GetScancodeFromName("L");
	player[1].select = SDL_GetScancodeFromName("Left Shift");
	player[1].start = SDL_GetScancodeFromName("Left Ctrl");
	player[1].a = SDL_GetScancodeFromName("M");
	player[1].b = SDL_GetScancodeFromName("N");
	player[1].ta = SDL_GetScancodeFromName("B");
	player[1].tb = SDL_GetScancodeFromName("V");
	
	player[1].ju = input_translate_string("j1h01");
	player[1].jd = input_translate_string("j1h04");
	player[1].jl = input_translate_string("j1h08");
	player[1].jr = input_translate_string("j1h02");
	player[1].jselect = input_translate_string("j1b8");
	player[1].jstart = input_translate_string("j1b9");
	player[1].ja = input_translate_string("j1b1");
	player[1].jb = input_translate_string("j1b0");
	player[1].jta = input_translate_string("j1b2");
	player[1].jtb = input_translate_string("j1b3");
}
void grpcNESEmulator::input_inject(Input::Controllers *controllers, nesinput_t input) {
	// Insert the input signal into the NES
	if (input.pressed) {
		controllers->pad[input.player].buttons |= input.nescode;
		
		if (input.turboa) { input.player == 0 ? turbostate.p1a = true : turbostate.p2a = true; }
		if (input.turbob) { input.player == 0 ? turbostate.p1b = true : turbostate.p2b = true; }
	}
	else {
		controllers->pad[input.player].buttons &= ~input.nescode;
		
		if (input.turboa) { input.player == 0 ? turbostate.p1a = false : turbostate.p2a = false; }
		if (input.turbob) { input.player == 0 ? turbostate.p1b = false : turbostate.p2b = false; }
	}
}
void grpcNESEmulator::input_match_network(Input::Controllers *controllers, boost::property_tree::ptree input) {
  // Match NES buttons to keyboard buttons
  nesinput_t nesinput;
  
  int player = 0;
  try {
    player = boost::lexical_cast<int>(input.get_child("player").get_value<std::string>());
  } catch( boost::bad_lexical_cast const& ) {
    std::cerr << "Error: player number string was not valid" << std::endl;
  }
  
  nesinput.nescode = 0x00;
  nesinput.player = player;
  nesinput.pressed = 0;
  nesinput.turboa = 0;
  nesinput.turbob = 0;
  
  controllers->pad[nesinput.player].buttons = 0;
  turbostate.p1a = false;
  turbostate.p1b = false;
  turbostate.p2a = false;
  turbostate.p2b = false;    
  
  BOOST_FOREACH(boost::property_tree::ptree::value_type &i, input.get_child("controls")) {
    std::string input_string(i.second.data());
    if (input_string == "up") {
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::UP;
    }
    if (input_string == "down") {
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::DOWN;
    }
    if (input_string == "left") {
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::LEFT;
    }
    if (input_string == "right") { // input.r
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::RIGHT;
    }
    if (input_string == "select") { // input.select
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::SELECT;
    }
    if (input_string == "start") { // input.start
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::START;
    }
    if (input_string == "a") { // input.a
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::A;
    }
    if (input_string == "b") { // input.b
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::B;
    }
    if (input_string == "turbo_a") { // input.ta
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::A;
      nesinput.turboa = 1;
    }
    if (input_string == "turbo_b") { // input.tb
      nesinput.pressed = 1;
      nesinput.nescode |= Input::Controllers::Pad::B;
      nesinput.turbob = 1;
    }
    if (input_string == "altspeed") { 
      timing_set_altspeed(); 
    }
    else { 
      timing_set_default(); 
    }
    
    // Insert Coins
    controllers->vsSystem.insertCoin = 0;
    if (input_string == "insertcoin1") { 
      controllers->vsSystem.insertCoin |= Input::Controllers::VsSystem::COIN_1; 
    }
    if (input_string == "insertcoin2") { 
      controllers->vsSystem.insertCoin |= Input::Controllers::VsSystem::COIN_2; 
    }
    
    // Process non-game events
    if (input_string == "fdsflip") { nst_flip_disk(); }
    if (input_string == "fdsswitch") { nst_switch_disk(); }
    if (input_string == "qsave1") { nst_state_quicksave(0); }
    if (input_string == "qsave2") { nst_state_quicksave(1); }
    if (input_string == "qload1") { nst_state_quickload(0); }
    if (input_string == "qload2") { nst_state_quickload(1); }
    
    // Screenshot
    //if (input_string == "screenshot") { video_screenshot(NULL); }
    
    // Reset
    if (input_string == "reset") { nst_reset(0); }
    
    // Rewinder
    if (input_string == "rwstart") { nst_set_rewind(0); }
    if (input_string == "rwstop") { nst_set_rewind(1); }
    
    // Video
    //if (input_string == "fullscreen") { video_toggle_fullscreen(); }
    //if (input_string == "filter") { video_toggle_filter(); }
    //if (input_string == "scalefactor") { video_toggle_scalefactor(); }
    
    // NSF
    if (nst_nsf) {
      Nsf nsf(emulator);
      
      if (input_string == "up") { // input.u
        nsf.PlaySong();
        video_clear_buffer();
        //video_disp_nsf();
      }
      if (input_string == "down") { // input.d
        //nsf.StopSong();
      }
      if (input_string == "left") { // input.l
        nsf.SelectPrevSong();
        video_clear_buffer();
        //video_disp_nsf();
      }
      if (input_string == "right") { // input.r
        nsf.SelectNextSong();
        video_clear_buffer();
        //video_disp_nsf();
      }
    }
    
    if (input_string == "quit") { 
      nst_schedule_quit();
    }
  }
  //if (nesinput.pressed == 0) {
  //    nesinput.nescode = 0;
  //    controllers->pad[player].buttons = 0;
  //}
  input_inject(controllers, nesinput);
}
void grpcNESEmulator::input_init() {
	// Initialize input
	
	char controller[32];
	
	for (int i = 0; i < NUMGAMEPADS; i++) {
		Input(emulator).AutoSelectController(i);
		
		switch(Input(emulator).GetConnectedController(i)) {
			case Input::UNCONNECTED:
				snprintf(controller, sizeof(controller), "%s", "Unconnected");
				break;
			case Input::PAD1:
			case Input::PAD2:
			case Input::PAD3:
			case Input::PAD4:
				snprintf(controller, sizeof(controller), "%s", "Standard Pad");
				break;
			case Input::ZAPPER:
				snprintf(controller, sizeof(controller), "%s", "Zapper");
				break;
			case Input::PADDLE:
				snprintf(controller, sizeof(controller), "%s", "Arkanoid Paddle");
				break;
			case Input::POWERPAD:
				snprintf(controller, sizeof(controller), "%s", "Power Pad");
				break;
			case Input::POWERGLOVE:
				snprintf(controller, sizeof(controller), "%s", "Power Glove");
				break;
			case Input::MOUSE:
				snprintf(controller, sizeof(controller), "%s", "Mouse");
				break;
			case Input::ROB:
				snprintf(controller, sizeof(controller), "%s", "R.O.B.");
				break;
			case Input::FAMILYTRAINER:
				snprintf(controller, sizeof(controller), "%s", "Family Trainer");
				break;
			case Input::FAMILYKEYBOARD:
				snprintf(controller, sizeof(controller), "%s", "Family Keyboard");
				break;
			case Input::SUBORKEYBOARD:
				snprintf(controller, sizeof(controller), "%s", "Subor Keyboard");
				break;
			case Input::DOREMIKKOKEYBOARD:
				snprintf(controller, sizeof(controller), "%s", "Doremikko Keyboard");
				break;
			case Input::HORITRACK:
				snprintf(controller, sizeof(controller), "%s", "Hori Track");
				break;
			case Input::PACHINKO:
				snprintf(controller, sizeof(controller), "%s", "Pachinko");
				break;
			case Input::OEKAKIDSTABLET:
				snprintf(controller, sizeof(controller), "%s", "Oeka Kids Tablet");
				break;
			case Input::KONAMIHYPERSHOT:
				snprintf(controller, sizeof(controller), "%s", "Konami Hypershot");
				break;
			case Input::BANDAIHYPERSHOT:
				snprintf(controller, sizeof(controller), "%s", "Bandai Hypershot");
				break;
			case Input::CRAZYCLIMBER:
				snprintf(controller, sizeof(controller), "%s", "Crazy Climber");
				break;
			case Input::MAHJONG:
				snprintf(controller, sizeof(controller), "%s", "Mahjong");
				break;
			case Input::EXCITINGBOXING:
				snprintf(controller, sizeof(controller), "%s", "Exciting Boxing");
				break;
			case Input::TOPRIDER:
				snprintf(controller, sizeof(controller), "%s", "Top Rider");
				break;
			case Input::POKKUNMOGURAA:
				snprintf(controller, sizeof(controller), "%s", "Pokkun Moguraa");
				break;
			case Input::PARTYTAP:
				snprintf(controller, sizeof(controller), "%s", "PartyTap");
				break;
			case Input::TURBOFILE:
				snprintf(controller, sizeof(controller), "%s", "Turbo File");
				break;
			case Input::BARCODEWORLD:
				snprintf(controller, sizeof(controller), "%s", "Barcode World");
				break;
			default:
				snprintf(controller, sizeof(controller), "%s", "Unknown");
				break;
		}
		
		fprintf(stderr, "Port %d: %s\n", i + 1, controller);
	}
	
}
void grpcNESEmulator::config_set_default() {
	
	// Video
	conf.video_filter = 0;
	conf.video_scale_factor = 1;
	conf.video_palette_mode = 0;
	conf.video_decoder = 0;
	conf.video_brightness = 0; // -100 to 100
	conf.video_saturation = 0; // -100 to 100
	conf.video_contrast = 0; // -100 to 100
	conf.video_hue = 0; // -45 to 45
	conf.video_ntsc_mode = 0;
	conf.video_xbr_corner_rounding = 0;
	conf.video_linear_filter = false;
	conf.video_tv_aspect = false;
	conf.video_unmask_overscan = false;
	conf.video_fullscreen = false;
	conf.video_stretch_aspect = false;
	conf.video_unlimited_sprites = false;
	conf.video_xbr_pixel_blending = false;
	
	// Audio
	conf.audio_api = 1;
	conf.audio_stereo = false;
	conf.audio_sample_rate = 44100;
	conf.audio_volume = 85;
	conf.audio_vol_sq1 = 85;
	conf.audio_vol_sq2 = 85;
	conf.audio_vol_tri = 85;
	conf.audio_vol_noise = 85;
	conf.audio_vol_dpcm = 85;
	conf.audio_vol_fds = 85;
	conf.audio_vol_mmc5 = 85;
	conf.audio_vol_vrc6 = 85;
	conf.audio_vol_vrc7 = 85;
	conf.audio_vol_n163 = 85;
	conf.audio_vol_s5b = 85;
	
	// Timing
	conf.timing_speed = 6000;
	conf.timing_altspeed = 18000;
	conf.timing_turbopulse = 3;
	conf.timing_vsync = true;
	conf.timing_limiter = true;
	
	// Misc
	conf.misc_default_system = 0;
	conf.misc_soft_patching = true;
	//conf.misc_suppress_screensaver = true;
	conf.misc_genie_distortion = false;
	conf.misc_disable_gui = false;
	conf.misc_config_pause = false;
}
void grpcNESEmulator::audio_play() {
	
	bufsize = 2 * channels * (conf.audio_sample_rate / framerate);
	
	if (conf.audio_api == 0) { // SDL
		#if SDL_VERSION_ATLEAST(2,0,4)
		SDL_QueueAudio(dev, (const void*)audiobuf, bufsize);
		// Clear the audio queue arbitrarily to avoid it backing up too far
		if (SDL_GetQueuedAudioSize(dev) > (Uint32)(bufsize * 3)) { SDL_ClearQueuedAudio(dev); }
		#endif
	}
	else if (conf.audio_api == 1) { // libao
		ao_play(aodevice, (char*)audiobuf, bufsize);
	}
	updateok = true;
}

void grpcNESEmulator::audio_init() {
	// Initialize audio device
	
	// Set the framerate based on the region. For PAL: (60 / 6) * 5 = 50
	framerate = nst_pal ? (conf.timing_speed / 6) * 5 : conf.timing_speed;
	
	channels = conf.audio_stereo ? 2 : 1;
	
	memset(audiobuf, 0, sizeof(audiobuf));
	
	#if SDL_VERSION_ATLEAST(2,0,4)
	#else // Force libao if SDL lib is not modern enough
	if (conf.audio_api == 0) {
		conf.audio_api = 1;
		fprintf(stderr, "Audio: Forcing libao\n");
	}
	#endif
	
	if (conf.audio_api == 0) { // SDL
		spec.freq = conf.audio_sample_rate;
		spec.format = AUDIO_S16SYS;
		spec.channels = channels;
		spec.silence = 0;
		spec.samples = 512;
		spec.userdata = 0;
		spec.callback = NULL; // Use SDL_QueueAudio instead
		
		dev = SDL_OpenAudioDevice(NULL, 0, &spec, &obtained, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (!dev) {
			fprintf(stderr, "Error opening audio device.\n");
		}
		else {
			fprintf(stderr, "Audio: SDL - %dHz %d-bit, %d channel(s)\n", spec.freq, 16, spec.channels);
		}
		
		SDL_PauseAudioDevice(dev, 1);  // Setting to 0 unpauses
	}
	else if (conf.audio_api == 1) { // libao
		ao_initialize();
		
		int default_driver = ao_default_driver_id();
		
		memset(&format, 0, sizeof(format));
		format.bits = 16;
		format.channels = channels;
		format.rate = conf.audio_sample_rate;
		format.byte_format = AO_FMT_NATIVE;
		
		aodevice = ao_open_live(default_driver, &format, NULL);
		if (aodevice == NULL) {
			fprintf(stderr, "Error opening audio device.\n");
			aodevice = ao_open_live(ao_driver_id("null"), &format, NULL);
		}
		else {
			fprintf(stderr, "Audio: libao - %dHz, %d-bit, %d channel(s)\n", format.rate, format.bits, format.channels);
		}
	}
}

void grpcNESEmulator::audio_deinit() {
	// Deinitialize audio
	
	if (conf.audio_api == 0) { // SDL
		SDL_CloseAudioDevice(dev);
	}
	else if (conf.audio_api == 1) { // libao
		ao_close(aodevice);
		ao_shutdown();
	}
}

void grpcNESEmulator::audio_pause() {
	// Pause the SDL audio device
	if (conf.audio_api == 0) { // SDL
		SDL_PauseAudioDevice(dev, 1);
	}
}

void grpcNESEmulator::audio_unpause() {
	// Unpause the SDL audio device
	if (conf.audio_api == 0) { // SDL
		SDL_PauseAudioDevice(dev, 0);
	}
}

void grpcNESEmulator::audio_set_params(Sound::Output *soundoutput) {
	// Set audio parameters
	Sound sound(emulator);
	
	sound.SetSampleBits(16);
	sound.SetSampleRate(conf.audio_sample_rate);
	
	sound.SetSpeaker(conf.audio_stereo ? Sound::SPEAKER_STEREO : Sound::SPEAKER_MONO);
	sound.SetSpeed(Sound::DEFAULT_SPEED);
	
	audio_adj_volume();
	
	soundoutput->samples[0] = audiobuf;
	soundoutput->length[0] = conf.audio_sample_rate / framerate;
	soundoutput->samples[1] = NULL;
	soundoutput->length[1] = 0;
}

void grpcNESEmulator::audio_adj_volume() {
	// Adjust the audio volume to the current settings
	Sound sound(emulator);
	sound.SetVolume(Sound::ALL_CHANNELS, conf.audio_volume);
	sound.SetVolume(Sound::CHANNEL_SQUARE1, conf.audio_vol_sq1);
	sound.SetVolume(Sound::CHANNEL_SQUARE2, conf.audio_vol_sq2);
	sound.SetVolume(Sound::CHANNEL_TRIANGLE, conf.audio_vol_tri);
	sound.SetVolume(Sound::CHANNEL_NOISE, conf.audio_vol_noise);
	sound.SetVolume(Sound::CHANNEL_DPCM, conf.audio_vol_dpcm);
	sound.SetVolume(Sound::CHANNEL_FDS, conf.audio_vol_fds);
	sound.SetVolume(Sound::CHANNEL_MMC5, conf.audio_vol_mmc5);
	sound.SetVolume(Sound::CHANNEL_VRC6, conf.audio_vol_vrc6);
	sound.SetVolume(Sound::CHANNEL_VRC7, conf.audio_vol_vrc7);
	sound.SetVolume(Sound::CHANNEL_N163, conf.audio_vol_n163);
	sound.SetVolume(Sound::CHANNEL_S5B, conf.audio_vol_s5b);
	
	if (conf.audio_volume == 0) { memset(audiobuf, 0, sizeof(audiobuf)); }
}

// Timing Functions

bool grpcNESEmulator::timing_frameskip() {
	// Calculate whether to skip a frame or not
	if (conf.audio_api == 0) { // SDL
		// Wait until the audio is drained
		#if SDL_VERSION_ATLEAST(2,0,4)
		while (SDL_GetQueuedAudioSize(dev) > (Uint32)bufsize) {
			if (conf.timing_limiter) { SDL_Delay(1); }
		}
		#endif
	}
	
	static int flipper = 1;
	
	if (altspeed) {
		if (flipper > 2) { flipper = 0; return false; }
		else { flipper++; return true; }
	}
	
	return false;
}

void grpcNESEmulator::timing_set_default() {
	// Set the framerate to the default
	//altspeed = false;
	//framerate = nst_pal ? (conf.timing_speed / 6) * 5 : conf.timing_speed;
	altspeed = true;
	framerate = conf.timing_altspeed;
	#if SDL_VERSION_ATLEAST(2,0,4)
	if (conf.audio_api == 0) { SDL_ClearQueuedAudio(dev); }
	#endif
}

void grpcNESEmulator::timing_set_altspeed() {
	// Set the framerate to the alternate speed
	altspeed = true;
	framerate = conf.timing_altspeed;
}
void grpcNESEmulator::video_init() {
	// Initialize video
	//opengl_cleanup();
	
	int scalefactor = conf.video_scale_factor;

  basesize.w = Video::Output::WIDTH;
  basesize.h = Video::Output::HEIGHT;
  conf.video_tv_aspect == true ? rendersize.w = TV_WIDTH * scalefactor : rendersize.w = Video::Output::WIDTH * scalefactor;
  rendersize.h = Video::Output::HEIGHT * scalefactor;

  video_set_filter();
	
	//opengl_init_structures();
	
	if (nst_nsf) { 
    video_clear_buffer(); 
    //video_disp_nsf(); 
  }
	
	//video_set_cursor();
}
void grpcNESEmulator::video_clear_buffer() {
	// Write black to the video buffer
	for (int i = 0; i < 31457280; i++) {
		videobuf[i] = 0x00000000;
	}
}

void grpcNESEmulator::video_set_filter() {
	// Set the filter
	Video video(emulator);
	int scalefactor = conf.video_scale_factor;
	
	switch(conf.video_filter) {
		case 0:	// None
			filter = Video::RenderState::FILTER_NONE;
			break;

		case 1: // NTSC
			filter = Video::RenderState::FILTER_NTSC;
			break;

		case 2: // xBR
			switch (scalefactor) {
				case 2:
					filter = Video::RenderState::FILTER_2XBR;
					break;

				case 3:
					filter = Video::RenderState::FILTER_3XBR;
					break;

				case 4:
					filter = Video::RenderState::FILTER_4XBR;
					break;

				default:
					filter = Video::RenderState::FILTER_NONE;
					break;
			}
			break;

		case 3: // scale HQx
			switch (scalefactor) {
				case 2:
					filter = Video::RenderState::FILTER_HQ2X;
					break;

				case 3:
					filter = Video::RenderState::FILTER_HQ3X;
					break;

				case 4:
					filter = Video::RenderState::FILTER_HQ4X;
					break;

				default:
					filter = Video::RenderState::FILTER_NONE;
					break;
			}
			break;
		
		case 4: // 2xSaI
			filter = Video::RenderState::FILTER_2XSAI;
			break;

		case 5: // scale x
			switch (scalefactor) {
				case 2:
					filter = Video::RenderState::FILTER_SCALE2X;
					break;

				case 3:
					filter = Video::RenderState::FILTER_SCALE3X;
					break;

				default:
					filter = Video::RenderState::FILTER_NONE;
					break;
			}
			break;
		break;
	}
	
	// Set the sprite limit:  false = enable sprite limit, true = disable sprite limit
	video.EnableUnlimSprites(conf.video_unlimited_sprites ? true : false);
	
	// Set Palette options
	switch (conf.video_palette_mode) {
		case 0: // YUV
			video.GetPalette().SetMode(Video::Palette::MODE_YUV);
			break;
		
		case 1: // RGB
			video.GetPalette().SetMode(Video::Palette::MODE_RGB);
	}
	
	// Set YUV Decoder/Picture options
	if (video.GetPalette().GetMode() != Video::Palette::MODE_RGB) {
		switch (conf.video_decoder) {
			case 0: // Consumer
				video.SetDecoder(Video::DECODER_CONSUMER);
				break;
			
			case 1: // Canonical
				video.SetDecoder(Video::DECODER_CANONICAL);
				break;
			
			case 2: // Alternative (Canonical with yellow boost)
				video.SetDecoder(Video::DECODER_ALTERNATIVE);
				break;
			
			default: break;
		}
		
		video.SetBrightness(conf.video_brightness);
		video.SetSaturation(conf.video_saturation);
		video.SetContrast(conf.video_contrast);
		video.SetHue(conf.video_hue);
	}
	
	// Set NTSC options
	switch (conf.video_ntsc_mode) {
		case 0:	// Composite
			video.SetSharpness(Video::DEFAULT_SHARPNESS_COMP);
			video.SetColorResolution(Video::DEFAULT_COLOR_RESOLUTION_COMP);
			video.SetColorBleed(Video::DEFAULT_COLOR_BLEED_COMP);
			video.SetColorArtifacts(Video::DEFAULT_COLOR_ARTIFACTS_COMP);
			video.SetColorFringing(Video::DEFAULT_COLOR_FRINGING_COMP);
			break;
		
		case 1:	// S-Video
			video.SetSharpness(Video::DEFAULT_SHARPNESS_SVIDEO);
			video.SetColorResolution(Video::DEFAULT_COLOR_RESOLUTION_SVIDEO);
			video.SetColorBleed(Video::DEFAULT_COLOR_BLEED_SVIDEO);
			video.SetColorArtifacts(Video::DEFAULT_COLOR_ARTIFACTS_SVIDEO);
			video.SetColorFringing(Video::DEFAULT_COLOR_FRINGING_SVIDEO);
			break;
		
		case 2:	// RGB
			video.SetSharpness(Video::DEFAULT_SHARPNESS_RGB);
			video.SetColorResolution(Video::DEFAULT_COLOR_RESOLUTION_RGB);
			video.SetColorBleed(Video::DEFAULT_COLOR_BLEED_RGB);
			video.SetColorArtifacts(Video::DEFAULT_COLOR_ARTIFACTS_RGB);
			video.SetColorFringing(Video::DEFAULT_COLOR_FRINGING_RGB);
			break;
		
		default: break;
	}
	
	// Set xBR options
	if (conf.video_filter == 2) {
		video.SetCornerRounding(conf.video_xbr_corner_rounding);
		video.SetBlend(conf.video_xbr_pixel_blending);
	}
	
	// Set up the render state parameters
	renderstate.filter = filter;
	renderstate.width = basesize.w;
	renderstate.height = basesize.h;
	renderstate.bits.count = 32;
	
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	renderstate.bits.mask.r = 0x000000ff;
	renderstate.bits.mask.g = 0xff000000;
	renderstate.bits.mask.b = 0x00ff0000;
	#else
	renderstate.bits.mask.r = 0x00ff0000;
	renderstate.bits.mask.g = 0x0000ff00;
	renderstate.bits.mask.b = 0x000000ff;
	#endif

	if (NES_FAILED(video.SetRenderState(renderstate))) {
		fprintf(stderr, "Nestopia core rejected render state\n");
		exit(1);
	}
}

// *******************
// emulation callbacks
// *******************


long grpcNESEmulator::video_lock_screen(void*& ptr) {
	ptr = videobuf;
	return Video::Output::WIDTH * 4;
}

void grpcNESEmulator::nst_unload() {
	// Remove the cartridge and shut down the NES
	Machine machine(emulator);
	
	if (!loaded) { return; }
	
	// Power down the NES
	fprintf(stderr, "\rEmulation stopped\n");
	machine.Power(false);

	// Remove the cartridge
	machine.Unload();
}

void grpcNESEmulator::nst_pause() {
	// Pauses the game
	if (playing) {
		audio_pause();
		audio_deinit();
	}
	
	playing = false;
	//video_set_cursor();
}

void grpcNESEmulator::nst_fds_info() {
	Fds fds(emulator);

	char* disk;
	char* side;

	fds.GetCurrentDisk() == 0 ? disk = "1" : disk = "2";
	fds.GetCurrentDiskSide() == 0 ? side = "A" : side = "B";

	fprintf(stderr, "Fds: Disk %s Side %s\n", disk, side);
	//snprintf(textbuf, sizeof(textbuf), "Disk %s Side %s", disk, side); drawtext = 120;
}

void grpcNESEmulator::nst_flip_disk() {
	// Flips the FDS disk
	Fds fds(emulator);

	if (fds.CanChangeDiskSide()) {
		fds.ChangeSide();
		nst_fds_info();
	}
}

void grpcNESEmulator::nst_switch_disk() {
	// Switches the FDS disk in multi-disk games
	Fds fds(emulator);
	
	int currentdisk = fds.GetCurrentDisk();
	
	// If it's a multi-disk game, eject and insert the other disk
	if (fds.GetNumDisks() > 1) {
		fds.EjectDisk();
		fds.InsertDisk(!currentdisk, 0);
		nst_fds_info();
	}
}

Machine::FavoredSystem grpcNESEmulator::nst_default_system() {
	switch (conf.misc_default_system) {
		case 0:
			return Machine::FAVORED_NES_NTSC;
			break;

		case 1:
			return Machine::FAVORED_NES_PAL;
			break;

		case 2:
			return Machine::FAVORED_FAMICOM;
			break;

		case 3:
			return Machine::FAVORED_DENDY;
			break;
	}

	return Machine::FAVORED_NES_NTSC;
}

void grpcNESEmulator::nst_dipswitch() {
	// Print DIP switch information and call handler
	DipSwitches dipswitches(emulator);
		
	int numdips = dipswitches.NumDips();
	
	if (numdips > 0) {
		for (int i = 0; i < numdips; i++) {
			fprintf(stderr, "%d: %s\n", i, dipswitches.GetDipName(i));
			int numvalues = dipswitches.NumValues(i);
			
			for (int j = 0; j < numvalues; j++) {
				fprintf(stderr, " %d: %s\n", j, dipswitches.GetValueName(i, j));
			}
		}
		//dip_handle();
	}
}

void grpcNESEmulator::nst_state_save(char *filename) {
	// Save a state by filename
	Machine machine(emulator);
	
	std::ofstream statefile(filename, std::ifstream::out|std::ifstream::binary);
	
	if (statefile.is_open()) { machine.SaveState(statefile, Nes::Api::Machine::NO_COMPRESSION); }
	fprintf(stderr, "State Saved: %s\n", filename);
	//snprintf(textbuf, sizeof(textbuf), "State Saved."); drawtext = 120;
}

void grpcNESEmulator::nst_state_load(char *filename) {
	// Load a state by filename
	Machine machine(emulator);
	
	std::ifstream statefile(filename, std::ifstream::in|std::ifstream::binary);
	
	if (statefile.is_open()) { machine.LoadState(statefile); }
	fprintf(stderr, "State Loaded: %s\n", filename);
	//snprintf(textbuf, sizeof(textbuf), "State Loaded."); drawtext = 120; 
}

void grpcNESEmulator::nst_state_quicksave(int slot) {
	// Quick Save State
	char slotpath[520];
	snprintf(slotpath, sizeof(slotpath), "%s_%d.nst", nstpaths.statepath, slot);
	nst_state_save(slotpath);
}


void grpcNESEmulator::nst_state_quickload(int slot) {
	// Quick Load State
	char slotpath[520];
	snprintf(slotpath, sizeof(slotpath), "%s_%d.nst", nstpaths.statepath, slot);
		
	struct stat qloadstat;
	if (stat(slotpath, &qloadstat) == -1) {
		//fprintf(stderr, "No State to Load\n"); drawtext = 120;
		//snprintf(textbuf, sizeof(textbuf), "No State to Load.");
		return;
	}
	
	nst_state_load(slotpath);
}

void grpcNESEmulator::nst_movie_save(char *filename) {
	// Save/Record a movie
	Movie movie(emulator);
	
	movierecfile = new std::fstream(filename, std::ifstream::out|std::ifstream::binary); 

	if (movierecfile->is_open()) {
		movie.Record((std::iostream&)*movierecfile, Nes::Api::Movie::CLEAN);
	}
	else {
		delete movierecfile;
		movierecfile = NULL;
	}
}

void grpcNESEmulator::nst_movie_load(char *filename) {
	// Load and play a movie
	Movie movie(emulator);
	
	moviefile = new std::ifstream(filename, std::ifstream::in|std::ifstream::binary); 

	if (moviefile->is_open()) {
		movie.Play(*moviefile);
	}
	else {
		delete moviefile;
		moviefile = NULL;
	}
}

void grpcNESEmulator::nst_movie_stop() {
	// Stop any movie that is playing or recording
	Movie movie(emulator);
	
	if (movie.IsPlaying() || movie.IsRecording()) {
		movie.Stop();
		movierecfile = NULL;
		delete movierecfile;
		moviefile = NULL;
		delete moviefile;
	}
}

void grpcNESEmulator::nst_play() {
	// Play the game
	if (playing || !loaded) { return; }
	
	video_init();
	audio_init();
	input_init();
	//cheats_init();
	
	cNstVideo = new Video::Output;
	cNstSound = new Sound::Output;
	cNstPads  = new Input::Controllers;
	
	audio_set_params(cNstSound);
	audio_unpause();
	
	if (nst_nsf) {
		Nsf nsf(emulator);
		nsf.PlaySong();
		//video_disp_nsf();
	}
	
	updateok = false;
	playing = true;
}

void grpcNESEmulator::nst_reset(bool hardreset) {
	// Reset the machine (soft or hard)
	Machine machine(emulator);
	Fds fds(emulator);
	machine.Reset(hardreset);
	
	// Set the FDS disk to defaults
	fds.EjectDisk();
	fds.InsertDisk(0, 0);
}

void grpcNESEmulator::nst_schedule_quit() {
	nst_quit = 1;
}

void grpcNESEmulator::nst_set_dirs() {
	// create system directory if it doesn't exist
	snprintf(nstpaths.nstdir, sizeof(nstpaths.nstdir), "%s/.nestopia/", getenv("HOME"));
	if (mkdir(nstpaths.nstdir, 0755) && errno != EEXIST) {	
		fprintf(stderr, "Failed to create %s: %d\n", nstpaths.nstdir, errno);
	}
	// create save and state directories if they don't exist
	char dirstr[256];
	snprintf(dirstr, sizeof(dirstr), "%ssave", nstpaths.nstdir);
	if (mkdir(dirstr, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", dirstr, errno);
	}

	snprintf(dirstr, sizeof(dirstr), "%sstate", nstpaths.nstdir);
	if (mkdir(dirstr, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", dirstr, errno);
	}
	
	// create cheats directory if it doesn't exist
	snprintf(dirstr, sizeof(dirstr), "%scheats", nstpaths.nstdir);
	if (mkdir(dirstr, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", dirstr, errno);
	}
	
	// create screenshots directory if it doesn't exist
	snprintf(dirstr, sizeof(dirstr), "%sscreenshots", nstpaths.nstdir);
	if (mkdir(dirstr, 0755) && errno != EEXIST) {
		fprintf(stderr, "Failed to create %s: %d\n", dirstr, errno);
	}
}

void grpcNESEmulator::nst_set_region() {
	// Set the region
	Machine machine(emulator);
	Cartridge::Database database(emulator);
	//Cartridge::Profile profile;
	
	if (database.IsLoaded()) {
		//std::ifstream dbfile(filename, std::ios::in|std::ios::binary);
		//Cartridge::ReadInes(dbfile, nst_default_system(), profile);
		//dbentry = database.FindEntry(profile.hash, nst_default_system());
		
		machine.SetMode(machine.GetDesiredMode());
		
		if (machine.GetMode() == Machine::PAL) {
			fprintf(stderr, "Region: PAL\n");
			nst_pal = true;
		}
		else {
			fprintf(stderr, "Region: NTSC\n");
			nst_pal = false;
		}
		//printf("Mapper: %d\n", dbentry.GetMapper());
	}
}

void grpcNESEmulator::nst_set_rewind(int direction) {
	// Set the rewinder backward or forward
	switch (direction) {
		case 0:
			Rewinder(emulator).SetDirection(Rewinder::BACKWARD);
			break;
			
		case 1:
			Rewinder(emulator).SetDirection(Rewinder::FORWARD);
			break;
			
		default: break;
	}
}

void grpcNESEmulator::nst_set_paths(const char *filename) {
	
	// Set up the save directory
	snprintf(nstpaths.savedir, sizeof(nstpaths.savedir), "%ssave/", nstpaths.nstdir);
	
	// Copy the full file path to the savename variable
	snprintf(nstpaths.savename, sizeof(nstpaths.savename), "%s", filename);
	
	// strip the . and extention off the filename for saving
	for (int i = strlen(nstpaths.savename)-1; i > 0; i--) {
		if (nstpaths.savename[i] == '.') {
			nstpaths.savename[i] = '\0';
			break;
		}
	}
	
	// Get the name of the game minus file path and extension
	snprintf(nstpaths.gamename, sizeof(nstpaths.gamename), "%s", basename(nstpaths.savename));
	
	// Construct save path
	snprintf(nstpaths.savename, sizeof(nstpaths.savename), "%s%s%s", nstpaths.savedir, nstpaths.gamename, ".sav");

	// Construct path for FDS save patches
	snprintf(nstpaths.fdssave, sizeof(nstpaths.fdssave), "%s%s", nstpaths.savedir, nstpaths.gamename);
	
	// Construct the save state path
	snprintf(nstpaths.statepath, sizeof(nstpaths.statepath), "%sstate/%s", nstpaths.nstdir, nstpaths.gamename);
	
	// Construct the cheat path
	snprintf(nstpaths.cheatpath, sizeof(nstpaths.cheatpath), "%scheats/%s.xml", nstpaths.nstdir, nstpaths.gamename);
}

bool grpcNESEmulator::nst_archive_checkext(const char *filename) {
	// Check if the file extension is valid
	int len = strlen(filename);

	if ((!strcasecmp(&filename[len-4], ".nes")) ||
	    (!strcasecmp(&filename[len-4], ".fds")) ||
	    (!strcasecmp(&filename[len-4], ".nsf")) ||
	    (!strcasecmp(&filename[len-4], ".unf")) ||
	    (!strcasecmp(&filename[len-5], ".unif"))||
	    (!strcasecmp(&filename[len-4], ".xml"))) {
		return true;
	}
	return false;
}

bool grpcNESEmulator::nst_archive_handle(const char *filename, char **rom, int *romsize, const char *reqfile) {
	// Handle archives
	struct archive *a;
	struct archive_entry *entry;
	int r;
	int64_t entrysize;
	
	a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	r = archive_read_open_filename(a, filename, 10240);
	
	// Test if it's actually an archive
	if (r != ARCHIVE_OK) {
		r = archive_read_free(a);
		return false;
	}
	
	// Scan through the archive for files
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
		char *rombuf;
		const char *currentfile = archive_entry_pathname(entry);
		
		if (nst_archive_checkext(currentfile)) {
			nst_set_paths(currentfile);
			
			// If there's a specific file we want, load it
			if (reqfile != NULL) {
				if (!strcmp(currentfile, reqfile)) {
					entrysize = archive_entry_size(entry);
					rombuf = (char*)malloc(entrysize);
					archive_read_data(a, rombuf, entrysize);
					archive_read_data_skip(a);
					r = archive_read_free(a);
					*romsize = entrysize;
					*rom = rombuf;
					return true;
				}
			}
			// Otherwise just take the first file in the archive
			else {
				entrysize = archive_entry_size(entry);
				rombuf = (char*)malloc(entrysize);
				archive_read_data(a, rombuf, entrysize);
				archive_read_data_skip(a);
				r = archive_read_free(a);
				*romsize = entrysize;
				*rom = rombuf;
				return true;
			}
		}
	}
	return false;
}

bool grpcNESEmulator::nst_find_patch(char *filename) {
	// Check for a patch in the same directory as the game
	FILE *file;
	
	if (!conf.misc_soft_patching) {
		return 0;
	}
	
	snprintf(filename, sizeof(nstpaths.savename), "%s.ips", nstpaths.gamename);
	
	if ((file = fopen(filename, "rb")) != NULL) {
		fclose(file);
		return 1;
	}
	else {
		snprintf(filename, sizeof(nstpaths.savename), "%s.ups", nstpaths.gamename);
		
		if ((file = fopen(filename, "rb")) != NULL) {
			fclose(file);
			return 1;
		}
	}
	
	return 0;
}

void grpcNESEmulator::nst_load_db() {
	Nes::Api::Cartridge::Database database(emulator);
	char dbpath[512];

	if (nstdb) { return; }

	// Try to open the database file
	snprintf(dbpath, sizeof(dbpath), "%sNstDatabase.xml", nstpaths.nstdir);
	nstdb = new std::ifstream(dbpath, std::ifstream::in|std::ifstream::binary);
	
	if (nstdb->is_open()) {
		database.Load(*nstdb);
		database.Enable(true);
		return;
	}
	// If it fails, try looking in the data directory
	snprintf(dbpath, sizeof(dbpath), "%s/NstDatabase.xml", DATADIR);
	nstdb = new std::ifstream(dbpath, std::ifstream::in|std::ifstream::binary);
	
	if (nstdb->is_open()) {
		database.Load(*nstdb);
		database.Enable(true);
		return;
	}
	
	// If that fails, try looking in the working directory
	char *pwd = getenv("PWD");
	snprintf(dbpath, sizeof(dbpath), "%s/NstDatabase.xml", pwd);
	nstdb = new std::ifstream(dbpath, std::ifstream::in|std::ifstream::binary);
	
	if (nstdb->is_open()) {
		database.Load(*nstdb);
		database.Enable(true);
		return;
	}
	else {
		fprintf(stderr, "NstDatabase.xml not found!\n");
		delete nstdb;
		nstdb = NULL;
	}
}

void grpcNESEmulator::nst_load_fds_bios() {
	// Load the Famicom Disk System BIOS
	Nes::Api::Fds fds(emulator);
	char biospath[512];
	
	if (fdsbios) { return; }

	snprintf(biospath, sizeof(biospath), "%sdisksys.rom", nstpaths.nstdir);

	fdsbios = new std::ifstream(biospath, std::ifstream::in|std::ifstream::binary);

	if (fdsbios->is_open())	{
		fds.SetBIOS(fdsbios);
	}
	else {
		fprintf(stderr, "%s not found, Disk System games will not work.\n", biospath);
		delete fdsbios;
		fdsbios = NULL;
	}
}

void grpcNESEmulator::nst_load(const char *filename) {
	// Load a Game ROM
	Machine machine(emulator);
	Nsf nsf(emulator);
	Sound sound(emulator);
	Nes::Result result;
	char *rom;
	int romsize;
	char patchname[512];
	
	// Pause play before pulling out a cartridge
	if (playing) { nst_pause(); }
	
	// Pull out any inserted cartridges
	nst_unload(); 
  //drawtime = false;
	
	// Handle the file as an archive if it is one
	if (nst_archive_handle(filename, &rom, &romsize, NULL)) {
		// Convert the malloc'd char* to an istream
		std::string rombuf(rom, romsize);
		std::istringstream file(rombuf);
		result = machine.Load(file, nst_default_system());
	}
	// Otherwise just load the file
	else {
		std::ifstream file(filename, std::ios::in|std::ios::binary);
		
		// Set the file paths
		nst_set_paths(filename);
		
		if (nst_find_patch(patchname)) { // Load with a patch if there is one
			std::ifstream pfile(patchname, std::ios::in|std::ios::binary);
			Machine::Patch patch(pfile, false);
			result = machine.Load(file, nst_default_system(), patch);
		}
		else { result = machine.Load(file, nst_default_system()); }
	}
	
	if (NES_FAILED(result)) {
		char errorstring[32];
		switch (result) {
			case Nes::RESULT_ERR_INVALID_FILE:
				snprintf(errorstring, sizeof(errorstring), "Error: Invalid file");
				break;

			case Nes::RESULT_ERR_OUT_OF_MEMORY:
				snprintf(errorstring, sizeof(errorstring), "Error: Out of Memory");
				break;

			case Nes::RESULT_ERR_CORRUPT_FILE:
				snprintf(errorstring, sizeof(errorstring), "Error: Corrupt or Missing File");
				break;

			case Nes::RESULT_ERR_UNSUPPORTED_MAPPER:
				snprintf(errorstring, sizeof(errorstring), "Error: Unsupported Mapper");
				break;

			case Nes::RESULT_ERR_MISSING_BIOS:
				snprintf(errorstring, sizeof(errorstring), "Error: Missing Fds BIOS");
				break;

			default:
				snprintf(errorstring, sizeof(errorstring), "Error: %d", result);
				break;
		}
		
		fprintf(stderr, "%s\n", errorstring);
		
		return;
	}
	
	// Deal with any DIP Switches
	nst_dipswitch();
	
	// Set the region
	nst_set_region();
	
	if (machine.Is(Machine::DISK)) {
		Fds fds(emulator);
		fds.InsertDisk(0, 0);
		nst_fds_info();
	}
	
	// Check if this is an NSF
	nst_nsf = (machine.Is(Machine::SOUND));
	if (nst_nsf) { nsf.StopSong(); }
	
	// Check if sound distortion should be enabled
	sound.SetGenie(conf.misc_genie_distortion);
	
	// note that something is loaded
	loaded = 1;
	
	// Set the title
	//video_set_title(nstpaths.gamename);

	// power on
	machine.Power(true); // false = power off
	
	nst_play();
}

int grpcNESEmulator::main(int argc, char *argv[]) {
  using namespace std::placeholders;
	// This is the main function
	
	static SDL_Event event;
	void *userData = (void*)0xDEADC0DE;

	// Set up directories
	nst_set_dirs();
	
	// Set default config options
	config_set_default();
	
	// Read the config file and override defaults
	//config_file_read();
	
	// Detect Joysticks
	//input_joysticks_detect();
	
	// Set default input keys
	input_set_default();
	
	// Read the input config file and override defaults
	//input_config_read();
	
	// Set the video dimensions
	//video_set_dimensions();
	
	// Create the window
	// Set up the callbacks
  //auto vLock = [&](void* userData, Video::Output& video) -> bool {
  //  VideoLock(userData, video);
  //  return true;
  //};
  //auto vUnlock = [&](void* userData, Video::Output& video) {
  //  VideoUnlock(userData, video);
  //};
  //auto sLock = [&](void* userData, Sound::Output& sound) -> bool {
  //  SoundLock(userData, sound);
  //  return true;
  //};
  //auto sUnlock = [&](void* userData, Sound::Output& sound) {
  //  SoundUnlock(userData, sound);
  //};
  //auto cbEvent = [&](void* userData, User::Event event, const void* data) {
  //  nst_cb_event(userData, event, data);
  //};
  //auto cbLog = [&](void* userData, const char *string, unsigned long int length) {
  //  nst_cb_log(userData, string, length);
  //}
  //auto cbFile = [&](void* userData, User::File& file) {
  //  nst_cb_file(userData, file);
  //};
  auto f = std::bind(VideoLock, this, std::placeholders::_1, std::placeholders::_2);
	Video::Output::lockCallback.Set(f, userData);
	//Video::Output::unlockCallback.Set(VideoUnlock, userData);
	//
	//Sound::Output::lockCallback.Set(SoundLock, userData);
	//Sound::Output::unlockCallback.Set(SoundUnlock, userData);
	//
	//User::fileIoCallback.Set(nst_cb_file, userData);
	//User::logCallback.Set(nst_cb_log, userData);
	//User::eventCallback.Set(nst_cb_event, userData);
	
	// Initialize and load FDS BIOS and NstDatabase.xml
	nstdb = NULL;
	fdsbios = NULL;
	nst_load_db();
	nst_load_fds_bios();

	// Load a rom from the command line
  int input_length = read(sockfd, buffer, sizeof(char) * MB);

  std::stringstream rom_stream;
  rom_stream << std::string(buffer);
  boost::property_tree::ptree init_request;
  try {
    boost::property_tree::read_json(rom_stream, init_request);
  } catch (boost::property_tree::json_parser::json_parser_error) {
    std::cerr << "Could not read json string: " << buffer << std::endl;    
    exit(1);
  }
  std::string rom_file = init_request.get_child("rom_file").get_value<std::string>();
  nst_load(rom_file.c_str());
  if (loaded) {
    boost::property_tree::ptree init_response;

    init_response.put("scale", conf.video_scale_factor);
    init_response.put("width", Video::Output::WIDTH);
    init_response.put("height", Video::Output::HEIGHT);
    std::ostringstream response_stream;
    boost::property_tree::write_json(response_stream, init_response);  
    std::string response_string = response_stream.str();
    write(sockfd, response_string.c_str(), response_string.size());
  } else {
    fprintf(stderr, "Fatal: Could not load ROM\n");
    exit(1);
  }
	// Start the main loop
	nst_quit = 0;
	
  int frame_count = 0;
	while (!nst_quit) {
    frame_count ++;

		if (playing) {
      memset(buffer, 0, input_length);
      input_length = read(sockfd, buffer, sizeof(char) * MB);
      std::stringstream ss;
      ss << std::string(buffer);
      boost::property_tree::ptree tree;
      try {
        boost::property_tree::read_json(ss, tree);
      } catch (boost::property_tree::json_parser::json_parser_error) {
        std::cerr << "Could not read json string: " << buffer << std::endl;
        exit(1);
      }
      input_match_network(cNstPads, tree);
			
			if (NES_SUCCEEDED(Rewinder(emulator).Enable(true))) {
				Rewinder(emulator).EnableSound(true);
			}
			
			audio_play();
			
			if (updateok) {
				input_pulse_turbo(cNstPads);
        emulator.Execute(cNstVideo, NULL, cNstPads); 
				// Execute a frame
				if (timing_frameskip()) {
					emulator.Execute(NULL, NULL, cNstPads);
				}
				else {
          emulator.Execute(cNstVideo, NULL, cNstPads); 
        }
        int video_scalefactor = conf.video_scale_factor;
        int frame_width = Video::Output::WIDTH * video_scalefactor;
        int frame_height = Video::Output::HEIGHT * video_scalefactor;
        int data_sent = write(sockfd, videoStream.pixels, frame_height * frame_width * 4);
			}
		}
	}
	
	// Remove the cartridge and shut down the NES
	nst_unload();
	
	// Unload the FDS BIOS and NstDatabase.xml
	if (nstdb) { delete nstdb; nstdb = NULL; }
	if (fdsbios) { delete fdsbios; fdsbios = NULL; }
	
	// Deinitialize audio
	audio_deinit();
	
	// Deinitialize joysticks
	//input_joysticks_close();
	
	// Write the input config file
	//input_config_write();
	
	// Write the config file
	//config_file_write();

	return 0;
}

int main(int argc, char *argv[]) {
  int sockfd = 0;
  int newsockfd = 0;
  socklen_t clilen = 0;
  int portno = 9090;
  struct sockaddr_in serv_addr, cli_addr;
  int  n, pid;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error opening socket");
    exit(1);
  }

  /* Initialize socket structure */
  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* Now bind the host address using bind() call.*/
  if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR on binding");
    exit(1);
  }
  // 1 MB

  while (1) {
    /* Now start listening for the clients, here
    * process will go in sleep mode and will wait
    * for the incoming connection
    */
    listen(sockfd,5);
    clilen = sizeof(cli_addr);
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }
    
    /* Create child process */
    pid = fork();
    if (pid < 0) {
        perror("ERROR on fork");
        exit(1);
    } else if (pid == 0) {
        /* This is the client process */
        grpcNESEmulator emu (newsockfd);
        emu.main(argc, argv);
        break;
    }
  } /* end of while */

}
