#include "in_gme.h"
#include "util.h"

// ------------------------------------------------------------------------------------------------
// Variables
// ------------------------------------------------------------------------------------------------
const char name[] = "MGME 0.5 beta 3";
In_Module mod =
{
	IN_VER,	name,0, 0,
	"AY\0ZX Spectrum/Amstrad CPC files (*.ay)\0"
	"GBS\0Nintendo Game Boy files (*.gbs)\0"
	"GYM\0Sega Genesis/Mega Drive files (*.gym)\0"
	"HES\0NEC TurboGrafx-16/PC Engine files (*.hes)\0"
	"KSS\0MSX Home Computer/misc. Z80 files (*.kss)\0"
    "NSF\0MSX  Nintendo NES/Famicom files (*.nsf)\0"
	"NSFE\0MSX  Nintendo NES/Famicom files (*.nsfe)\0"
	"SAP\0Atari system files(*.sap)\0"
	"SPC\0Super Nintendo/Super Famicom files (*.spc)\0"
	"VGM\0Sega VGM files(*.vgm)\0"
	"VGZ\0Sega VGM (Zipped) files (*.vgz)\0"
	"\0",1,TRUE,Config,About,Init,Quit,GetFileInfo,InfoBox,
	IsOurFile,Play,Pause,UnPause,IsPaused,Stop,GetLength,
	GetOutputTime,SetOutputTime,SetVolume,SetPan,
	0,0,0,0,0,0,0,0,0,0,0,SetEQ,NULL,0
};
int RunDecodeThread = 0;
HANDLE hDecodeThread = INVALID_HANDLE_VALUE;
long seek_needed, decode_position;
BYTE *SampleBuffer = NULL;
#define DEF_TITLE "%a (%g) - %s"

static int samplerate_tbl[] = {
	8000,
	11025,
	16000,
	22050,
	24000,
	32000,
	44100,
	48000,
	-1,
};

__declspec(dllexport) In_Module * winampGetInModule2()
{return &mod;}

//-------------------------CONFIGURATION/INIT/EXIT CODE-----------------------------------------
static int lsearch_int(int key, int *array, int num){
	int k;
	for(k = 0; num > k; ++k){
		if(key == array[k]){
			return k;
		}
	}
	return -1;
}

static BOOL WritePrivateProfileInt(LPCTSTR lpszSection, LPCTSTR lpszKey, INT dwInt, LPCTSTR lpszFile){
	char buf[32];
	wsprintf(buf, "%d", dwInt);
	return WritePrivateProfileString(lpszSection, lpszKey, buf, lpszFile);
}

static int init_cb(HWND hwndDlg, int id, int *val_tbl, int def){
	HWND hwnd;
	char buf[16];
	int k, j;
	hwnd = GetDlgItem(hwndDlg, id);
	SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
	j = -1;
	for(k = 0; 0 < val_tbl[k]; ++k){
		wsprintf(buf, "%d", val_tbl[k]);
		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)buf);
		if(val_tbl[k] == def){
			j = k;
			SendMessage(hwnd, CB_SETCURSEL, (WPARAM)k, 0);
		}
	}
	return j;
}

static int init_cb_x(HWND hwndDlg, int id, int *val_tbl, char **name_tbl, int def){
	HWND hwnd;
	int k, j;
	hwnd = GetDlgItem(hwndDlg, id);
	SendMessage(hwnd, CB_RESETCONTENT, 0, 0);
	j = -1;
	for(k = 0; NULL != name_tbl[k] ; ++k){
		SendMessage(hwnd, CB_ADDSTRING, 0, (LPARAM)name_tbl[k]);
		if(val_tbl[k] == def){
			j = k;
			SendMessage(hwnd, CB_SETCURSEL, (WPARAM)k, 0);
		}
	}
	return j;
}


void validate_setting(void){
	if(0 >= PluginOptions.samplerate){
		PluginOptions.samplerate = 44100;
	}
	if(0 >= PluginOptions.ignoresilence){
		PluginOptions.ignoresilence = 0;
	}
	if(0 >= PluginOptions.neverend){
		PluginOptions.neverend = 0;
	}
	if(0 >= PluginOptions.customtitles){
		PluginOptions.customtitles = 0;
	}
}

int save_setting(HINSTANCE hDllInstance){
	char ini_path[MAX_PATH];

	{
		char *p;
		GetModuleFileName(hDllInstance, ini_path, sizeof(ini_path));
		p = strrchr(ini_path, '\\');
		if(NULL == p){
			p = (ini_path - 1);
		}
		strcpy((p + 1), "in_mgme.ini");
	}
	WritePrivateProfileInt("in_mgme", "rate", PluginOptions.samplerate, ini_path);
	WritePrivateProfileInt("in_mgme", "ignoresilence", PluginOptions.ignoresilence, ini_path);
	WritePrivateProfileInt("in_mgme", "neverend", PluginOptions.neverend, ini_path);
	WritePrivateProfileInt("in_mgme", "customtitles", PluginOptions.customtitles, ini_path);
	WritePrivateProfileString("in_mgme", "title", PluginOptions.titlefmt, ini_path);


	//WritePrivateProfileString(NULL, NULL, NULL, ini_path);
	return 0;
}

int load_setting(HINSTANCE hDllInstance){
	char ini_path[MAX_PATH];

	{
		char *p;
		GetModuleFileName(hDllInstance, ini_path, sizeof(ini_path));
		p = strrchr(ini_path, '\\');
		if(NULL == p){
			p = (ini_path - 1);
		}
		strcpy((p + 1), "in_mgme.ini");
	}

	PluginOptions.samplerate = GetPrivateProfileInt("in_mgme", "rate", 44100, ini_path);
	PluginOptions.ignoresilence = GetPrivateProfileInt("in_mgme", "ignoresilence", 0, ini_path);
	PluginOptions.neverend = GetPrivateProfileInt("in_mgme", "neverend", 0, ini_path);
	PluginOptions.customtitles = GetPrivateProfileInt("in_mgme", "customtitles", 0, ini_path);
	GetPrivateProfileString("in_mgme", "title", DEF_TITLE, PluginOptions.titlefmt, 32, ini_path);
	validate_setting();

	return 0;
}

static void destroy(HWND hwndDlg, int apply){
	if(0 != apply){

		PluginOptions.samplerate = GetDlgItemInt(hwndDlg, IDC_SAMPLERATE, NULL, FALSE);
		PluginOptions.ignoresilence = SendDlgItemMessage(hwndDlg, IDC_IGNORESILENCE, BM_GETCHECK, 0, 0);
		PluginOptions.neverend = SendDlgItemMessage(hwndDlg, IDC_PLAYFOREVER, BM_GETCHECK, 0, 0);
        PluginOptions.customtitles = SendDlgItemMessage(hwndDlg, IDC_CUSTTITLE, BM_GETCHECK, 0, 0);
		GetDlgItemText(hwndDlg, IDC_TXTFORMAT, PluginOptions.titlefmt, 32);

		validate_setting();
	}
	EndDialog(hwndDlg, apply);
}

static int init(HWND hwndDlg){
	if(0 > init_cb(hwndDlg, IDC_SAMPLERATE, samplerate_tbl, PluginOptions.samplerate)){
		SetDlgItemInt(hwndDlg, IDC_SAMPLERATE, PluginOptions.samplerate, 0);
	}
	SendDlgItemMessage(hwndDlg, IDC_IGNORESILENCE, BM_SETCHECK, PluginOptions.ignoresilence, 0);
	SendDlgItemMessage(hwndDlg, IDC_PLAYFOREVER, BM_SETCHECK, PluginOptions.neverend, 0);
	SendDlgItemMessage(hwndDlg, IDC_CUSTTITLE, BM_SETCHECK, PluginOptions.customtitles, 0);
	SetDlgItemText(hwndDlg, IDC_TXTFORMAT, PluginOptions.titlefmt);


	return 0;
}

BOOL CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {

		case WM_COMMAND:
			wParam &= 0xFFFF;
			if(IDOK == wParam){
                PluginOptions.changed = true;
				destroy(hwnd, 1);
			}
			if(IDCANCEL == wParam){
				destroy(hwnd, 0);
			}
			break;

		case WM_SYSCOMMAND:
			if(SC_CLOSE == wParam){
				destroy(hwnd, 0);
				return TRUE;
			}
			break;

		case WM_INITDIALOG:
			init(hwnd);
			return TRUE;

	}
	return FALSE;
}


void Init()
{
    load_setting(mod.hDllInstance);
    PluginOptions.nch = 2;
    PluginOptions.bps = 16;
    PluginOptions.changed = false;
    PluginOptions.track = 0;
    PluginOptions.SampleBufLen = 0;
    PluginOptions.Paused = 0;
}

void Quit()
{
 if (PluginOptions.changed == true){
 save_setting(mod.hDllInstance);
 }
}

//------------------------------GET FILE INFO CODE----------------------------------------
void GetFileInfo(char *file, char *title, int *length_in_ms)
{
	if (file)
	{
		if (*file == 0)		// Info about the current file
		{
			if (emu)
			{
				if (title)
				{
				    char game[256];
					track_info_t info;
					gme_track_info( emu, &info, PluginOptions.track );
                    if (info.length <= 0 && info.loop_length <=0){ //NSF/HES/whatever
                    *length_in_ms = (long) (3.0 * 60 * 1000);}
                    else if (info.length <= 0 && info.loop_length  > 0) { //VGM/some NSFs
                    *length_in_ms = info.loop_length + info.intro_length;}
                    else if (info.length > 0) //SPC
                    *length_in_ms = info.length;
					if (PluginOptions.customtitles == 1)
                    {
                        title_formatting(title, PluginOptions.titlefmt,emu, file);
                    }
                    else
                    {
                        sprintf(game,"%s (%s) - %s",info.author,info.game, info.song);
                        strcpy(title, game);
                    }
                    if ((*info.author)==0) {
                    // use filename if song title is blank
                    char *p=file+strlen(file);
                    while (*p != '\\' && p >= file) p--;
                    strcpy(title,++p);
                    }
				}
			}
		}
		else				// Info about another file!
		{
			Music_Emu* inf;
			gme_open_file( file, &inf, gme_info_only );
			if (inf)
			{
				if (title)
				{
				    char game[256];
					track_info_t info2;
					gme_track_info( inf, &info2, PluginOptions.track );
                    if (info2.length <= 0 && info2.loop_length <=0) {
                    *length_in_ms = (long) (3.0 * 60 * 1000);}
                    else if (info2.length <= 0 && info2.loop_length  > 0){
                    *length_in_ms = info2.loop_length + info2.intro_length;}
                    else if (info2.length > 0) //SPC
                    *length_in_ms = info2.length;
					if (PluginOptions.customtitles == 1)
                    {
                        title_formatting(title, PluginOptions.titlefmt,inf, file);
                    }
                    else
                    {
                        sprintf(game,"%s (%s) - %s",info2.author,info2.game, info2.song);
                        strcpy(title, game);
                    }
                    if ((*info2.author)==0) {
                    char *p=file+strlen(file);
                    while (*p != '\\' && p >= file) p--;
                    strcpy(title,++p);
                    }
				}
				gme_delete(inf);
			}
		}
	}

}

BOOL CALLBACK InfoDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    char buf[256];
	switch (message)
	{
	    case WM_COMMAND:
			switch (LOWORD(wParam))
			{
                gme_track_info( trackemu, &trackinfo, PluginOptions.track );
				case IDC_PREV:
				if (PluginOptions.track > 0)
				{
                    PluginOptions.track--;
                    ChangeSubSong = TRUE;
				}
                sprintf(buf,"%d",(int)PluginOptions.track+1);
			    SendDlgItemMessage(hwndDlg, IDC_CURRTRACK, WM_SETTEXT, 0, (LPARAM)(char *) buf);
				return TRUE;

				case IDC_NEXT:
				if (PluginOptions.track < trackinfo.track_count-1)
				{
                    PluginOptions.track++;
                    ChangeSubSong = TRUE;
				}
                sprintf(buf,"%d",(int)PluginOptions.track+1);
			    SendDlgItemMessage(hwndDlg, IDC_CURRTRACK, WM_SETTEXT, 0, (LPARAM)(char *) buf);
                return TRUE;

			}
		return TRUE;

		case WM_INITDIALOG:
		{
            gme_track_info( trackemu, &trackinfo, PluginOptions.track );
			sprintf(buf,"%d",(int)trackinfo.track_count);
			SendDlgItemMessage(hwndDlg, IDC_TRACKCOUNT, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            sprintf(buf,"%d",(int)PluginOptions.track+1);
			SendDlgItemMessage(hwndDlg, IDC_CURRTRACK, WM_SETTEXT, 0, (LPARAM)(char *) buf);
			sprintf( buf,"%ld:%02ld",
            (long) trackinfo.length / 1000 / 60, (long) trackinfo.length / 1000 % 60 );
			SendDlgItemMessage(hwndDlg, IDC_LENGTH, WM_SETTEXT, 0, (LPARAM)(char *) buf);
			_snprintf(buf, sizeof(buf), "%s", trackinfo.system);
			SendDlgItemMessage(hwndDlg, IDC_SYSTEM, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            _snprintf(buf, sizeof(buf), "%s", trackinfo.game);
            SendDlgItemMessage(hwndDlg, IDC_GAME, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            _snprintf(buf, sizeof(buf), "%s", trackinfo.song);
            SendDlgItemMessage(hwndDlg, IDC_SONG, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            _snprintf(buf, sizeof(buf), "%s", trackinfo.author);
            SendDlgItemMessage(hwndDlg, IDC_AUTHOR, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            _snprintf(buf, sizeof(buf), "%s", trackinfo.copyright);
            SendDlgItemMessage(hwndDlg, IDC_COPYRIGHT, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            _snprintf(buf, sizeof(buf), "%s", trackinfo.dumper);
			SendDlgItemMessage(hwndDlg, IDC_DUMPER, WM_SETTEXT, 0, (LPARAM)(char *) buf);
			_snprintf(buf, sizeof(buf), "%s", trackinfo.comment);
            SendDlgItemMessage(hwndDlg, IDC_COMMENT, WM_SETTEXT, 0, (LPARAM)(char *) buf);
            gme_delete(trackemu);
			return TRUE;
		}
		case WM_CLOSE:
		{
			  EndDialog(hwndDlg,TRUE );
		}
	}
	return FALSE;
}

int GetLength()
{
    track_info_t infolength;
    gme_track_info( emu, &infolength, PluginOptions.track );
    if (infolength.length <= 0 && infolength.loop_length <=0) //NSF/HES/whatever
	{
        return infolength.length = (long) (3.0 * 60 * 1000);
	}
	else if ( infolength.loop_length  > 0) //VGM/some NSFs
	{
        return infolength.loop_length + infolength.intro_length;
	}
    return infolength.length;
}


//---------------------------------------DLG CODE------------------------------------------

void About(HWND hwndParent) //TODO: Make decent about box?
{
	MessageBox(hwndParent,
	"Coded by mudlord\n"
	"Uses Game_Music_Emu 0.5.2 by blargg\n"
	"Uses zlib 1.2.3 by Mark Adler and Jean-loup Gailly\n"
	"\n"
	"Thanks to: silverHIRAME, Peter Mulholland, I.S.T, franpa\n"
	"Metatron, Amir Szekely, neo_bahamut1985" , name, MB_OK);
}

int InfoBox(char *file, HWND hwndParent)
{
    gme_open_file( file, &trackemu, gme_info_only );
	DialogBoxParam(mod.hDllInstance, MAKEINTRESOURCE(IDD_INFODIALOG), hwndParent, (DLGPROC) InfoDlgProc, (LPARAM) (char *) file);
	return 0;
}

void Config(HWND hwndParent)
{
    DialogBox(mod.hDllInstance, MAKEINTRESOURCE(IDD_CONFIGDIALOG), hwndParent, (DLGPROC) ConfigDlgProc);
}

//--------------------------------PLAYBACK YHREAD AND PLAY CODE-----------------------------------------

int Play(char *fn)
{
	int maxlatency;
	DWORD thread_id;
	track_info_t info;
	gme_open_file( fn, &emu, PluginOptions.samplerate); //TODO: Add configurable sample rate
    gme_track_info( emu, &info, PluginOptions.track );
	PluginOptions.SampleBufLen =  4096;
	SampleBuffer = (BYTE *) malloc(PluginOptions.SampleBufLen*4);	// DSP may want more, suck!
	decode_position=0;
	seek_needed=-1;
	if (!SampleBuffer)
	{MessageBox(mod.hMainWindow, "Failed to malloc() sample buffer!", name, MB_OK);
    return 1;}
	maxlatency = mod.outMod->Open(PluginOptions.samplerate, PluginOptions.nch, PluginOptions.bps, -1,-1);
	if (maxlatency < 0) // error opening device
	{return 1;}

	mod.SetInfo(PluginOptions.nch, PluginOptions.samplerate/1000, 2, 1);
	mod.SAVSAInit(maxlatency, PluginOptions.samplerate);
	mod.VSASetInfo(PluginOptions.samplerate, PluginOptions.nch);
	mod.outMod->SetVolume(-666);
	PluginOptions.track = 0;
	gme_start_track( emu, PluginOptions.track );

    if ( info.length <= 0 && info.loop_length <=0)
    {
    info.length = (long) (3.0 * 60 * 1000);
    }
    else
    {
    if ( info.loop_length > 0)
    info.length = info.intro_length + info.loop_length;
    }

    if (PluginOptions.neverend != 1)
    {
    gme_set_fade(emu, info.length);
    }
    gme_ignore_silence( emu, PluginOptions.ignoresilence );

	RunDecodeThread = 1;
	hDecodeThread = (HANDLE) CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DecodeThread, NULL, 0, &thread_id);
	SetThreadPriority(hDecodeThread, THREAD_PRIORITY_ABOVE_NORMAL);
	return 0;	// Success!
}


void Stop()
{
	if (hDecodeThread != INVALID_HANDLE_VALUE)
	{
		RunDecodeThread = 0;
		if (WaitForSingleObject(hDecodeThread, 1000 * 10) == WAIT_TIMEOUT)
		{
			MessageBox(mod.hMainWindow,"Error asking playback thread to die!", name, MB_OK);
			TerminateThread(hDecodeThread,0);
		}
		CloseHandle(hDecodeThread);
		hDecodeThread = INVALID_HANDLE_VALUE;
	}
	mod.outMod->Close();
	mod.SAVSADeInit();
	gme_delete( emu );
	if (SampleBuffer)
	{
		free(SampleBuffer);
		SampleBuffer = NULL;
	}
}

void play_siren( long count, short* out )
{
 static double a, a2;
 while ( count-- )
 *out++ = 0x2000 * sin( a += .1 + .05*sin( a2+=.00005 ) );
}

DWORD WINAPI DecodeThread(void *b)
{
	int t, l;
	while (RunDecodeThread)
	{
	    if (seek_needed != -1) //we need to seek!
		{
			decode_position = seek_needed-(seek_needed%1000);
			seek_needed=-1;
			mod.outMod->Flush(decode_position); // what
			gme_seek( emu, decode_position );
		}

		if (ChangeSubSong) //change subsongs!
		{
			gme_start_track( emu, PluginOptions.track );
			ChangeSubSong = FALSE;
			mod.outMod->Flush(0);
			decode_position = 0;
		}

        if (gme_track_ended(emu) && PluginOptions.track < gme_track_count(emu) -1 ) //subsongs
        {
        PluginOptions.track++;
        gme_start_track( emu, PluginOptions.track );
        }

        if (gme_track_ended(emu)) //normal
		{
        PostMessage(mod.hMainWindow,WM_WA_MPEG_EOF,0,0);
        return 0;
		}

		if (mod.outMod->CanWrite() >= (PluginOptions.SampleBufLen << (mod.dsp_isactive() ? 1 : 0)))
		{
		    gme_play( emu, (long)PluginOptions.SampleBufLen/2, (short*)SampleBuffer );
			t=mod.outMod->GetWrittenTime();
			decode_position += 1000L/PluginOptions.samplerate;
			mod.SAAddPCMData((char *)SampleBuffer,PluginOptions.nch,PluginOptions.bps,t);
			mod.VSAAddPCMData((char *)SampleBuffer,PluginOptions.nch,PluginOptions.bps,t);
			if (mod.dsp_isactive())
                l = mod.dsp_dosamples((short *) SampleBuffer,PluginOptions.SampleBufLen/PluginOptions.nch/(PluginOptions.bps/8),PluginOptions.bps,PluginOptions.nch,PluginOptions.samplerate)*(PluginOptions.nch*(PluginOptions.bps/8));
			else
				l = PluginOptions.SampleBufLen;
			mod.outMod->Write((char*)SampleBuffer, l);
		}
		else
			Sleep(20);
	}
	return 0;
}

//--------------------SIMPLE CODE------------------------------------------------

int GetOutputTime()
{return mod.outMod->GetOutputTime();}
void SetOutputTime(int time_in_ms)
{seek_needed=time_in_ms;}
void SetVolume(int volume)
{mod.outMod->SetVolume(volume);}
void SetPan(int pan)
{mod.outMod->SetPan(pan);}
void SetEQ(int on, char data[10], int preamp){}
void Pause(){
PluginOptions.Paused=1;
mod.outMod->Pause(1);}
void UnPause(){
PluginOptions.Paused=0;
mod.outMod->Pause(0);}
int IsPaused()
{return PluginOptions.Paused;}
int IsOurFile(char *fn)
{return 0;}
