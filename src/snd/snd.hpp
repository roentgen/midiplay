#if !defined(SND_SND_HPP__)
#define SND_SND_HPP__

#include <stdint.h>
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
	device_t* init_sound();
	int send_pcm(const uint16_t* pcm, int cnt);
	void final_sound(device_t* dev);
}

#endif
