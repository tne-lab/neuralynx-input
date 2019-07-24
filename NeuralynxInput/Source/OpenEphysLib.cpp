/*
------------------------------------------------------------------

This file is part of a plugin for the Open Ephys GUI
Copyright (C) 2018 Translational NeuroEngineering Laboratory

------------------------------------------------------------------

We hope that this plugin will be useful to others, but its source code
and functionality are subject to a non-disclosure agreement (NDA) with
Neuralynx, Inc. If you or your institution have not signed the appropriate
NDA, STOP and do not read or execute this plugin until you have done so.
Do not share this plugin with other parties who have not signed the NDA.

*/

#include <PluginInfo.h>
#include "NeuralynxThread.h"
#include <string>
#ifdef WIN32
#include <Windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

using namespace Plugin;
#define NUM_PLUGINS 1

extern "C" EXPORT void getLibInfo(Plugin::LibraryInfo* info)
{
	info->apiVersion = PLUGIN_API_VER;
	info->name = "Neuralynx Data";
	info->libVersion = 1;
	info->numPlugins = NUM_PLUGINS;
}

extern "C" EXPORT int getPluginInfo(int index, Plugin::PluginInfo* info)
{
	switch (index)
	{
	case 0:
		info->type = Plugin::PLUGIN_TYPE_DATA_THREAD;
		info->dataThread.name = "Neuralynx Data";
		info->dataThread.creator = &createDataThread<NeuralynxThread>;
		break;
	default:
		return -1;
		break;
	}
	return 0;
}

#ifdef WIN32
BOOL WINAPI DllMain(IN HINSTANCE hDllHandle,
	IN DWORD     nReason,
	IN LPVOID    Reserved)
{
	return TRUE;
}

#endif