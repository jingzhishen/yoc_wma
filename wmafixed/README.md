## 简介

wmafixed是一个定点化的开源wma解码库，支持WMAV1和WMAV2版本

## 如何使用

### 示例参考代码

```c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "wmadec.h"
#include "wma_raw.h"

#define PCM_BUF_SIZE (6*1024*1024)
int main()
{
	int rc, i, nb_samples;
	int offset = 0, remain, bsize = 512, pcm_size = 0;
	static int32_t obuf[1024 * 8];
	char *pcm, *data;
	asf_waveformatex_t wfx;
	WMADecodeContext wmadec;

	memset(&wfx, 0, sizeof(asf_waveformatex_t));
	memset(&wmadec, 0, sizeof(WMADecodeContext));
	
	pcm = malloc(PCM_BUF_SIZE);

	wfx.rate          = 16000;
	wfx.bitrate       = 16000*8;
	wfx.channels      = 1;
	wfx.blockalign    = bsize;
	wfx.bitspersample = 16;
	wfx.codec_id      = ASF_CODEC_ID_WMAV2;
	wfx.datalen       = 6;
	wfx.data[4]       = 0x1;

#if 0
	if( p_dec->fmt_in.i_codec == VLC_CODEC_WMA1 )
		wfx.codec_id = ASF_CODEC_ID_WMAV1;
	else if( p_dec->fmt_in.i_codec == VLC_CODEC_WMA2 )
		wfx.codec_id = ASF_CODEC_ID_WMAV2;

	wfx.datalen = p_dec->fmt_in.i_extra;
	if( wfx.datalen > 6 ) wfx.datalen = 6;
	if( wfx.datalen > 0 )
		memcpy( wfx.data, p_dec->fmt_in.p_extra, wfx.datalen );
#endif

	/* Init codec */
	rc = wma_decode_init(&wmadec, &wfx);
	if (rc < 0 ) {
		printf("codec init failed\n");
		return 0;
	}

	
	remain = _test_bin_len;
	while (remain > 0) {
		data = _test_bin + offset;
		rc = wma_decode_superframe_init(&wmadec, data, bsize);
		if (rc == 0) {
			printf("failed initializing wmafixed decoder\n" );
			return -1;
		}

		if (wmadec.nb_frames <= 0 ) {
			printf("can not decode, invalid ASF packet ?\n" );
			return -1;
		}

#if 1
		for (i = 0 ; i < wmadec.nb_frames; i++) {
			nb_samples = wma_decode_superframe_frame(&wmadec, obuf, data, bsize);
			if (nb_samples < 0) {
				printf("wma_decode_superframe_frame() failed for frame\n");
				return -1;
			}
			rc = nb_samples * 1 * sizeof(int32_t);
			for (int j = 0; j < nb_samples; j++)
				obuf[j] <<= 2;
			memcpy(pcm + pcm_size, obuf, rc);
			pcm_size += rc;
		}
		remain -= bsize;
		offset += bsize;
#endif
	}
	printf("====>>pcm = %p, pcm_size = %d\n", pcm, pcm_size);
	free(pcm);

	return 0;
} 
```

