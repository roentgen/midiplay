#if !defined(SND_SND_HPP__)
#define SND_SND_HPP__

#include <stdint.h>
#include <string>
#if defined(__linux__)
#include <asoundlib.h>
#endif

namespace snd {
	struct device_t {
#if defined(__linux__)
		snd_pcm_t* hdl;
#else
		int fd;
#endif
	};
	device_t* init_sound(const std::string& device, int latency, int samplerate = 44100, int bits = 16, int ch = 2);
	int send_pcm(device_t* , const uint16_t* pcm, int cnt);
	void stop_sound(device_t*);
	void final_sound(device_t* dev);
}

#endif
