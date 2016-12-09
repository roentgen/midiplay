#include <stdio.h>
#include <unistd.h>
#include "midi.hpp"
#include "port/cmdq.hpp"
#include <thread>

namespace midi {


int midi_client::init(device_t* d)
{
	dev_ = d;
	listening_ = true;
	thr_ = std::move(std::thread([this](){
				while (listening_) {
					uint8_t ch;
					ch_read(dev_, &ch);
					port::cmd_t cmd;
#if defined(__linux__)
#else
					if (ch == '0') {
						cmd.tag = port::CHANGE_PROG;
						cmd.data0 = 0;
					}
					else if (ch == '1') {
						cmd.tag = port::CHANGE_PROG;
						cmd.data0 = 1;
					}
					else if (ch == 'p') {
						cmd.tag = port::START;
					}
					else if (ch == 'b') {
						cmd.tag = port::STOP;
					}
					else {
						continue;
					}
#endif
					put(std::move(cmd));
				}
			}));
	return 0;
}

int midi_client::get(port::cmd_t* cmd)
{
	*cmd = q_.wait();
	return 0;
}

int midi_client::put(port::cmd_t&& cmd)
{
	q_.send(std::move(cmd));
	return 0;
}

device_t* init_midi()
{
#if defined(__linux__)
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
#else
	auto dev = new device_t;
	dev->fdin = 1;
	dev->fdout = 0;
	return dev;
#endif
}

void final_midi(device_t* dev)
{
#if defined(__linux__)
	snd_rawmidi_close(dev->hdlin);
	snd_rawmidi_close(dev->hdlout);
	dev->hdlin = nullptr;
	dev->hdlout = nullptr;
#endif
	delete dev;
}

int ch_read(device_t* dev, uint8_t* ch)
{
#if defined(__linux__)
	snd_rawmidi_read(dev->hdlin, ch, 1); /* blocking */
#else
	while (read(dev->fdin, ch, 1) == EAGAIN) {
		usleep(100000);
	}
#endif
	return 0;
}

int ch_write(device_t* dev, const uint8_t* ch, int cnt)
{
#if defined(__linux__)
	snd_rawmidi_write(dev->hdlout, ch, cnt); /* blocking */
	snd_rawmidi_drain(dev->hdlout);
#else
	write(dev->fdout, ch, cnt);
#endif
	return 0;

}
}
