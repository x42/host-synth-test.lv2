/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define HST_URI "http://gareus.org/oss/lv2/host_synth_test"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#include <lv2/lv2plug.in/ns/ext/log/logger.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/urid/urid.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>

typedef struct {
	/* ports */
	const LV2_Atom_Sequence* midi_in;
	float*                   audio_out;

	/* LV2 Features and URIs */
	LV2_URID_Map*  map;
	LV2_URID       midi_MidiEvent;
	LV2_Log_Log*   log;
	LV2_Log_Logger logger;
} HostSynthTest;

/* ****************************************************************************/

static LV2_Handle
instantiate (const LV2_Descriptor*     descriptor,
             double                    rate,
             const char*               bundle_path,
             const LV2_Feature* const* features)
{
	HostSynthTest* self = (HostSynthTest*)calloc (1, sizeof (HostSynthTest));

	for (int i = 0; features[i]; ++i) {
		if (!strcmp (features[i]->URI, LV2_URID__map)) {
			self->map = (LV2_URID_Map*)features[i]->data;
		} else if (!strcmp (features[i]->URI, LV2_LOG__log)) {
			self->log = (LV2_Log_Log*)features[i]->data;
		}
	}

	/* Initialise logger (if map is unavailable, will fallback to printf) */
	lv2_log_logger_init (&self->logger, self->map, self->log);

	if (!self->map) {
		lv2_log_error (&self->logger, "HostSynthTest.lv2: Host does not support urid:map\n");
		free (self);
		return NULL;
	}

	self->midi_MidiEvent = self->map->map (self->map->handle, LV2_MIDI__MidiEvent);

	return (LV2_Handle)self;
}

static void
connect_port (LV2_Handle instance,
              uint32_t   port,
              void*      data)
{
	HostSynthTest* self = (HostSynthTest*)instance;

	switch (port) {
		case 0:
			self->midi_in = (const LV2_Atom_Sequence*)data;
			break;
		case 1:
			self->audio_out = (float*)data;
			break;
		default:
			break;
	}
}

static void
run (LV2_Handle instance, uint32_t n_samples)
{
	HostSynthTest* self = (HostSynthTest*)instance;

	/* silence output */
	memset (self->audio_out, 0, sizeof (float) * n_samples);

	/* process MIDI events */
	LV2_ATOM_SEQUENCE_FOREACH (self->midi_in, ev)
	{
		if (ev->body.type == self->midi_MidiEvent) {
			const uint8_t* const data = (const uint8_t*)(ev + 1);
			const uint32_t       when = ev->time.frames;
#if 1 // skip MIDI panic messages
			if (ev->body.size == 3 && 0xb == (data[0] >> 4) && (data[1] == 0x7b || data[1] == 0x79 || data[1] == 0x40)) {
				continue;
			}
#endif
			if (ev->body.size == 3 && 0x8 == (data[0] >> 4)) {
				/* Note off */
				self->audio_out[when] -= -.5;
			} else if (ev->body.size == 3 && 0x9 == (data[0] >> 4)) {
				/* Note On */
				self->audio_out[when] += 1;
			}
		}
	}
}

static void
cleanup (LV2_Handle instance)
{
	free (instance);
}

static const LV2_Descriptor descriptor = {
	HST_URI,
	instantiate,
	connect_port,
	NULL,
	run,
	NULL,
	cleanup,
	NULL
};

/* clang-format off */
#undef LV2_SYMBOL_EXPORT
#ifdef _WIN32
# define LV2_SYMBOL_EXPORT __declspec(dllexport)
#else
# define LV2_SYMBOL_EXPORT __attribute__ ((visibility ("default")))
#endif
/* clang-format on */
LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor (uint32_t index)
{
	switch (index) {
		case 0:
			return &descriptor;
		default:
			return NULL;
	}
}
