/*
 * fft.c
 *
 *  Created on: 2026. 5. 23.
 *      Author: ADJ
 */

#include "main.h"

#include "audio_pipeline.h"
#include "global_define.h"
#include "adc.h"


extern volatile AudioSource_t audioSource;	// 오디오 모드 가져오기


#define FFT_SIZE 2048						// FFT 샘플 사이즈 설정
static arm_rfft_fast_instance_f32 fftInst;	// FFT 라이브러리 인스턴스
static uint8_t s_initialStart = 1;			// FFT 초기 실행 플래그


static float32_t s_window[FFT_SIZE];		// 윈도우 함수 저장 용도
static float32_t s_winGain;					// 윈도우 게인 값
static float32_t s_fftScale;				// FFT 스케일 보정 값


// 아래 부터 버퍼는 코드 상에서 순서대로 사용됨
static int16_t s_rawSample[FFT_SIZE];		// RAW 샘플 버퍼
static float32_t s_floatSample[FFT_SIZE];	// 플롯 변환된 샘플 버퍼
static float32_t s_winSample[FFT_SIZE];		// 윈도우 함수 적용한 샘플 버퍼


static float32_t s_fftOutput[FFT_SIZE];		// FFT 출력(실수, 복소수)
static float32_t s_fftMag[(FFT_SIZE/2)-1];	// FFT 크기


static float32_t s_magBand[16];				// FFT 크기 16개 검출
static float32_t s_scaleBand[16];			// FFT 크기 0~1 스케일로 변환


static float32_t s_dbfsBand[16];			// dBFS 저장


static float32_t s_dbfsPerCell;				// LED 한 칸당 dBFS
static float32_t s_ledLevel[16];			// LED 스케일 표시 정보


// 외부 수출 변수
static float32_t s_trail[16];
static float32_t s_peakHold[16];


// 밴드 범위 설정 ==========================================================================================
typedef struct
{
	uint16_t startIndex;
	uint16_t count;
} FFT_BandRange_t;

#define FFT_HZ_TO_BIN(hz)			((uint16_t)((((uint32_t)(hz) * FFT_SIZE) + (48000 / 2)) / 48000))
#define FFT_BIN_TO_MAG_INDEX(bin)	((uint16_t)((bin) - 1))
#define FFT_HZ_TO_MAG_INDEX(hz)		FFT_BIN_TO_MAG_INDEX(FFT_HZ_TO_BIN(hz))

#define FFT_BAND_RANGE(startHz, endHz)								\
	{																\
		FFT_HZ_TO_MAG_INDEX(startHz),								\
		(uint16_t)(FFT_HZ_TO_BIN(endHz) - FFT_HZ_TO_BIN(startHz))	\
	}

/*
static const FFT_BandRange_t s_bandRange[16] =
{
	FFT_BAND_RANGE(23,    94),		// s_fftMag[0   ~ 2]    : FFT bin 1   ~ 3
	FFT_BAND_RANGE(94,    164),		// s_fftMag[3   ~ 5]    : FFT bin 4   ~ 6
	FFT_BAND_RANGE(164,   258),		// s_fftMag[6   ~ 9]    : FFT bin 7   ~ 10
	FFT_BAND_RANGE(258,   375),		// s_fftMag[10  ~ 14]   : FFT bin 11  ~ 15
	FFT_BAND_RANGE(375,   539),		// s_fftMag[15  ~ 21]   : FFT bin 16  ~ 22
	FFT_BAND_RANGE(539,   773),		// s_fftMag[22  ~ 31]   : FFT bin 23  ~ 32
	FFT_BAND_RANGE(773,   1102),	// s_fftMag[32  ~ 45]   : FFT bin 33  ~ 46
	FFT_BAND_RANGE(1102,  1570),	// s_fftMag[46  ~ 65]   : FFT bin 47  ~ 66
	FFT_BAND_RANGE(1570,  2250),	// s_fftMag[66  ~ 94]   : FFT bin 67  ~ 95
	FFT_BAND_RANGE(2250,  3234),	// s_fftMag[95  ~ 136]  : FFT bin 96  ~ 137
	FFT_BAND_RANGE(3234,  4641),	// s_fftMag[137 ~ 196]  : FFT bin 138 ~ 197
	FFT_BAND_RANGE(4641,  6656),	// s_fftMag[197 ~ 282]  : FFT bin 198 ~ 283
	FFT_BAND_RANGE(6656,  9539),	// s_fftMag[283 ~ 405]  : FFT bin 284 ~ 406
	FFT_BAND_RANGE(9539,  13664),	// s_fftMag[406 ~ 581]  : FFT bin 407 ~ 582
	FFT_BAND_RANGE(13664, 18352),	// s_fftMag[582 ~ 781]  : FFT bin 583 ~ 782
	FFT_BAND_RANGE(18352, 24000)	// s_fftMag[782 ~ 1022] : FFT bin 783 ~ 1023
};
*/

static const FFT_BandRange_t s_bandRange[16] =
{
	FFT_BAND_RANGE(23,    94),		// s_fftMag[0   ~ 2]    : FFT bin 1   ~ 3
	FFT_BAND_RANGE(94,    188),		// s_fftMag[3   ~ 6]    : FFT bin 4   ~ 7
	FFT_BAND_RANGE(188,   328),		// s_fftMag[7   ~ 12]   : FFT bin 8   ~ 13
	FFT_BAND_RANGE(328,   516),		// s_fftMag[13  ~ 20]   : FFT bin 14  ~ 21
	FFT_BAND_RANGE(516,   773),		// s_fftMag[21  ~ 31]   : FFT bin 22  ~ 32
	FFT_BAND_RANGE(773,   1125),	// s_fftMag[32  ~ 46]   : FFT bin 33  ~ 47
	FFT_BAND_RANGE(1125,  1617),	// s_fftMag[47  ~ 67]   : FFT bin 48  ~ 68
	FFT_BAND_RANGE(1617,  2310),	// s_fftMag[68  ~ 97]   : FFT bin 69  ~ 98
	FFT_BAND_RANGE(2310,  3258),	// s_fftMag[98  ~ 137]  : FFT bin 99  ~ 138
	FFT_BAND_RANGE(3258,  4594),	// s_fftMag[138 ~ 194]  : FFT bin 139 ~ 195
	FFT_BAND_RANGE(4594,  6469),	// s_fftMag[195 ~ 274]  : FFT bin 196 ~ 275
	FFT_BAND_RANGE(6469,  9000),	// s_fftMag[275 ~ 382]  : FFT bin 276 ~ 383
	FFT_BAND_RANGE(9000,  11500),	// s_fftMag[383 ~ 489]  : FFT bin 384 ~ 490
	FFT_BAND_RANGE(11500, 13500),	// s_fftMag[490 ~ 574]  : FFT bin 491 ~ 575
	FFT_BAND_RANGE(13500, 15000),	// s_fftMag[575 ~ 638]  : FFT bin 576 ~ 639
	FFT_BAND_RANGE(15000, 24000)	// s_fftMag[639 ~ 1022] : FFT bin 640 ~ 1023
};


/*
 * ======================================================================================================
 */

void FFT_Init(void)
{
	// 윈도우 특성은 함수 정의에서 확인할 것
	//arm_hamming_f32(s_window, FFT_SIZE);
	arm_blackman_harris_92db_f32(s_window, FFT_SIZE);


	arm_mean_f32(s_window, FFT_SIZE, &s_winGain);
	s_fftScale = 2.0f / ((float32_t)FFT_SIZE * s_winGain);
	arm_rfft_fast_init_f32(&fftInst, FFT_SIZE);
	s_initialStart = 1;
}

uint8_t FFT_Run(void)
{
	/* FFT 초기 실행 로직 */
	if (s_initialStart)
	{
		if (AudioPipeline_Mono_Available() < FFT_SIZE)
			return 0;

		AudioPipeline_PopMono(
				s_rawSample,
				FFT_SIZE
		);	// 오버랩 없이 배열에 샘플 싹다 채우기

		s_initialStart = 0;
	}

	/* FFT 50% 오버랩 로직 */
	else
	{
		if (AudioPipeline_Mono_Available() < FFT_SIZE/2)
			return 0;

		arm_copy_q15(
			&s_rawSample[FFT_SIZE/2],
			&s_rawSample[0],
			FFT_SIZE/2
		);	// 배열 절반 뒤를 앞으로 당김

		AudioPipeline_PopMono(
				&s_rawSample[FFT_SIZE/2],
				FFT_SIZE/2
		);	// 배열 절반 뒤에 샘플 넣기
	}




	/*
	 * dBFS 정규화 작업
	 *
	 * dBFS란 디지털 최대값을 0dB로 놓고 현재 신호가 얼마나 작은지 보는 거임
	 * 공식 : dBFS = 20 * log10(진폭 절댓값 / 최대 진폭 절댓값)
	 *
	 * 여기서 최대 진폭 절댓값을 1로 정규화하면
	 * dBFS = 20 * log10(진폭 절댓값)   이처럼 수식을 축약 가능
	 *
	 * 그래서 샘플을 -1 ~ 1 로 정규화 하는 거임
	 *
	 * int16_t signed PCM
	 * -32768 ~ +32767
	 * arm_q15_to_float() 결과: -1.0f ~ +0.999969f
	 */
	arm_q15_to_float(s_rawSample, s_floatSample, FFT_SIZE);




	// 윈도우 적용
	arm_mult_f32(s_floatSample, s_window, s_winSample, FFT_SIZE);




	// FFT 수행
	arm_rfft_fast_f32(&fftInst, s_winSample, s_fftOutput, 0);




	/*
	 * FFT 크기 계산
	 *
	 * s_fftOutput[0] = DC
	 * s_fftOutput[1] = Nyquist
	 *
	 * s_fftOutput[2] = bin 1 real
	 * s_fftOutput[3] = bin 1 imag
	 * ...
	 */
	arm_cmplx_mag_f32(&s_fftOutput[2], &s_fftMag[0], (FFT_SIZE/2)-1);




	// 피크 검출 밴드 계산
	float32_t peak;
	uint32_t index;
	for (uint16_t i = 0; i < 16; i++)
	{
		arm_max_f32(
			&s_fftMag[s_bandRange[i].startIndex],
			s_bandRange[i].count,
			&peak,
			&index
		);

		s_magBand[i] = peak;
	}




	/*
	 * FFT 크기는 입력 진폭(-1~1) 그대로 나오지 않고 FFT 내부 누적합 크기임
	 *
	 * FFT_SIZE가 2048이라면 FFT 최대 크기는 "FFT_SIZE / 2 = 1024"가 됨
	 * 2를 나누는 이유는 rfft 함수는 음수 주파수를 생략하기 때문에 에너지가 절반이 됨
	 *
	 * 여기에 윈도우를 적용하면 "윈도우 게인" 만큼 더 작아짐
	 * 게인은 윈도우 배열의 평균값을 통해 구할 수 있음
	 *
	 * 따라서 입력 진폭 크기로 되돌리려면
	 * 진폭 = FFT 크기 * 2 / (FFT_SIZE * 윈도우 게인) 보정이 필요함
	 *
	 * 최종적으로 FFT 크기를 0~1로 정규화함
	 * "s_fftScale"은 FFT_INIT 함수에서 계산함
	 */
	arm_scale_f32(s_magBand, s_fftScale, s_scaleBand, 16);			// 보정 적용
	arm_clip_f32(s_scaleBand, s_scaleBand, 0.000001f, 1.0f, 16);	// log(0) 방지 : 000001~1.0 클림프




	// dBFS 계산 : dBFS = 20 * log10(amplitude)
	for (uint16_t i = 0; i < 16; i++)
		s_dbfsBand[i] = 20.0f * log10f(s_scaleBand[i]);




	// 가변저항 값 준비
	uint8_t vol;								// 가변저항 값 0~100
	float32_t dbfsMin;							// 정규화된 가변저항 값 -60 ~ -20
	ADC_GetValue_VOL(&vol);						// 가변저항 값 가져오기
	dbfsMin = -60.0f + ((float32_t)vol * 0.4f);	// 가변저항 정규화




	// WS2812B 매핑
	s_dbfsPerCell = -dbfsMin / 16.0f;	// LED 셀당 dBFS 크기 계산
	for (uint16_t i = 0; i < 16; i++)	// LED 매핑
	{
		s_ledLevel[i] = 16.0f + (s_dbfsBand[i] / s_dbfsPerCell);
		if (s_ledLevel[i] > 15.5f)
			s_ledLevel[i] = 15.5f;
		if (s_ledLevel[i] < 0.0f)
			s_ledLevel[i] = 0.0f;
	}



	// 저역->고역 가중치 적용 로직
	//#define USE_BAND_WEIGHT
	#ifdef USE_BAND_WEIGHT
	for (uint16_t i = 0; i < 16; i++)
	{
		float32_t t = i / 15.0f;
		float32_t weight = 1.0f + powf(t, 1.3f);

		s_ledLevel[i] *= weight;

		if (s_ledLevel[i] > 15.5f)
			s_ledLevel[i] = 15.5f;
		if (s_ledLevel[i] < 0.0f)
			s_ledLevel[i] = 0.0f;
	}
	#endif




	// PeakHold 수출 변수 작성
	for (uint16_t i = 0; i < 16; i++)
	{
		if (s_trail[i] > s_peakHold[i])
			s_peakHold[i] = s_trail[i];
		else
			s_peakHold[i] *= 0.985f;
	}



	// Trail 수출 변수 작성
	#define USE_SMOOTHING
	#ifdef USE_SMOOTHING
	for (uint16_t i = 0; i < 16; i++)
	{
		if (s_ledLevel[i] > s_trail[i])
			s_trail[i] += 0.70f * (s_ledLevel[i] - s_trail[i]);
		else
			s_trail[i] += 0.37f * (s_ledLevel[i] - s_trail[i]);
	}
	#else
	arm_copy_f32(
			s_ledLevel,
			s_trail,
			16
	);
	#endif

	return 1;
}

const float32_t *Visual_GetTrail(void)
{
	return s_trail;
}
const float32_t *Visual_GetPeak(void)
{
	return s_peakHold;
}
