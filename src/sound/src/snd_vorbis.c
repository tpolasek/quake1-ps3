/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) Henrique Barateli, <henriquejb194@gmail.com>, et al.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#include "snd_vorbis.h"
#include "console.h"
#include "zone.h"

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

// Vorbis codec can return the samples in a number of different
// formats, we use the standard signed short format.
#define VORBIS_SAMPLEBITS  16
#define VORBIS_SAMPLEWIDTH 2
#define VORBIS_SIGNED_DATA 1

// CALLBACK FUNCTIONS:

static int ovc_fclose(void* f) {
    // We fclose() elsewhere.
    return 0;
}

static int ovc_fseek(void* f, ogg_int64_t off, int whence) {
    if (f == NULL) {
        return (-1);
    }
    return Q_fseek(f, (long) off, whence);
}

static ov_callbacks ovc_qfs = {
    .read_func = (size_t (*)(void*, size_t, size_t, void*)) Q_fread,
    .seek_func = ovc_fseek,
    .close_func = ovc_fclose,
    .tell_func = (long (*)(void*)) Q_ftell,
};

static qboolean S_VORBIS_CodecInitialize(void) {
    return true;
}

static void S_VORBIS_CodecShutdown(void) {
}

static qboolean S_VORBIS_CodecOpenStream(snd_stream_t* stream) {
    OggVorbis_File* ovFile;
    vorbis_info* ovf_info;
    long numstreams;
    int res;

    ovFile = (OggVorbis_File*) Z_Malloc(sizeof(OggVorbis_File));
    stream->priv = ovFile;
    res = ov_open_callbacks(&stream->fh, ovFile, NULL, 0, ovc_qfs);
    if (res != 0) {
        Con_Printf("%s is not a valid Ogg Vorbis file (error %i).\n",
                   stream->name, res);
        goto _fail;
    }

    if (!ov_seekable(ovFile)) {
        Con_Printf("Stream %s not seekable.\n", stream->name);
        goto _fail;
    }

    ovf_info = ov_info(ovFile, 0);
    if (!ovf_info) {
        Con_Printf("Unable to get stream info for %s.\n", stream->name);
        goto _fail;
    }

    // FIXME: handle section changes
    numstreams = ov_streams(ovFile);
    if (numstreams != 1) {
        Con_Printf("More than one (%ld) stream in %s.\n", numstreams,
                   stream->name);
        goto _fail;
    }

    if (ovf_info->channels != 1 && ovf_info->channels != 2) {
        Con_Printf("Unsupported number of channels %d in %s\n",
                   ovf_info->channels, stream->name);
        goto _fail;
    }

    stream->info.rate = ovf_info->rate;
    stream->info.channels = ovf_info->channels;
    stream->info.bits = VORBIS_SAMPLEBITS;
    stream->info.width = VORBIS_SAMPLEWIDTH;

    return true;
_fail:
    if (res == 0)
        ov_clear(ovFile);
    Z_Free(ovFile);
    return false;
}

static i32 S_VORBIS_CodecReadStream(
    snd_stream_t* stream,
    i32 bytes,
    void* buffer
) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    qboolean host_bigendian = true;
#else
    qboolean host_bigendian = false;
#endif
    i32 cnt = 0;
    i32 rem = bytes;
    char* ptr = (char*) buffer;
    const int word = VORBIS_SAMPLEWIDTH;
    const int sgned = VORBIS_SIGNED_DATA;
    i32 res;

    while (true) {
        // # ov_read() from libvorbisfile returns the decoded
        //   PCM audio in requested endianness, signedness and
        //   word size.
        // # ov_read() from Tremor (libvorbisidec) returns
        //   decoded audio always in host-endian, signed 16 bit
        //   PCM format.
        // # For both of the libraries, if the audio is
        //   multichannel, the channels are interleaved in
        //   the output buffer.
        int section; // FIXME: handle section changes
        res = ov_read(stream->priv, ptr, rem, host_bigendian, word, sgned, &section);
        if (res <= 0) {
            break;
        }
        rem -= res;
        cnt += res;
        if (rem <= 0) {
            break;
        }
        ptr += res;
    }

    if (res < 0) {
        return res;
    }
    return cnt;
}

static void S_VORBIS_CodecCloseStream(snd_stream_t* stream) {
    ov_clear(stream->priv);
    Z_Free(stream->priv);
    S_CodecUtilClose(&stream);
}

static i32 S_VORBIS_CodecRewindStream(snd_stream_t* stream) {
    /*
     * For libvorbisfile, the ov_time_seek() position argument
     * is seconds as doubles, whereas for Tremor libvorbisidec
     * it is milliseconds as 64 bit integers.
     */
    return ov_time_seek(stream->priv, 0);
}

snd_codec_t vorbis_codec = {
    .type = CODECTYPE_VORBIS,
    .initialized = true, // always available.
    .ext = "ogg",
    .initialize = S_VORBIS_CodecInitialize,
    .shutdown = S_VORBIS_CodecShutdown,
    .codec_open = S_VORBIS_CodecOpenStream,
    .codec_read = S_VORBIS_CodecReadStream,
    .codec_rewind = S_VORBIS_CodecRewindStream,
    .codec_jump = NULL,
    .codec_close = S_VORBIS_CodecCloseStream,
    .next = NULL,
};
