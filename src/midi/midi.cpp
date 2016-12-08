#include "midi.hpp"

namespace midi {


int midi_client::init(device_t* dev)
{
	return 0;
}

int midi_client::get(cmd_t* cmd)
{
	*cmdd = q_.wait();
	return 0;
}

int midi_client::put(cmd_t&& cmd)
{
	q_.send(std::move(cmd));
	return 0;
}

#if defined(__LINUX__)

device_t* init_midi()
{
	int err;
	snd_rawmidi_t* hdlin;
	snd_rawmidi_t* hdlout;
	err = snd_rawmidi_open(&hdlin, &hdlout, "default", 0);
	if (err < 0) {
		printf("Control midi device open error:%s\n", snd_strerror(err));
		assert(false);
		return nullptr;
	}
	auto dev = new device_t;
	dev->hdlin = hdlin;
	dev->hdlout = hdlout;
	return dev;
}

void final_midi(device_t* dev)
{
	snd_rawmidi_close(dev->hdlin);
	snd_rawmidi_close(dev->hdlout);
	dev->hdl = nullptr;
	delete dev;
}

int ch_read(device_t* dev, uint8_t* ch)
{
	snd_rawmidi_read(dev->hdlin, ch, 1); /* blocking */
	return 0;
}

int ch_write(device_t* dev, const uint8_t* ch, int cnt)
{
	snd_rawmidi_write(dev->hdlout, ch, cnt); /* blocking */
	snd_rawmidi_drain(dev->hdlout);
	return 0;

}
#endif
}
