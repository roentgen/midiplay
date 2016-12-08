#include "snd.hpp"

namespace snd {
#if defined(__LINUX__)
device_t* init_sound()
{
	int err;
	snd_pcm_t* hdl;
	err = snd_pcm_open(&hdl, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback snd device open error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}

	err = snd_pcm_set_params(hdl, SND_PCM_FORMAT_U16, SND_PCM_ACCESS_RW_INTERLEAVED, 2 /* ch */, 48000, 0 /* soft resample: 0=deny 1=allow */, 500000 /* required latency in us, バッファサイズと同じ？ */);
	if (err < 0) {
		printf("Playback snd device setup error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}
	
	auto dev = new device_t;
	dev->hdl = hdl;
	return dev;
}

int send_pcm(device_t* dev, uint16_t* pcm, int cnt)
{
	int frames = snd_pcm_writei(dev->hdl, pcm, cnt);
	if (frames < 0) 
		frames = snd_pcm_recover(dev->hdl, frams, 0);
	if (frames < 0) {
		printf("Playback snd device write error:%s\n", snd_strerror(frames));
		assert(false);
		return -1;
	}
	return 0;
}

void final_sound(device_t* dev)
{
	snd_pcm_close(dev->hdl);
	dev->hdl = nullptr;
	delete dev;
}
#endif
}
