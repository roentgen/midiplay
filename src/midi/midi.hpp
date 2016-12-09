#if !defined(MIDI_MIDI_HPP__)
#define MIDI_MIDI_HPP__

#include <stdint.h>
#include <atomic>
#include "port/cmdq.hpp"
#if defined(__linux__)
#include <asoundlib.h>
#endif
#include <thread>

namespace midi {

	struct device_t {
#if defined(__linux__)
		snd_rawmidi_t* hdlin;
		snd_rawmidi_t* hdlout;
#else
		int fdin;
		int fdout;
#endif
	};
	
	class midi_client {
		port::cmdq_t q_;
		device_t* dev_;
		std::atomic< bool > listening_;
		std::thread thr_;
	public:
		midi_client() : dev_(nullptr), listening_(false) {};
		~midi_client() {};
		
		int init(device_t* dev);
		int get(port::cmd_t* cmd);
		int put(port::cmd_t&& cmd);
	};
	
	device_t* init_midi();
	void final_midi(device_t* dev);

	int ch_read(device_t* dev, uint8_t* ch);
	int ch_write(device_t* dev, const uint8_t* ch, int cnt);

}

#endif
