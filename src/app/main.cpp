#include <stdio.h>
#include <unistd.h>
#include <string>
#include <list>
#include <regex>
#include <algorithm>
#include <thread>
#include <atomic>
#include <functional>
#include <stdlib.h>
//#include "json/parse.hpp"
#include "led.hpp"
#include "snd/snd.hpp"
#include "midi/midi.hpp"

struct node_t {
	int prog;
	int patch;
	double bpm;
	int count;
	std::string pcmfile;
	std::string seqfile;
};


// patch:0 pcm:filename bpm:xxx seq:filename

std::list< node_t > parse_text(const char* fn)
{
	FILE* fp = fopen(fn, "r");
	std::list< node_t > v;
	if (fp == nullptr) {
		return v;
	}
	char buf[1024];
	std::regex pat(R"([a-z]+\:[^ ]+)");
	while (fgets(buf, 1023, fp)) {
		std::cmatch l;
		std::regex_search(buf, l, std::regex("^[^#\n]+"));
		bool found_patch = false;
		bool found_pcm = false;
		if (l.str().length() > 0) {
			node_t n;
			std::string str = l.str(); /* l.str() */
			std::sregex_iterator it = std::sregex_iterator(str.cbegin(), str.cend(), pat); // split to token (K:V)
			std::sregex_iterator end = std::sregex_iterator();
			for ( ; it != end; ++ it) {
				std::smatch kv;
				std::regex_match(it->str(), kv, std::regex(R"(([a-z]+):([^ ]+))"));
				if (kv.size() > 2) {
					std::string key = kv[1].str();
					std::string value = kv[2].str();
					printf("k:%s v:%s <= [%s] match:%d\n", key.c_str(), value.c_str(), it->str().c_str(), it->size());
					n.prog = 0;
					if (key == "patch") {
						n.patch = atoi(value.c_str());
						found_patch = true;
					}
					else if (key == "pcm") {
						n.pcmfile = value;
						found_pcm = true;
					}
					else if (key == "seq") {
						n.seqfile = value;
					}
					else if (key == "bpm") {
						n.bpm = strtod(value.c_str(), nullptr);
					}
					else if (key == "count") {
						n.count = atoi(value.c_str());
					}
				}
			}
			if (found_pcm && found_patch) 
				v.push_back(n);
		}
	}
	fclose(fp);
	return v;
}

int load_seq(const node_t& n)
{
	return 0;
}

int load_pcm(const node_t& n)
{
	return 0;
}

snd::device_t* snd_ = nullptr;
midi::device_t* midi_ = nullptr;
rpi::led_device_t* led_ = nullptr;
std::list< node_t > ents;

std::pair< bool, node_t > lookup(int pg, int patch)
{
	auto it = std::find_if(ents.begin(), ents.end(), [pg,patch](const node_t& t) { return t.prog == pg && t.patch == patch; });
	if (it == ents.end())
		return std::make_pair(false, node_t());
	return std::make_pair(true, *it);
}

void ready(rpi::led_device_t* l)
{
	printf("ready to play\n");
}

#pragma pack(1)
struct riff_chnuk_header_t {
	char tag[4];
	uint32_t size;
};

struct riff_wav_header_t {
	riff_chnuk_header_t hd;
	char wav[4];
};

struct riff_wav_format_t {
	riff_chnuk_header_t hd;
	uint16_t code;
	uint16_t ch;
	uint32_t samplerate;
	uint32_t byte_per_sec;
	uint16_t samplelength;
	uint16_t bits_per_sample;
};

struct riff_wav_data_t {
	riff_chnuk_header_t hd;
};
#pragma pack()

#define LATENCY (50) // in ms
/* 16bits stereo 48KHz で 0.5sec ぶんバッファがある場合、読み込む bytes は最小で (24000 * 4) 必要 */
#define SAMPLERATE (44100)
//#define PLAY_CHUNK ((int)((500.0/1000.0) * (SAMPLERATE)))
//#define LOAD_CHUNK ((int)((500.0/1000.0) * (SAMPLERATE)))
#define PLAY_CHUNK ((int)(((double)LATENCY/1000.0) * (SAMPLERATE)))
#define LOAD_CHUNK ((int)(10 * (SAMPLERATE)))

constexpr size_t samples_to_bytes(int chunk)
{
	return chunk << 2;
}


void command_func(const std::string& basepath, const std::string& midihw, const std::string& pcmhw)
{
	using namespace port;
	midi::midi_client cl;
	cl.init(midi_);

	int state = 0;

	volatile std::atomic< bool > loading(false);
	volatile std::atomic< bool > loadcomplete(false);
	volatile std::atomic< bool > playing(false);
	volatile std::atomic< int > pos(0);
	volatile std::atomic< int > playpos(0);
	/* FIXME: FIFO にしよう */
	uint16_t* pcm_buffer = (uint16_t*)malloc(128*1024*1024);
	auto loadfunc = [&](FILE* fp) {
		const int u = sizeof(uint16_t) * 2;
		do {
			printf("loading: %d\n", pos.load());
			size_t r = fread(pcm_buffer + pos, u, LOAD_CHUNK, fp);
			if (r == 0) {
				loadcomplete = true;
				/* 間違っても変な音が再生されないよう、終端をクリアしておく */
				memset(pcm_buffer + pos, 0, samples_to_bytes(LOAD_CHUNK));
				break;
			}
			pos += (r << 1); // stereo
		}
		while (loading);
		printf("load end at:%d (loading:%d)\n", pos, loading);
	};

	auto playfunc = [&](const uint16_t* buffer) {
		snd::reset_position(snd_);
		while (playing) {
			if (playpos < pos) {
				/* **絶対に** ノイズにならないよう、再生チャンクサイズぶんがバッファされているように確認する */
				int cnt = std::min(pos - playpos, PLAY_CHUNK);
				printf("send: playpos:%d cnt:%d pos:%d %d\n", playpos.load(), cnt, pos.load(), PLAY_CHUNK);
				snd::send_pcm(snd_, buffer + playpos, cnt);
				playpos += (cnt << 1); // stereo
			}
			else if (!loadcomplete) {
				printf("BUFFER SHORT\n");
				usleep(250000);
			}
			else {
				break; 
			}
#if !defined(__linux__)
			//usleep((LATENCY * 1000) - 10000);
#endif
		}
		printf("playback end at:%d (playing:%d)\n", playpos.load(), playing.load());
		snd::stop_sound(snd_);
	};

	
	std::thread pcm_load_thr;
	std::thread play_thr;
	FILE* fp = nullptr;
	try {
		cmd_t cmd;
		while (cl.get(&cmd) == 0) {
			switch (cmd.tag) {
			case CHANGE_PROG:
			{
				printf("CHANGE_PROG: %d\n", cmd.data0);
				int patch = cmd.data0;
				auto found = lookup(0, patch);
				if (state == 2 || !found.first) {
					/* 再生中か、 見つからなければ完全に無視 */
					break;
				}
				auto cur = found.second;
				loadcomplete = false;
				if (loading) {
					loading = false;
					pcm_load_thr.join();
				}
				if (fp) {
					fclose(fp);
				}
				
				load_pcm(cur);
				std::string pcmfilepath = basepath + "/" + cur.pcmfile;
				printf("patch:%d load file:%s\n", patch, pcmfilepath.c_str());
				fp = fopen(pcmfilepath.c_str(), "rb");
				if (fp) {
					pos = 0;
					loadfunc(fp);
					loading = true;
					pcm_load_thr = std::move(std::thread(loadfunc, fp));
				}
				else {
					perror(pcmfilepath.c_str());
					assert(false);
					exit(-1);
				}
				
				ready(led_);
				state = 1;
				break;
			}
			case TAP_CTRL:
				printf("TAP_CTRL: %d\n", cmd.data0);
				if (cmd.data0 == 0x1) {
					/*  Ctrl1 なら Start/Pause の toggle */ 
					if (state == 1) {
						printf("! START Playing\n");
						
						state = 2;
						// playback
						playpos = (sizeof(riff_wav_data_t) + sizeof(riff_wav_format_t)) >> 1;
						playing = true;
						play_thr = std::move(std::thread(playfunc, pcm_buffer));
					}
					else if (state == 2) {
						printf("! STOP Playing\n");
						// stop
						playing = false;
						if (play_thr.joinable())
							play_thr.join();
						loading = false;
						if (pcm_load_thr.joinable()) 
							pcm_load_thr.join();
						state = 1;
					}
				}
				break;
			default:
				break;
			}
		}
	}
	catch (...) {
	}

	free(pcm_buffer);
}

int dev_probe(std::string& pcmhw, std::string& midihw)
{
	FILE* cards = fopen("/proc/asound/cards", "r");
	if (!cards) {
		perror("/proc/asound/cards");
		return -1;
	}

	std::vector< std::pair< std::string, std::string > > lst;
	lst.reserve(16);
	char buf[1024];
	while (fgets(buf, 1023, cards)) {
		std::cmatch l;
		if (std::regex_search(buf, l, std::regex(R"(([0-9]+)\s*\[(\S+))"))) {
			if (l.size() > 2) {
				std::string key = l[1].str();
				std::string value = l[2].str();
				
				printf("probed [index:%s name:[%s]]\n", key.c_str(), value.c_str());
				lst.push_back(std::make_pair(key, value));
			}
		}
	}
	fclose(cards);

	/* dedicated な rawmidi があれば優先する. なければ UA25EX のを使う */
	int midi = -1;
	for (int i = 0; i < lst.size() ; ++ i) {
		if (lst[i].second == "UMONE") {
			midi = i;
			/* なぜか知らないが rawmidi は hw:NAME とか hw:2 とかでないとダメ */
			midihw = "hw:UMONE";
			break;
		}
	}
	
	int pcm = -1;
	for (int i = 0; i < lst.size() ; ++ i) {
		if (lst[i].second == "UA25EX") {
			pcm = i;
			/* pcm は plughw:NAME とか、 plughw でないとダメ */
			pcmhw = "plughw:UA25EX";
			if (midi < 0) {
				midihw = "hw:UA25EX";
				midi = i;
			}
			break;
		}
	}

	return !(pcm < 0 || midi < 0) ;
}


int main(int argc, char** argv)
{
#if defined(__linux__)
	std::string basepath(argv[2]);
	std::string midihw;
	std::string pcmhw;
	if (argc < 5) {
		if (!dev_probe(pcmhw, midihw)) {
			printf("failed to probe\n");
			exit(-1);
		}
	}
	else {
		pcmhw = std::string(argv[3]);
		midihw = std::string(argv[4]);
	}
	printf("PCM device:%s\n", pcmhw.c_str());
	printf("RAW MIDI device:%s\n", midihw.c_str());
#else
	std::string basepath(argv[2]);
	std::string devicepath(argv[2]);
	std::string mididevicepath = "default";
#endif
	snd_ = snd::init_sound(pcmhw, LATENCY * 1000, SAMPLERATE);
	//snd::final_sound(snd_);
	
	midi_ = midi::init_midi(midihw);
	led_ = rpi::init_led(rpi::LED_MODE_ONESHOT, 0);
	
	printf("load config:%s\n", argv[1]);
	//json::Parser conf;
	//auto ent = conf.parse(argv[1]);
	ents = parse_text(argv[1]);
	
	for (auto& e : ents) {
	}

	std::thread comthr([=](){command_func(basepath, midihw, pcmhw);});
	comthr.join();
	
	return 0;
}

