
#include "common.h"

#pragma comment(lib, "winmm.lib")

HWAVEIN wIn;
HWAVEOUT hWaveOut = 0;
WAVEHDR      *WaveHdr;
FILE *PcmWriter	=	NULL;
int gRecorderThread = 1;
int gPlayerThread = 1;

#define LOG(format, ...) wprintf(format L"\n", __VA_ARGS__)

#define PLAYER_LATENCY	120
#define BYTES_PER_FRAME(a,b,c) (a*b*c*0.02)	//a=bit depth, b=Sampling Frequency, c=Channel Count

	
/* Internal: init WAVEFORMATEX */
static int init_pltfrm_waveformatex(LPWAVEFORMATEX wfx, int clock_rate, int channel_count)
{
	int eRet = DEV_TRUE;

	
	memset(wfx, 0, sizeof(WAVEFORMATEX));
	{
		enum { BYTES_PER_SAMPLE = 2 };
		wfx->wFormatTag = WAVE_FORMAT_PCM; 
		wfx->nChannels = channel_count;
		wfx->nSamplesPerSec = clock_rate;
		wfx->nBlockAlign = (channel_count * BYTES_PER_SAMPLE);
		wfx->nAvgBytesPerSec = clock_rate * channel_count * BYTES_PER_SAMPLE;
		wfx->wBitsPerSample = 16;
		printf("init_pltfrm_waveformatex: PCM Format Clock rate is %d, Channel count is %d and  Bytes per sample is %d\n", clock_rate,channel_count, BYTES_PER_SAMPLE);
		return eRet;
	} 
	
}

int SetVolumeAudioDevices()
{
	DWORD dVol	=	0;
	HWAVEOUT  Out;
	MMRESULT mr;

	mr = waveOutGetVolume(Out, &dVol); 
	if (mr != MMSYSERR_NOERROR) 
	{
		printf("SetVolumeAudioDevices: Windows platform error is %d in Getting Output Volume",mr);
		return DEV_FALSE;
	}
	else
	{
		/* Convert unsigned value to the required format */
		dVol &= 0xFFFF; //To get the volume value in Hexadecimal format
	}


	printf("SetVolumeAudioDevices: Windows Vol is %x \n",dVol);
	return DEV_TRUE;
}

void CALLBACK WinAudThread(HWAVEIN hwi,UINT uMsg,DWORD dwInstance,DWORD dwParam1,DWORD dwParam2)
{
	int i;
	MMRESULT mr = MMSYSERR_NOERROR;
	WAVEHDR *pHdr=NULL;
	switch(uMsg)
	{
		case WIM_CLOSE:
			break;

		case WIM_DATA:
			{
				if(gRecorderThread)
				{
					for(i=0;i<1;i++)
					{
						char *buf		=	WaveHdr[i].lpData;
						int read_len	=	WaveHdr[i].dwBytesRecorded;

						if(read_len > 0)
						{
							fwrite(buf,1,read_len,PcmWriter);
							printf("InitAudioCaptureDevice: read_len %d..\n",read_len);
							mr = waveInAddBuffer(wIn,WaveHdr,sizeof(WAVEHDR));

							if (mr != MMSYSERR_NOERROR)
							{
								printf("WinAudThread: waveInAddBuffer FAILED and returned status %d: Hence breaking from FOR loop", mr);
								break;
							}
						}
						else
						{
							printf("InitAudioCaptureDevice: ZERO...\n");
							continue;
						}

					}
				}
			}
			break;

		case WIM_OPEN:
			break;

		default:
			break;
	}
}

int	OpenFileToWrite(int fileMode)
{
	enum{APPEND_BINARY = 0, READ_BINARY};
	if(APPEND_BINARY == fileMode)
		PcmWriter	=	fopen("data.pcm","ab+");
	else if(READ_BINARY == fileMode)
		PcmWriter	=	fopen("data.pcm","rb");
	if(!PcmWriter)
	{
		printf("OpenFileToWrite: PcmWriter Open Error...\n");
		return DEV_FALSE;
	}
	return DEV_TRUE;
}
int GetCaptureDeviceID(unsigned int *CaptureDeviceID)
{
	MMRESULT mm;
	WAVEINCAPS pwic = {0};
	
	mm	=	waveInGetDevCaps(*CaptureDeviceID,&pwic,sizeof(pwic));
	if(mm != MMSYSERR_NOERROR)
	{
		printf("InitAudioCaptureDevice: waveInGetDevCaps Error...\n");
		return DEV_FALSE;
	}
	PrintDeviceInfo(*CaptureDeviceID,pwic);

	return DEV_TRUE;
}
int OpenCaptureDevice(WAVEFORMATEX *wfx)
{
	MMRESULT mm;
	unsigned int CaptureDeviceID	=	0;

	if(DEV_FALSE == GetCaptureDeviceID(&CaptureDeviceID))
	{
		return DEV_FALSE;
	}

	mm	=	waveInOpen(&wIn,CaptureDeviceID,wfx,(DWORD)WinAudThread,0,CALLBACK_FUNCTION);
	
	if(mm != MMSYSERR_NOERROR)
	{
		printf("InitAudioCaptureDevice: waveInOpen() Error...\n");
		return DEV_FALSE;
	}

	return DEV_TRUE;
}

int PrepareBuffers(int clock_rate, int channel_count, int sample_per_frame)
{
	int buf_count	=	0,i;
	MMRESULT mm;


	buf_count	=	PLAYER_LATENCY*clock_rate/channel_count/sample_per_frame/1000;
	printf("bufCount: %d\n",buf_count);
	WaveHdr	=	(WAVEHDR*)calloc(buf_count,sizeof(WAVEHDR));

	for(i=0;i<buf_count;i++)
	{
		WaveHdr[i].lpData			=	calloc(1,(2*clock_rate*channel_count)/50);
		printf("BYTES_PER_FRAME: %d\n",(2*clock_rate*channel_count)/50);
		if(!WaveHdr[i].lpData)
		{
			printf("InitAudioCaptureDevice: WaveHdr[%d].lpData...failed\n",i);
			continue;
		}
		WaveHdr[i].dwBufferLength	=	BYTES_PER_FRAME(2,clock_rate,channel_count);


		mm							=	waveInPrepareHeader(wIn,&(WaveHdr[i]),sizeof(WAVEHDR));
		if(mm != MMSYSERR_NOERROR)
		{
			printf("InitAudioCaptureDevice: waveInPrepareHeader Error...\n");
			return DEV_FALSE;
		}

		mm = waveInAddBuffer(wIn,&(WaveHdr[i]),sizeof(WAVEHDR));
		if(mm != MMSYSERR_NOERROR)
		{
			printf("InitAudioCaptureDevice: waveInAddBuffer Error...\n");
			return DEV_FALSE;
		}
		printf("InitAudioCaptureDevice: loop [%d]\n",i);
	}


	return DEV_TRUE;
}

int InitAudioCaptureDevice(int clock_rate, int channel_count, int sample_per_frame)
{
	MMRESULT mm;
	WAVEFORMATEX wfx;
	
	unsigned int CaptureDeviceID	=	0;

	
	
	int buf_count	=	0;
	int i	=	0;
	HANDLE thread = NULL;
	DWORD ThreadId,deRet;


	init_pltfrm_waveformatex(&wfx,clock_rate,channel_count);



	if(DEV_TRUE != OpenFileToWrite(0))//0 = APPEND_BINARY
	{
		return DEV_FALSE;
	}
	


	//Open Capture Device
	if(DEV_TRUE != OpenCaptureDevice(&wfx))
	{
		printf("InitAudioCaptureDevice: OpenCaptureDevice() Error...\n");
		return DEV_FALSE;
	}


	//Create Buffers
	if(DEV_TRUE != PrepareBuffers(clock_rate,channel_count,sample_per_frame))
	{
		printf("InitAudioCaptureDevice: OpenCaptureDevice() Error...\n");
		return DEV_FALSE;
	}


	mm	=	waveInStart(wIn);
	if(mm != MMSYSERR_NOERROR)
	{
		printf("InitAudioCaptureDevice: waveInStart Error...\n");
		return DEV_FALSE;
	}

	while(gRecorderThread);

	fclose(PcmWriter);

	return DEV_TRUE;
}

int PrintDeviceInfo(int dev, WAVEINCAPS caps)
{

        
        LOG(
            L"-- waveIn device #%u --\n"
            L"Manufacturer ID: %u\n"
            L"Product ID: %u\n"
            L"Version: %u.%u\n"
            L"Product Name: %s\n"
            L"Formats: 0x%x\n"
            L"Channels: %u\n"
            L"Reserved: %u\n"
            ,
            dev,
            caps.wMid,
            caps.wPid,
            caps.vDriverVersion / 256, caps.vDriverVersion % 256,
            caps.szPname,
            caps.dwFormats,
            caps.wChannels,
            caps.wReserved1
        );

}





//##############################  Player Implementation ####################################//


int InitAudioPlaybackDevice(int sampling_frequency, int channel_count, int samples_per_frame)
{
	int iCounter = 0;
	MMRESULT mm;
	WAVEFORMATEX wfx;
	int bytesRead	=	0;
	unsigned char *buffer	=	(unsigned char*)calloc(sizeof(unsigned char),(sampling_frequency*channel_count*2/50));
	if(!buffer)
	{
		return DEV_FALSE;
	}
	
	init_pltfrm_waveformatex(&wfx,sampling_frequency,channel_count);


	waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);


	if(DEV_TRUE != OpenFileToWrite(1))//1 = READ_BINARY
	{
		return DEV_FALSE;
	}

	while(gPlayerThread)
	{
		bytesRead = fread(buffer,1,(sampling_frequency*channel_count*2/50),PcmWriter);
		printf("Count: %d\n",iCounter++);
		if(bytesRead > 0)
		{
			WAVEHDR header = { (LPSTR)buffer, (sampling_frequency*channel_count*2/50), 0, 0, 0, 0, 0, 0 };
			waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
			waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));
			waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
		}
		else if (bytesRead == 0)
		{
			iCounter = 0;
			fclose(PcmWriter);
			if(DEV_TRUE != OpenFileToWrite(1))//1 = READ_BINARY
			{
				return DEV_FALSE;
			}
		}
		Sleep(20);
	}

	return DEV_TRUE;
}



//##############################  Player Implementation ####################################//


void main()
{
	//InitAudioCaptureDevice(48000,2,960);
	InitAudioPlaybackDevice(48000,2,960);
	//PlaySound(TEXT("recycle.wav"), NULL, SND_FILENAME);
	
	printf("waveInGetNumDevs() returned %d\n",waveInGetNumDevs());
	printf("waveOutGetNumDevs() returned %d\n",waveOutGetNumDevs());
	
}



