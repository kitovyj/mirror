#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <fstream>
#include <conio.h>
#include <thread>

#include "thread_pool.h"

class audio_t {

public:

	audio_t::audio_t(char * filename)
		: threads(1)
	{

		using namespace std;

		ok = false;
		buffer = 0;
		HInstance = GetModuleHandle(0);

		ifstream infile(filename, ios::binary);

		if (!infile)
		{
			std::cout << "audio_t::file error: " << filename << std::endl;
			return;
		}

		infile.seekg(0, ios::end);   // get length of file
		int length = infile.tellg();
		buffer = new char[length];    // allocate memory
		infile.seekg(0, ios::beg);   // position to start of file
		infile.read(buffer, length);  // read entire file

		infile.close();
		ok = true;
	}

	audio_t::~audio_t()
	{
		PlaySound(NULL, 0, 0); // STOP ANY PLAYING SOUND
		delete[] buffer;      // before deleting buffer.
	}
	void audio_t::play(bool async = true)
	{
		if (!ok)
			return;

		if (async)
			PlaySound(buffer, HInstance, SND_MEMORY | SND_ASYNC | SND_NOSTOP);
		else
			PlaySound(buffer, HInstance, SND_MEMORY | SND_NOSTOP);
	}

	// to do : make thread always running waiting for a command

	void audio_t::play_threaded()
	{
		
		
		//if (play_thread.joinable())
		//	play_thread.join();

		threads.perform(std::bind(&audio_t::play, this, false));

	}

	bool audio_t::isok()
	{
		return ok;
	}

private:
	char * buffer;
	bool ok;
	HINSTANCE HInstance;

	//std::thread play_thread;
	thread_pool threads;

};

