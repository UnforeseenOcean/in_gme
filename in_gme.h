//GME WinAMP Plugin
#ifndef ___IN_GME_H_
#define ___IN_GME_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "in2.h"
#include "wa_ipc.h"
#include "math.h"
#include "resource.h"
#include "Game_Music_Emu-0.5.2\gme\gme.h"

Music_Emu *emu, *trackemu;
track_info_t trackinfo;
static BOOL ChangeSubSong = FALSE;
// ------------------------------------------------------------------------------------------------
// Function protos
// ------------------------------------------------------------------------------------------------
void Config(HWND hwndParent);
void About(HWND hwndParent);
void Init();void Quit();
void GetFileInfo(char *file, char *title, int *length_in_ms);
int InfoBox(char *file, HWND hwndParent);
int IsOurFile(char *fn);int Play(char *fn);
void Stop();void Pause();void UnPause();
int IsPaused();int GetLength();			// get length in ms
int GetOutputTime();		// returns current output time in ms. (usually returns outMod->GetOutputTime()
void SetOutputTime(int time_in_ms);	// seeks to point in stream (in ms). Usually you signal yoru thread to seek, which seeks and calls outMod->Flush()..
void SetVolume(int volume);	// from 0 to 255.. usually just call outMod->SetVolume
void SetPan(int pan);	// from -127 to 127.. usually just call outMod->SetPan
void SetEQ(int on, char data[10], int preamp);
DWORD WINAPI DecodeThread(void *b);
BOOL CALLBACK InfoDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK ConfigDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
#define WM_WA_MPEG_EOF WM_USER+2


struct options{
    int samplerate;
    int neverend;
    int fadelen;
    int ignoresilence;
    int bps;
    int nch;
    int track;
    bool changed;
    int Paused;
    int customtitles;
    char titlefmt[32];
    DWORD SampleBufLen;
} PluginOptions;



#endif


