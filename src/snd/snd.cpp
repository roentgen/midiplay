#include <stdio.h>
#include <unistd.h>
#include "snd.hpp"

#include <fcntl.h>

namespace snd {
device_t* init_sound(const std::string& device, int latency, int samplerate, int bits, int ch)
{
#if defined(__linux__)
	int err;
	snd_pcm_t* hdl;
	err = snd_pcm_open(&hdl, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Playback snd device open error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}

	err = snd_pcm_set_params(hdl, SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, ch /* ch */, samplerate, 0 /* soft resample: 0=deny 1=allow */, latency /* required latency in us, バッファサイズと同じ？ */);
	if (err < 0) {
		printf("Playback snd device setup error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}

	/* 
	   Period は一回の割り込みで処理可能なサンプル数: 551 なら 16bit stereo 44,1k の場合 2204.
	   Buffer が 50msec なら同条件で 2205 frames になるので、 4 回割り込みが起きる間 1 sample は余るが、 FIFO なので問題はない.
	 */
	snd_pcm_uframes_t buff = 0, period = 0;
	err = snd_pcm_get_params(hdl, &buff, &period);
	printf("buffer:%d period:%d\n", buff, period);

	auto dev = new device_t;
	dev->hdl = hdl;
#else
	auto dev = new device_t;
	//dev->fd = 0;
	std::string path = device + "/test_out.pcm";
	dev->fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
	if (dev->fd < 0) {
		perror(path.c_str());
	}
	printf("psudo sound file:%s fd:%d\n", path.c_str(), dev->fd);
#endif
	return dev;
}

int send_pcm(device_t* dev, const uint16_t* pcm, int cnt)
{
#if defined(__linux__)
	int frames = snd_pcm_writei(dev->hdl, pcm, cnt);
	if (frames < 0) 
		frames = snd_pcm_recover(dev->hdl, frames, 0);
	if (frames < 0) {
		printf("Playback snd device write error:%s\n", snd_strerror(frames));
		assert(false);
		return -1;
	}
#else
	for (int i = 0; i < cnt; ++ i) {
		write(dev->fd, pcm + i * 2, 2);
		write(dev->fd, pcm + i * 2 + 1, 2);
	}
#endif
	return 0;
}


void reset_position(device_t* dev)
{
	snd_pcm_reset(dev->hdl);
	snd_pcm_hwsync(dev->hdl);
	snd_pcm_prepare(dev->hdl);
}

void stop_sound(device_t* dev)
{
#if defined(__linux__)
	snd_pcm_drop(dev->hdl);
#else
	close(dev->fd);
	dev->fd = -1;
#endif
}
	
void final_sound(device_t* dev)
{
#if defined(__linux__)
	snd_pcm_close(dev->hdl);
	dev->hdl = nullptr;
#else
	if (dev->fd > 0)
		close(dev->fd);
#endif
	delete dev;
}
}
