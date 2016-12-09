#if !defined(SND_SND_HPP__)
#define SND_SND_HPP__

#include <stdint.h>
#include <string>
#if defined(__LINUX__)
#include <asoundlib.h>
#endif

namespace snd {
	struct device_t {
#if defined(__LINUX__)
		snd_pcm_t* hdl;
#else
		int fd;
#endif
	};
	device_t* init_sound(const std::string& device);
	int send_pcm(device_t* , const uint16_t* pcm, int cnt);
	void stop_sound(device_t*);
	void final_sound(device_t* dev);
}

#endif
