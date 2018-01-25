/* Copyright (C) 2018 Yosshin(@yosshin4004) */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>

#include <mdx_util.h>
#include <mxdrv.h>
#include <mxdrv_context.h>

#ifdef _WIN32
	#include <SDL.h>
#else
	#include <SDL2/SDL.h>
	#include <libgen.h>		/* for dirname() */
#endif

void *mallocReadFile(
	const char *fileName,
	uint32_t *sizeRet
){
#ifdef _WIN32
	FILE *fd; fopen_s(&fd, fileName, "rb");
#else
	FILE *fd = fopen(fileName, "rb");
#endif
	if (fd == NULL) return NULL;
	struct stat stbuf;
#ifdef _WIN32
	if (fstat(_fileno(fd), &stbuf) == -1) {
#else
	if (fstat(fileno(fd), &stbuf) == -1) {
#endif
		fclose(fd);
		return NULL;
	}
	assert(stbuf.st_size < 0x100000000LL);
	uint32_t size = (uint32_t)stbuf.st_size;
	void *buffer = malloc(size);
	if (buffer == NULL) {
		fclose(fd);
		return NULL;
	}
	fread(buffer, 1, size, fd);
	*sizeRet = size;
	return buffer;
}

static void sdlAudioCallback(
	void	*userdata,
	uint8_t	*stream,
	int		len
){
	MXDRV_GetPCM(
		(MxdrvContext *)userdata,
		stream,
		len / (sizeof(uint16_t) * 2)
	);
}

int
main(
	int		argc
,	char	**argv
){
	/* ������� */
	if (argc == 1) {
		printf(
			"simple mdx player\n"
			"usage : %s mdxfilename\n",
			argv[0]
		);
		exit(EXIT_SUCCESS);
	}
	char *mdxFilePath = argv[1];

	/* MDX �t�@�C���̓ǂݍ��� */
	uint32_t mdxFileImageSizeInBytes = 0;
	void *mdxFileImage = mallocReadFile(mdxFilePath, &mdxFileImageSizeInBytes);
	if (mdxFileImage == NULL) {
		printf("mallocReadFile '%s' failed.\n", mdxFilePath);
		exit(EXIT_FAILURE);
	}

	/* MDX �^�C�g���̎擾 */
	char mdxTitle[256];
	if (
		MdxGetTitle(
			mdxFileImage, mdxFileImageSizeInBytes,
			mdxTitle, sizeof(mdxTitle)
		) == false
	) {
		printf("MdxGetTitle failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("mdx title = %s\n", mdxTitle);

	/* PDX �t�@�C����v�����邩 */
	bool hasPdx;
	if (
		MdxHasPdxFileName(
			mdxFileImage, mdxFileImageSizeInBytes,
			&hasPdx
		) == false
	) {
		printf("MdxHasPdxFileName failed.\n");
		exit(EXIT_FAILURE);
	}

	/* PDX �t�@�C���̓ǂݍ��� */
	uint32_t pdxFileImageSizeInBytes = 0;
	void *pdxFileImage = NULL;
	if (hasPdx) {
		char pdxFileName[FILENAME_MAX];
		if (
			MdxGetPdxFileName(
				mdxFileImage, mdxFileImageSizeInBytes,
				pdxFileName, sizeof(pdxFileName)
			) == false
		) {
			printf("MdxGetPdxFileName failed.\n");
			exit(EXIT_FAILURE);
		}
		printf("pdx filename = %s\n", pdxFileName);

		char pdxFilePath[FILENAME_MAX];
#ifdef _WIN32
		char mdxDirName[FILENAME_MAX];
		_splitpath_s(mdxFilePath, NULL, 0, mdxDirName, sizeof(mdxDirName), NULL, 0, NULL, 0);
		sprintf_s(pdxFilePath, sizeof(pdxFilePath), "%s%s", mdxDirName, pdxFileName);
#else
		sprintf(pdxFilePath, "%s/%s", dirname(mdxFilePath), pdxFileName);
#endif
		printf("pdx filepath = %s\n", pdxFilePath);

		pdxFileImage = mallocReadFile(pdxFilePath, &pdxFileImageSizeInBytes);
		if (pdxFileImage == NULL) {
			printf("mallocReadFile '%s' failed.\n", pdxFilePath);
			exit(EXIT_FAILURE);
		}
	}

	/* MDX PDX �o�b�t�@�̗v���T�C�Y�����߂� */
	uint32_t mdxBufferSizeInBytes = 0;
	uint32_t pdxBufferSizeInBytes = 0;
	if (
		MdxGetRequiredBufferSize(
			mdxFileImage,
			mdxFileImageSizeInBytes,
			pdxFileImageSizeInBytes,
			&mdxBufferSizeInBytes,
			&pdxBufferSizeInBytes
		) == false
	) {
		printf("MdxGetRequiredBufferSize failed.\n");
		exit(EXIT_FAILURE);
	}
	printf("mdxBufferSizeInBytes = %d\n", mdxBufferSizeInBytes);
	printf("pdxBufferSizeInBytes = %d\n", pdxBufferSizeInBytes);

	/* MDX PDX �o�b�t�@�̊m�� */
	void *mdxBuffer = NULL;
	mdxBuffer = (uint8_t *)malloc(mdxBufferSizeInBytes);
	if (mdxBuffer == NULL) {
		printf("malloc mdxBuffer failed.\n");
		exit(EXIT_FAILURE);
	}
	void *pdxBuffer = NULL;
	if (hasPdx) {
		pdxBuffer = (uint8_t *)malloc(pdxBufferSizeInBytes);
		if (pdxBuffer == NULL) {
			printf("malloc pdxBuffer failed.\n");
			exit(EXIT_FAILURE);
		}
	}

	/* MDX PDX �o�b�t�@���쐬 */
	if (
		MdxUtilCreateMdxPdxBuffer(
			mdxFileImage, mdxFileImageSizeInBytes,
			pdxFileImage, pdxFileImageSizeInBytes,
			mdxBuffer, mdxBufferSizeInBytes,
			pdxBuffer, pdxBufferSizeInBytes
		) == false
	) {
		printf("MdxUtilCreateMdxPdxBuffer failed.\n");
		exit(EXIT_FAILURE);
	}

	/* ���̎��_�ŁA�t�@�C���C���[�W�͔j�����Ă悢 */
	if (pdxFileImage != NULL) free(pdxFileImage);
	free(mdxFileImage);

	/* �R���e�L�X�g�̏����� */
	#define MDX_BUFFER_SIZE		1 * 1024 * 1024
	#define PDX_BUFFER_SIZE		2 * 1024 * 1024
	#define MEMORY_POOL_SIZE	4 * 1024 * 1024
	MxdrvContext context;
	if(
		MxdrvContext_Initialize(
			&context,
			MEMORY_POOL_SIZE
		) == false
	) {
		printf("MxdrvContext_Initialize failed.\n");
		exit(EXIT_FAILURE);
	}

	/* MXDRV �̏����� */
	#define SAMPLES_PER_SEC 48000
	{
		int ret = MXDRV_Start(
			&context,
			SAMPLES_PER_SEC,
			0, 0, 0,
			MDX_BUFFER_SIZE,
			PDX_BUFFER_SIZE,
			0
		);
		if (ret != 0) {
			printf("MXDRV_Start failed. return code = %d\n", ret);
			exit(EXIT_FAILURE);
		}
	}

	/* PCM8 ��L���� */
	uint8_t *pcm8EnableFlag = (uint8_t *)MXDRV_GetWork(&context, MXDRV_WORK_PCM8);
	*(pcm8EnableFlag) = 1;

	/* ���ʐݒ� */
	MXDRV_TotalVolume(&context, 256);

	/* �Đ����Ԃ����߂� */
	int loop = 1;
	int fadeout = 0;
	float songDuration = MXDRV_MeasurePlayTime(
		&context,
		mdxBuffer, mdxBufferSizeInBytes,
		pdxBuffer, pdxBufferSizeInBytes,
		loop,
		fadeout
	) / 1000.0f;
	printf("songDuration %.1f(sec)\n", songDuration);

	/* MDX �Đ� */
	MXDRV_Play(
		&context,
		mdxBuffer, mdxBufferSizeInBytes,
		pdxBuffer, pdxBufferSizeInBytes
	);

	/* SDL ������ */
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init(SDL_INIT_EVERYTHING) failed: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	/* ���C���E�B���h�E�쐬 */
	#define WINDOW_WIDTH	512
	#define WINDOW_HEIGHT	512
	SDL_Window *const window = SDL_CreateWindow(
		"simple mdx player",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		WINDOW_WIDTH, WINDOW_HEIGHT,
		SDL_WINDOW_FULLSCREEN_DESKTOP * 0);
	if (window == NULL) {
		printf("SDL_CreateWindow() failed: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}
	SDL_Surface *surface = SDL_GetWindowSurface(window);

	/* SDL AUDIO ������ */
	{
		SDL_AudioSpec fmt;
		memset(&fmt, 0, sizeof(fmt));
		fmt.freq		= SAMPLES_PER_SEC;
		fmt.format		= AUDIO_S16SYS;
		fmt.channels	= 2;
		fmt.samples		= 256;
		fmt.callback	= sdlAudioCallback;
		fmt.userdata	= &context;
		if (SDL_OpenAudio(&fmt, NULL) < 0) {
			printf("SDL_OpenAudio() failed: %s", SDL_GetError());
			exit(EXIT_FAILURE);
		}
		SDL_PauseAudio(0);
	}

	/* ���b�Z�[�W���[�v */
	bool bQuit = false;
	while (bQuit == false) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN: {
					if (event.key.keysym.sym == SDLK_ESCAPE) {
						bQuit = true;
					}
				} break;
				case SDL_QUIT: {
					bQuit = true;
				} break;
			}
		}

		/* ��ʏ��� */
		SDL_FillRect(surface, NULL, 0);

		/* OPM ���W�X�^�̉��� */
		{
			static int levelMeters[256];
			#define NUM_PIXELS_PER_COLUMN	64
			#define NUM_PIXELS_PER_ROW		8
			#define NUM_PIXELS_PER_BIT		7
			int regIndex, bit;
			for (regIndex = 0; regIndex < 256; regIndex++) {
				uint8_t regVal = 0;
				bool updated = false;
				MxdrvContext_GetOpmReg(&context, regIndex, &regVal, &updated);
				for (bit = 0; bit < 8; bit++) {
					int x = (regIndex & 7) * NUM_PIXELS_PER_COLUMN + bit * NUM_PIXELS_PER_BIT;
					int y = (regIndex / 8) * NUM_PIXELS_PER_ROW;
					SDL_Rect rect = {x, y, NUM_PIXELS_PER_BIT - 1, NUM_PIXELS_PER_ROW - 1};
					uint32_t color = 0x202020;
					levelMeters[regIndex] = levelMeters[regIndex] * 15 / 16;
					if (updated) levelMeters[regIndex] = 0x80;
					if (regVal & (1 << bit)) color = 0x70607F;
					color += levelMeters[regIndex] * 0x010101;
					SDL_FillRect(surface, &rect, color);
				}
			}
		}

		/* KEY �̉��� */
		{
			#define KEY_DISPLAY_X	32
			#define KEY_DISPLAY_Y	256
			#define KEY_WIDTH		5
			#define KEY_HEIGHT		16
			int i;
			const MXWORK_CH *fmChannels = (MXWORK_CH *)MXDRV_GetWork(&context, MXDRV_WORK_FM);
			const MXWORK_CH *pcmChannels = (MXWORK_CH *)MXDRV_GetWork(&context, MXDRV_WORK_PCM);

			/* FM 0~7 */
			for (i = 0; i < 8; i++) {
				if (fmChannels[i].S0016 & (1 << 3)) {
					int key = (fmChannels[i].S0012 + 27) / 64;

					int volume = (fmChannels[i].S0022);
					if (volume & 0x80) {
						volume = (0x7F - (volume & 0x7F)) * 2;
					} else {
						volume = (volume & 0xF) * 0x11;
					}

					{
						int x = key * KEY_WIDTH + ((fmChannels[i].S0014 - fmChannels[i].S0012) * KEY_WIDTH / 64);
						int y = i * KEY_HEIGHT;
						SDL_Rect rect = {x + KEY_DISPLAY_X, y + KEY_DISPLAY_Y, KEY_WIDTH, KEY_HEIGHT - 1};
						uint32_t color = 0x008000;
						SDL_FillRect(surface, &rect, color);
					}
					{
						int x = key * KEY_WIDTH;
						int y = i * KEY_HEIGHT;
						SDL_Rect rect = {x + KEY_DISPLAY_X, y + KEY_DISPLAY_Y, KEY_WIDTH, KEY_HEIGHT - 1};
						uint32_t color = 0x0000FF | (volume * 0x010100);
						SDL_FillRect(surface, &rect, color);
					}
				}
			}

			/* PCM 0~7 */
			for (i = 8; i < 16; i++) {
				const MXWORK_CH *pcmChannel;
				if (i == 8) {
					pcmChannel = &fmChannels[8];
				} else {
					pcmChannel = &pcmChannels[i - 9];
				}
				if (pcmChannel->S0016 & (1 << 3)) {
					int key = (pcmChannel->S0012 + 27) / 64;
					int volume = (pcmChannel->S0022);
					if (volume & 0x80) {
						volume = (0x7F - (volume & 0x7F)) * 2;
					} else {
						volume = (volume & 0xF) * 0x11;
					}
					int x = key * KEY_WIDTH;
					int y = i * KEY_HEIGHT;
					SDL_Rect rect = {x + KEY_DISPLAY_X, y + KEY_DISPLAY_Y, KEY_WIDTH, KEY_HEIGHT - 1};
					uint32_t color = 0xFF0000 | (volume * 0x000101);
					SDL_FillRect(surface, &rect, color);
				}
			}

			#define LEVEL_METER_DISPLAY_X	0
			#define LEVEL_METER_DISPLAY_Y	256
			#define LEVEL_METER_WIDTH		32
			#define LEVEL_METER_HEIGHT		16
			static int keyOnLevelMeters[16] = {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
			static int keyOffLevelMeters[16] = {0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
			for (i = 0; i < 8; i++) {
				bool currentKeyOn = false;
				bool logicalSumOfKeyOn = false;
				MxdrvContext_GetFmKeyOn(&context, i, &currentKeyOn, &logicalSumOfKeyOn);

				keyOnLevelMeters[i] = keyOnLevelMeters[i] * 15 / 16;
				if (currentKeyOn == false) {
					keyOffLevelMeters[i] = keyOffLevelMeters[i] * 63 / 64;
				}
				if (logicalSumOfKeyOn) {
					keyOnLevelMeters[i] = 0xFF;
					keyOffLevelMeters[i] = 0xFF;
				}
			}
			for (i = 8; i < 16; i++) {
				bool logicalSumOfKeyOn = false;
				MxdrvContext_GetPcmKeyOn(&context, i - 8, &logicalSumOfKeyOn);

				keyOnLevelMeters[i] = keyOnLevelMeters[i] * 15 / 16;
				if (logicalSumOfKeyOn) {
					keyOnLevelMeters[i] = 0xFF;
				}
			}
			for (i = 0; i < 16; i++) {
				{
					int x = 0;
					int y = i * LEVEL_METER_HEIGHT;
					int w = keyOffLevelMeters[i] * LEVEL_METER_WIDTH / 0xFF;
					SDL_Rect rect = {x + LEVEL_METER_DISPLAY_X, y + LEVEL_METER_DISPLAY_Y, w, LEVEL_METER_HEIGHT - 1};
					uint32_t color = 0x404040;
					SDL_FillRect(surface, &rect, color);
				}
				{
					int x = 0;
					int y = i * LEVEL_METER_HEIGHT;
					int w = keyOnLevelMeters[i] * LEVEL_METER_WIDTH / 0xFF;
					SDL_Rect rect = {x + LEVEL_METER_DISPLAY_X, y + LEVEL_METER_DISPLAY_Y, w, LEVEL_METER_HEIGHT - 1};
					uint32_t color = 0xFFFFFF;
					SDL_FillRect(surface, &rect, color);
				}
			}
		}

		/* ��ʍX�V */
		SDL_UpdateWindowSurface(window);
		SDL_Delay(10);
	}

	/* �I������ */
	SDL_DestroyWindow(window);
	SDL_Quit();
	MXDRV_End(&context);
	MxdrvContext_Terminate(&context);
	if (pdxBuffer != NULL) free(pdxBuffer);
	free(mdxBuffer);

	exit(EXIT_SUCCESS);
}

