/*
 * LibBink
 * Copyright (C) 2026 Justin Marshall
 *
 * This file is part of LibBink by Justin Marshall(justinmarshall20@gmail.com).
 *
 * LibBink is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * LibBink is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LibBink; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef CLASSWARS_LIBBINK_H
#define CLASSWARS_LIBBINK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cw_bink_decoder cw_bink_decoder;

enum cw_bink_status
{
	CW_BINK_ERROR = -1,
	CW_BINK_DONE = 0,
	CW_BINK_MORE = 1,
	CW_BINK_LAST = 2
};

cw_bink_decoder *cw_bink_open_file(char const *path);
void cw_bink_close(cw_bink_decoder *decoder);

int cw_bink_info(
	cw_bink_decoder const *decoder,
	unsigned long *width,
	unsigned long *height,
	unsigned long *frame_count,
	unsigned long *fps_numerator,
	unsigned long *fps_denominator);

int cw_bink_first(cw_bink_decoder *decoder);
int cw_bink_next(cw_bink_decoder *decoder);

/*
 * Packed, top-down RGB24 output for the most recently decoded frame.
 * The pointer remains valid until the next decode call or close.
 */
unsigned char const *cw_bink_get_rgb24(cw_bink_decoder const *decoder);

/*
 * Embedded audio metadata and decoded interleaved float samples. The sample
 * pointer contains only the audio carried by the most recently decoded video
 * frame and remains valid until the next decode call or close.
 */
int cw_bink_audio_info(
	cw_bink_decoder const *decoder,
	unsigned long *sample_rate,
	unsigned int *channels);
float const *cw_bink_get_audio_f32(
	cw_bink_decoder const *decoder,
	unsigned long *sample_frames);

char const *cw_bink_get_error(cw_bink_decoder const *decoder);

#ifdef __cplusplus
}
#endif

#endif
