/*
 * DSD Stream File (DSF) demuxer
 * Copyright (c) 2014 Peter Ross
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavutil/channel_layout.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/dst/dst_decoder.h"
#include "sacd_ripper/scarletbook_read.h"
#include "sacd_ripper/genre.dat"
#include "avformat.h"
#include "internal.h"
#include "id3v2.h"
#include <stdbool.h>
#include <libkern/OSByteOrder.h>

#define SACD_OFFSET 1044480 // 2048 * 510

#define SACD_RIPPER 1
typedef struct {
    int track_num;
    int64_t index;
    int64_t origin_index;
    int64_t total_frames;
    int64_t origin_frames;
    double duration;
    int64_t gap;
    int64_t origin_gap;
} SACDISOTrack;

typedef struct {
    int num_tracks;
    int sample_rate;
    int64_t block_size;
    int64_t total_samples;
    int64_t total_pcm_samples;
    int64_t dsd_samples_per_block;
    int64_t dsd_bytes_per_block;
    int64_t pcm_sample_per_block;
    int64_t total_blocks;
    int64_t current_block;
    int64_t last_block_dsd_bytes;
    uint32_t trackLSN[256];
    uint32_t currentLSN;
    unsigned char *buffer;
    int bytesInBuffer;
    unsigned char *dstBuffer;
    unsigned int bytesInDSTBuffer;
    ebunch *dstDecoder;
    SACDISOTrack *tracks;
} SACDISOContext;


static int sacd_iso_probe(const AVProbeData *p)
{
    if(p->buf_size < SACD_OFFSET + 8) {
        return 0;
    }
    unsigned char* offset_buffer =  p->buf + SACD_OFFSET;
    //Master_TOC_Signature
    if(memcmp(offset_buffer, "SACDMTOC", 8) == 0){
        return AVPROBE_SCORE_MAX;
    }
    return 0;
}

static int sacd_seek(AVFormatContext *s, off_t pos){
    av_log(s, AV_LOG_ERROR, "sacd seek pos:%lld\n", pos);
    SACDISOContext *sacd = s->priv_data;
    AVIOContext *pb = s->pb;
    
    int i;
    for(i=1;i<sacd->num_tracks;i++) {
        SACDISOTrack track = sacd->tracks[i];
        if (track.origin_index * 588 * 8 * 2 > pos) {
            break;
        }
    }
    
    //double posInTrack = pos - [(XLDTrack *)[trackList objectAtIndex:i-1] index]*588*8*2;
    double posInTrack = (double)(pos - (sacd->tracks[i-1].origin_index) * 588 * 8 * 2);
    //av_log(s, AV_LOG_ERROR, "sacd seek i:%d, posInTrack:%f\n", i, posInTrack);
    double trackLength;
    if(i==sacd->num_tracks) trackLength = sacd->tracks[i-1].total_frames * 588 * 8 * 2;
    else trackLength = (sacd->tracks[i].origin_index - sacd->tracks[i-1].origin_index) * 588 * 8 * 2;
    double relativePos = posInTrack / trackLength;
    //av_log(s, AV_LOG_ERROR, "sacd seek trackLength:%f, relativePos:%f\n", trackLength, relativePos);
    sacd->currentLSN = sacd->trackLSN[i-1] + (int)(relativePos * (sacd->trackLSN[i] - sacd->trackLSN[i-1]));
    if(sacd->currentLSN >= sacd->trackLSN[0]+5) sacd->currentLSN -= 5;
    
    off_t estimatedPos = (off_t)sacd->currentLSN * 2048;
    //if(fseeko(fp,estimatedPos,SEEK_SET)) return NO;
    av_log(s, AV_LOG_ERROR, "sacd seek estimatedPos:%lld\n",estimatedPos);
    int64_t new_pos = avio_seek(pb, estimatedPos, SEEK_SET);
    if (new_pos < 0) {
        return 0;
    }
    
    sacd->bytesInBuffer = 0;
    sacd->bytesInDSTBuffer = 0;
    
    unsigned char tmp8;
    unsigned short tmp16;
    unsigned int tmp32;
    off_t diff = 0;
    bool mustRead = false;
    
    while(!mustRead) {
        int nPacketInfo;
        int nFrameInfo;
        int dstEncoded;
        int packetType[7];
        int packetLength[7];
        int frameStart[7];
        int seekpointFrameindex = 0;
        int frameIndex = 0;
        bool seekpointFound = false;
        off_t lastDiff;
        //fprintf(stderr, "current sector:%d\n",currentLSN);
        off_t sectorBegin = avio_tell(pb);
        //int ret = fread(&tmp8,1,1,fp);
        //if(ret < 1) break;
        tmp8 = avio_r8(pb);
        
        nPacketInfo = tmp8 >> 5;
        nFrameInfo = (tmp8 >> 2) & 0x07;
        dstEncoded = tmp8 & 1;
        for(i=0;i<nPacketInfo;i++) {
//            ret = fread(&tmp16,2,1,fp);
//            if(ret < 1) break;
            tmp16 = avio_rl16(pb);
            tmp16 = OSSwapBigToHostInt16(tmp16);
            frameStart[i] = tmp16 >> 15;
            packetType[i] = (tmp16 >> 11) & 0x7;
            packetLength[i] = tmp16 & 0x7ff;
        }
        for(i=0;i<nFrameInfo;i++) {
//            ret = fread(&tmp32,4,1,fp);
//            if(ret < 1) break;
            tmp32 = avio_rl32(pb);
            tmp32 = OSSwapBigToHostInt32(tmp32);
            int minutes = tmp32 >> 24;
            int seconds = (tmp32 >> 16) & 0xff;
            int frames = (tmp32 >> 8) & 0xff;
            off_t offsetInBytes = (off_t)minutes * 75 * 60;
            offsetInBytes += (off_t)seconds * 75;
            offsetInBytes += frames;
            offsetInBytes -= 150;
            offsetInBytes *= 588 * 8 * 2;
            lastDiff = pos - offsetInBytes;
            //fprintf(stdout,"Found frame %02d:%02d:%02d\n",minutes,seconds,frames);
            //fprintf(stdout,"byte offset: %lld, difference: %lld\n",offsetInBytes,lastDiff);
            if(lastDiff >= 0 && lastDiff < 588 * 8 * 2) {
                seekpointFound = true;
                seekpointFrameindex = i;
                diff = lastDiff;
            }
            if(!dstEncoded){
                //fseeko(fp,-1,SEEK_CUR);
                avio_seek(pb, -1, SEEK_CUR);
            }
        }
        if(!seekpointFound) {
            if(lastDiff >= 588 * 8 * 2) {
                sacd->currentLSN++;
                //fseeko(fp, sectorBegin+2048, SEEK_SET);
                avio_seek(pb, sectorBegin+2048, SEEK_SET);
                goto end;
            }
            else if(lastDiff < 0) {
                sacd->currentLSN -= 5;
                //fseeko(fp, sectorBegin-10240, SEEK_SET);
                avio_seek(pb, sectorBegin-10240, SEEK_SET);
                goto end;
            }
        }
        int ret = 0;
        for(i=0;i<nPacketInfo;i++) {
            if(packetType[i] == 2) {
                if(frameStart[i]) {
                    if(seekpointFrameindex == frameIndex++) mustRead = true;
                }
                if(mustRead) {
                    if(!dstEncoded) {
                        if(packetLength[i] <= diff) {
                            //fseeko(fp,packetLength[i],SEEK_CUR);
                            avio_seek(pb, packetLength[i], SEEK_CUR);
                            diff -= packetLength[i];
                        }
                        else {
                            if(diff){
                                //fseeko(fp,diff,SEEK_CUR);
                                avio_seek(pb, diff, SEEK_CUR);
                            }
                            //ret = fread(buffer+bytesInBuffer,1,packetLength[i]-diff,fp);
                            ret = avio_read(pb, sacd->buffer+sacd->bytesInBuffer, packetLength[i]-diff);
                            sacd->bytesInBuffer += ret;
                            diff = 0;
                        }
                    }
                    else {
                        if(frameStart[i] && sacd->bytesInDSTBuffer) {
                            ret = DSTDecoderDecode(sacd->dstDecoder, sacd->dstBuffer, sacd->buffer+sacd->bytesInBuffer, 0, &(sacd->bytesInDSTBuffer));
                            sacd->bytesInBuffer += 588*2*8;
                            sacd->bytesInDSTBuffer = 0;
                            if(sacd->bytesInBuffer <= diff) {
                                diff -= sacd->bytesInBuffer;
                                sacd->bytesInBuffer = 0;
                            }
                            else {
                                memmove(sacd->buffer,sacd->buffer+diff,sacd->bytesInBuffer-diff);
                                sacd->bytesInBuffer -= diff;
                                diff = 0;
                            }
                        }
                        //ret = fread(dstBuffer+bytesInDSTBuffer,1,packetLength[i],fp);
                        ret = avio_read(pb, sacd->dstBuffer+sacd->bytesInDSTBuffer, packetLength[i]);
                        sacd->bytesInDSTBuffer += ret;
                    }
                }
                else{
                    //fseeko(fp,packetLength[i],SEEK_CUR);
                    avio_seek(pb, packetLength[i], SEEK_CUR);
                }
            }else{
                //fseeko(fp,packetLength[i],SEEK_CUR);
                avio_seek(pb, packetLength[i], SEEK_CUR);
            }
        }
        //fseeko(fp, sectorBegin+2048, SEEK_SET);
        avio_seek(pb, sectorBegin+2048, SEEK_SET);
        sacd->currentLSN++;
        if(mustRead && !diff) break;
    end:
        ;
    }
    return 0;
}

static int sacd_iso_seek(AVFormatContext *s, int stream_index,
                     int64_t timestamp, int flags)
{
    AVStream *st = s->streams[stream_index];
    SACDISOContext *sacd = s->priv_data;
    AVIOContext *pb = s->pb;
    double offset_time = 0;
    double position = (double)timestamp / st->time_base.den;
    av_log(s, AV_LOG_ERROR, "sacd seek position:%f\n", position);
    int i = 0;
    SACDISOTrack *track = NULL;
    for (i = sacd->num_tracks - 1; sacd->num_tracks >= 0; i--) {
        track = &(sacd->tracks[i]);
        double startTimeN = (double)track->origin_index / 75.0;
        if (position >= startTimeN) {
            offset_time = position - startTimeN;
            break;
        }
    }
    if (track == NULL) {
        return 0;
    }
    int trackIndex = i;
    if (offset_time >= track->duration) {
        offset_time = track->duration - 5;
    }
    
    av_log(s, AV_LOG_ERROR, "sacd seek track index:%d, offset_time:%f\n", track->track_num,offset_time);
    
    int64_t framesToPlay = 0;
    int64_t totalFrame = sacd->total_pcm_samples;
    if(trackIndex == sacd->num_tracks - 1) { //last track
        framesToPlay = totalFrame - track->index;
    }
    else {
        framesToPlay = track->total_frames;
    }
    int64_t seekPoint = track->index + (offset_time / track->duration) * framesToPlay;
    if(seekPoint > totalFrame){
        seekPoint = totalFrame;
    }
    sacd->current_block = seekPoint / sacd->pcm_sample_per_block;
    //av_log(s, AV_LOG_ERROR, "sacd seek track seekpoint:%lld, current block:%lld\n", seekPoint,sacd->current_block);
    sacd_seek(s, sacd->current_block * sacd->block_size);

    return 0;
}

static int sacd_iso_read_header(AVFormatContext *s)
{
    SACDISOContext *sacd = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream *st;
    
    uint8_t tmp8;
    uint16_t tmp16;
    uint32_t tmp32;
    off_t areaTocOffset1;
    off_t areaTocOffset2;
    int64_t new_pos;
    
    int64_t totalSamples = 0;
    int numTracks = 0;
    char header[8] = {0};
    
#if SACD_RIPPER
    scarletbook_handle_t *sb_handle = scarletbook_open(s);
    if (sb_handle == 0) {
        return AVERROR_INVALIDDATA;
    }
    
    areaTocOffset1 = sb_handle->master_toc->area_1_toc_1_start * SACD_LSN_SIZE;
    areaTocOffset2 = sb_handle->master_toc->area_1_toc_2_start * SACD_LSN_SIZE;
    uint8_t key[256] = {0}, value[256] = {0};
    
    if (areaTocOffset1) {
        if (sb_handle->twoch_area_idx == -1) {
            return AVERROR_INVALIDDATA;
        }
        
        totalSamples = ((int64_t)sb_handle->area[sb_handle->twoch_area_idx].area_toc->total_playtime.minutes * 60 + (int64_t)sb_handle->area[sb_handle->twoch_area_idx].area_toc->total_playtime.seconds - 2/* 2 seconds offset */) * 75 + sb_handle->area[sb_handle->twoch_area_idx].area_toc->total_playtime.frames;
        totalSamples = totalSamples * 37632;
        snprintf(value, 255, "%lld", totalSamples);
        av_dict_set(&s->metadata, "sacd_total_frames", value, 0);
        sacd->total_samples = totalSamples;
        sacd->sample_rate = 2822400;
        snprintf(value, 255, "%d", 2822400);
        av_dict_set(&s->metadata, "sacd_sample_rate", value, 0);
        numTracks = sb_handle->area[sb_handle->twoch_area_idx].area_toc->track_count;
        sacd->num_tracks = numTracks;
        
        snprintf(value, 255, "%d", numTracks);
        av_dict_set(&s->metadata, "tracktotal", value, 0);
        if(numTracks <= 0){
            return AVERROR_INVALIDDATA;
        }
        
        sacd->tracks = malloc(sizeof(SACDISOTrack) * numTracks);
        
        /* create primary stream before any id3 coverart streams */
        st = avformat_new_stream(s, NULL);
        if (!st)
            return AVERROR(ENOMEM);
        int MM,SS,FF;
        int64_t idx, duration;
        int64_t previousFrame = 0;
        int64_t previousIndex = 0;
        
        for(int i=0;i<numTracks;i++) {
            sacd->trackLSN[i] = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_offset->track_start_lsn[i];
            SACDISOTrack *track = &(sacd->tracks[i]);
            track->track_num = i+1;
            int MM = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->start[i].minutes;
            int SS = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->start[i].seconds;
            int FF = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->start[i].frames;
            idx = (int64_t)MM * 60 * 75;
            idx += (int64_t)SS * 75;
            idx += FF;
            idx -= 150; /* 2 seconds offset */
            
            track->index = idx;
            track->origin_index = idx;

            MM = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->duration[i].minutes;
            SS = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->duration[i].seconds;
            FF = sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_time->duration[i].frames;
            duration = (int64_t)MM * 60 * 75;
            duration += (int64_t)SS * 75;
            duration += FF;
            
            track->total_frames = duration;
            track->origin_frames = duration;
            double seconds = MM*60 + SS + (double)FF/75.0;
            track->duration = seconds;
            track->gap = 0;
            track->origin_gap = 0;
            if(i > 0) {
                int64_t gap = idx - (previousIndex + previousFrame);
                previousIndex = idx;
                previousFrame = duration;
                track->gap = gap;
                track->origin_gap = gap;
            }else {
                previousIndex = idx;
                previousFrame = duration;
            }
        }
        if (numTracks > 0) {
            sacd->trackLSN[numTracks] = sacd->trackLSN[numTracks - 1] + (sb_handle->area[sb_handle->twoch_area_idx].area_tracklist_offset->track_length_lsn[numTracks - 1]);
        }
        double scale = (double)sacd->sample_rate/ 8.0 / 75.0;
        const int sacd_id3_genres[] = {
            12,     /* Not used => Other */
            12,     /* Not defined => Other */
            60,     /* Adult Contemporary => Top 40 */
            40,     /* Alternative Rock => AlternRock */
            12,     /* Children's Music => Other */
            32,     /* Classical => Classical */
            140,    /* Contemporary Christian => Contemporary Christian */
            2,      /* Country => Country */
            3,      /* Dance => Dance */
            98,     /* Easy Listening => Easy Listening */
            109,    /* Erotic => Porn Groove */
            80,     /* Folk => Folk */
            38,     /* Gospel => Gospel */
            7,      /* Hip Hop => Hip-Hop */
            8,      /* Jazz => Jazz */
            86,     /* Latin => Latin */
            77,     /* Musical => Musical */
            10,     /* New Age => New Age */
            103,    /* Opera => Opera */
            104,    /* Operetta => Chamber Music */
            13,     /* Pop Music => Pop */
            15,     /* RAP => Rap */
            16,     /* Reggae => Reggae */
            17,     /* Rock Music => Rock */
            14,     /* Rhythm & Blues => R&B */
            37,     /* Sound Effects => Sound Clip */
            24,     /* Sound Track => Soundtrack */
            101,    /* Spoken Word => Speech */
            48,     /* World Music => Ethnic */
            0,      /* Blues => Blues */
            12,     /* Not used => Other */
        };
        for(int i=0;i<numTracks;i++) {
            SACDISOTrack *origTrack = &(sacd->tracks[i]);
            
            double index = scale * origTrack->index + 0.5;
            double frames = scale * origTrack->total_frames + 0.5;
            double gap = scale * origTrack->gap + 0.5;
            
            origTrack->index = (int64_t)index;
            origTrack->total_frames = (int64_t)frames;
            origTrack->gap = (int64_t)gap;

            if(i > 0) {
                SACDISOTrack *prev = &(sacd->tracks[i-1]);
                if(prev->gap) {
                    origTrack->gap = origTrack->index - (prev->index + prev->total_frames);
                }
                else {
                    prev->total_frames = origTrack->index - prev->index;
                }
            }

            snprintf(key, 255, "track_%d_track_number", i);
            snprintf(value, 255, "%d", i+1);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_index", i);
            snprintf(value, 255, "%d", origTrack->index);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_origin_index", i);
            snprintf(value, 255, "%d", origTrack->origin_index);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d__total_frames", i);
            snprintf(value, 255, "%lld", origTrack->total_frames);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_origin_frames", i);
            snprintf(value, 255, "%lld", origTrack->origin_frames);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_dutaion", i);
            snprintf(value, 255, "%f", origTrack->duration);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_gap", i);
            snprintf(value, 255, "%lld", origTrack->gap);
            av_dict_set(&s->metadata, key, value, 0);
            
            snprintf(key, 255, "track_%d_origin_gap", i);
            snprintf(value, 255, "%lld", origTrack->origin_gap);
            av_dict_set(&s->metadata, key, value, 0);
            
            //title
            char *title = sb_handle->area[sb_handle->twoch_area_idx].area_track_text[i].track_type_title;
            if (title)
            {
                snprintf(key, 255, "track_%d_title", i);
                snprintf(value, 255, "%s", title);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            //album
            master_text_t *master_text = &sb_handle->master_text;
            char *album_title = 0;

            if (master_text->album_title)
                album_title = master_text->album_title;
            else if (master_text->album_title_phonetic)
                album_title = master_text->album_title_phonetic;
            else if (master_text->disc_title)
                album_title = master_text->disc_title;
            else if (master_text->disc_title_phonetic)
                album_title = master_text->disc_title_phonetic;

            if (album_title)
            {
                snprintf(key, 255, "track_%d_album", i);
                snprintf(value, 255, "%s", album_title);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            
            // Track Artists (Artist name /performer)
            char *artist = sb_handle->area[sb_handle->twoch_area_idx].area_track_text[i].track_type_performer;
            if (artist)
            {
                snprintf(key, 255, "track_%d_artist", i);
                snprintf(value, 255, "%s", artist);
                av_dict_set(&s->metadata, key, value, 0);
            }
   
            char *album_artist = 0;
            if (master_text->disc_artist)
                album_artist = master_text->disc_artist;
            else if (master_text->disc_artist_phonetic)
                album_artist = master_text->disc_artist_phonetic;
            else if (master_text->album_artist)
                album_artist = master_text->album_artist;
            else if (master_text->album_artist_phonetic)
                album_artist = master_text->album_artist_phonetic;
            if (album_artist)
            {
                snprintf(key, 255, "track_%d_album_artist", i);
                snprintf(value, 255, "%s", album_artist);
                av_dict_set(&s->metadata, key, value, 0);
                if (artist == 0) {
                    snprintf(key, 255, "track_%d_artist", i);
                    snprintf(value, 255, "%s", album_artist);
                    av_dict_set(&s->metadata, key, value, 0);
                }
            }
            
            char *composer = sb_handle->area[sb_handle->twoch_area_idx].area_track_text[i].track_type_composer;
            if (composer)
            {
                snprintf(key, 255, "track_%d_composer", i);
                snprintf(value, 255, "%s", composer);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            // ISCR
            if (&sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->isrc[i])
            {
                char isrc[16];
                
                memcpy(isrc, sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->isrc[i].country_code, 2);
                memcpy(isrc + 2, sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->isrc[i].owner_code, 3);
                memcpy(isrc + 5, sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->isrc[i].recording_year, 2);
                memcpy(isrc + 7, sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->isrc[i].designation_code, 5);
                isrc[12] = 0x00;

                snprintf(key, 255, "track_%d_iscr", i);
                snprintf(value, 255, "%s", isrc);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            // Publisher
            if (sb_handle->master_text.album_publisher)
            {
                char *publisher = sb_handle->master_text.album_publisher;
                snprintf(key, 255, "track_%d_publisher", i);
                snprintf(value, 255, "%s", publisher);
                av_dict_set(&s->metadata, key, value, 0);
            }

            // Copyright
            if (sb_handle->master_text.album_copyright)
            {
                char *copyright = sb_handle->master_text.album_copyright;
                snprintf(key, 255, "track_%d_copyright", i);
                snprintf(value, 255, "%s", copyright);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            // Part of set. Disc sequence/set size
            if (sb_handle->master_toc)
            {
                master_toc_t *mtoc = sb_handle->master_toc;
                snprintf(key, 255, "track_%d_track_disc", i);
                snprintf(value, 255, "%d/%d", mtoc->album_sequence_number, mtoc->album_set_size);
                av_dict_set(&s->metadata, key, value, 0);
            }
            
            // Genre
            char *genre = (char *)genre_table[sacd_id3_genres[sb_handle->area[sb_handle->twoch_area_idx].area_isrc_genre->track_genre[i].genre & 0x1f]];
            if (genre ) {
                snprintf(key, 255, "track_%d_genre", i);
                snprintf(value, 255, "%s", genre);
                av_dict_set(&s->metadata, key, value, 0);
            }

            // YEAR
            if (sb_handle->master_toc->disc_date_year > 0) {
                snprintf(key, 255, "track_%d_year", i);
                snprintf(value, 255, "%04d", sb_handle->master_toc->disc_date_year);
                av_dict_set(&s->metadata, key, value, 0);
            }

            // Month, day
            uint8_t day = sb_handle->master_toc->disc_date_day;
            uint8_t mon = sb_handle->master_toc->disc_date_month;
            if (day > 0 && mon > 0) {
                if (mon > 12) {
                    day = sb_handle->master_toc->disc_date_month;
                    mon =sb_handle->master_toc->disc_date_day;
                }
                snprintf(key, 255, "track_%d_mon", i);
                snprintf(value, 255, "%02d", mon);
                av_dict_set(&s->metadata, key, value, 0);
                snprintf(key, 255, "track_%d_day", i);
                snprintf(value, 255, "%02d", day);
                av_dict_set(&s->metadata, key, value, 0);
            }
        }
    }
    if (sb_handle) {
        scarletbook_close(sb_handle);
        sb_handle = 0;
    }
#else
    new_pos = avio_seek(pb, SACD_OFFSET + 8, SEEK_SET);
    if (new_pos < 0) {
        return AVERROR_INVALIDDATA;
    }
    tmp16 = avio_rb16(pb);
    new_pos = avio_seek(pb, 54, SEEK_CUR);
    if (new_pos < 0) {
        return AVERROR_INVALIDDATA;
    }
    
    tmp32 = avio_rl32(pb);
    areaTocOffset1 = (off_t)OSSwapBigToHostInt32(tmp32) * 2048;
    tmp32 = avio_rl32(pb);
    areaTocOffset2 = (off_t)OSSwapBigToHostInt32(tmp32) * 2048;
    if(!areaTocOffset1 && !areaTocOffset2){
        return AVERROR_INVALIDDATA;
    }
    
    uint8_t key[256] = {0}, value[256] = {0};
    
    unsigned int flagTRL = 0;
    if(areaTocOffset1) {
        int nSectors;
        //if(fseeko(fp,areaTocOffset1,SEEK_SET)) goto fail;
        new_pos = avio_seek(pb, areaTocOffset1, SEEK_SET);
        if (new_pos < 0) {
            return AVERROR_INVALIDDATA;
        }
        //if(fread(header,1,8,fp) != 8) goto fail;
        //if(memcmp(header,"TWOCHTOC",8)) goto fail;
        avio_read(pb, header, 8);
        if(memcmp(header,"TWOCHTOC",8) != 0){
            return AVERROR_INVALIDDATA;
        }
        
        //if(fseeko(fp,2,SEEK_CUR)) goto fail;
        new_pos = avio_skip(pb, 2);
        if(new_pos < 0){
            return AVERROR_INVALIDDATA;
        }
        
        //if(fread(&tmp16,2,1,fp) != 1) goto fail;
        tmp16 = avio_rl16(pb);
        nSectors = OSSwapBigToHostInt16(tmp16) - 1;
        
        //if(fseeko(fp,52,SEEK_CUR)) goto fail;
        new_pos = avio_seek(pb, 52, SEEK_CUR);
        if(new_pos < 0){
            return AVERROR_INVALIDDATA;
        }
        
        //if(fread(&tmp8,1,1,fp) != 1) goto fail;
        tmp8 = avio_r8(pb);
        totalSamples =  (int64_t)tmp8 * 75 * 60;
        
        //if(fread(&tmp8,1,1,fp) != 1) goto fail;
        tmp8 = avio_r8(pb);
        totalSamples +=  (int64_t)tmp8 * 75;
        
        //if(fread(&tmp8,1,1,fp) != 1) goto fail;
        tmp8 = avio_r8(pb);
        totalSamples +=  (int64_t)tmp8;
        totalSamples -= 150; /* 2 seconds offset */
        totalSamples = totalSamples * 37632;
        snprintf(value, 255, "%lld", totalSamples);
        av_dict_set(&s->metadata, "sacd_total_frames", value, 0);
        sacd->total_samples = totalSamples;
        sacd->sample_rate = 2822400;
        snprintf(value, 255, "%d", 2822400);
        av_dict_set(&s->metadata, "sacd_sample_rate", value, 0);
        //if(fseeko(fp,2,SEEK_CUR)) goto fail;
        new_pos = avio_skip(pb,  2);
        if (new_pos < 0) {
            return AVERROR_INVALIDDATA;
        }
        //if(fread(&tmp8,1,1,fp) != 1) goto fail;
        tmp8 = avio_r8(pb);
        numTracks = tmp8;
        sacd->num_tracks = numTracks;

        snprintf(value, 255, "%d", numTracks);
        av_dict_set(&s->metadata, "tracktotal", value, 0);
        if(numTracks <= 0){
            return AVERROR_INVALIDDATA;
        }
        
        sacd->tracks = malloc(sizeof(SACDISOTrack) * numTracks);
        
        /* create primary stream before any id3 coverart streams */
       st = avformat_new_stream(s, NULL);
       if (!st)
           return AVERROR(ENOMEM);
        //if(fseeko(fp,1978,SEEK_CUR)) goto fail;
        new_pos = avio_seek(pb, 1978, SEEK_CUR);
        if (new_pos < 0) {
            return AVERROR_INVALIDDATA;
        }
        
        while(nSectors > 0) {
            //if(fread(header,1,8,fp) != 8) goto fail;
            avio_read(pb, header, 8);
            if(!memcmp(header,"SACDTTxt",8)) {
                //if(fseeko(fp,2040,SEEK_CUR)) goto fail;
                new_pos = avio_seek(pb, 2040, SEEK_CUR);
                if(new_pos < 0){
                    return AVERROR_INVALIDDATA;
                }
                nSectors--;
            }
            else if(!memcmp(header,"SACD_IGL",8)) {
                //if(fseeko(fp,4088,SEEK_CUR)) goto fail;
                new_pos = avio_seek(pb, 4088, SEEK_CUR);
                if(new_pos < 0){
                    return AVERROR_INVALIDDATA;
                }
                nSectors -= 2;
            }
            else if(!memcmp(header,"SACD_ACC",8)) {
                //if(fseeko(fp,2048*32-8,SEEK_CUR)) goto fail;
                new_pos = avio_seek(pb, 2048*32-8, SEEK_CUR);
                if(new_pos < 0){
                    return AVERROR_INVALIDDATA;
                }
                nSectors -= 32;
            }
            else if(!memcmp(header,"SACDTRL1",8)) {
                int i;
                off_t start = avio_skip(pb, 0);
                for(i=0;i<numTracks;i++) {
                    //if(fread(&tmp32,4,1,fp) != 1) goto fail;
                    tmp32 = avio_rl32(pb);
                    sacd->trackLSN[i] = OSSwapBigToHostInt32(tmp32);
                }
                //if(fseeko(fp,4*254,SEEK_CUR)) goto fail;
                new_pos = avio_seek(pb, 4*254, SEEK_CUR);
                if(new_pos < 0){
                    return AVERROR_INVALIDDATA;
                }
               //if(fread(&tmp32,4,1,fp) != 1) goto fail;
                tmp32 = avio_rl32(pb);
                sacd->trackLSN[numTracks] = sacd->trackLSN[numTracks-1] + OSSwapBigToHostInt32(tmp32);
                new_pos = avio_seek(pb, start+2040, SEEK_SET);
                //if(fseeko(fp,start+2040,SEEK_SET)) goto fail;
                if (new_pos < 0) {
                    return AVERROR_INVALIDDATA;
                }
                nSectors--;
                flagTRL |= 1;
            }else if(!memcmp(header,"SACDTRL2",8)) {
                int i;
                //off_t start = ftello(fp);
                off_t start = avio_skip(pb, 0);
                int64_t previousFrame = 0;
                int64_t previousIndex = 0;
                
                for(i=0;i<numTracks;i++) {
                    SACDISOTrack *track = &(sacd->tracks[i]);
                    track->track_num = i+1;
                    //if(fread(&tmp32,4,1,fp) != 1) goto fail;
                    tmp32 = avio_rl32(pb);
                    tmp32 = OSSwapBigToHostInt32(tmp32);
                    int MM = tmp32 >> 24;
                    int SS = (tmp32 >> 16) & 0xff;
                    int FF = (tmp32 >> 8) & 0xff;
                    int64_t idx = (int64_t)MM * 60 * 75;
                    idx += (int64_t)SS * 75;
                    idx += FF;
                    idx -= 150; /* 2 seconds offset */
                    
                    
//                    XLDTrack *track = [[objc_getClass("XLDTrack") alloc] init];
//                    [[track metadata] setObject:[NSNumber numberWithInt:i+1] forKey:XLD_METADATA_TRACK];
//                    [[track metadata] setObject:[NSNumber numberWithInt:numTracks] forKey:XLD_METADATA_TOTALTRACKS];
                    //[track setIndex:idx];
                    track->index = idx;
                    //track.originIndex = idx;
                    track->origin_index = idx;
                    //if(fseeko(fp,4*254,SEEK_CUR)) goto fail;
                    new_pos = avio_seek(pb, 4*254, SEEK_CUR);
                    if(new_pos < 0){
                        return AVERROR_INVALIDDATA;
                    }
                    
                    //if(fread(&tmp32,4,1,fp) != 1) goto fail;
                    tmp32 = avio_rl32(pb);
                    tmp32 = OSSwapBigToHostInt32(tmp32);
                    MM = tmp32 >> 24;
                    SS = (tmp32 >> 16) & 0xff;
                    FF = (tmp32 >> 8) & 0xff;
                    int64_t duration = (int64_t)MM * 60 * 75;
                    duration += (int64_t)SS * 75;
                    duration += FF;
                    
                    //[track setFrames:duration];
                    track->total_frames = duration;
                    //track.originFrames = duration;
                    track->origin_frames = duration;
                    //[track setSeconds:MM*60 + SS + (double)FF/75.0];
                    double seconds = MM*60 + SS + (double)FF/75.0;
                    track->duration = seconds;
                    track->gap = 0;
                    track->origin_gap = 0;
                    if(i > 0) {
//                        XLDTrack *previous = [trackList objectAtIndex:i-1];
//                        xldoffset_t gap = [track index] - ([previous index] + [previous frames]);
//                        [track setGap:gap];
//                        track.originGap = gap;
                        int64_t gap = idx - (previousIndex + previousFrame);
                        previousIndex = idx;
                        previousFrame = duration;
                        track->gap = gap;
                        track->origin_gap = gap;
                    }else {
                        previousIndex = idx;
                        previousFrame = duration;
                    }
                    //[trackList addObject:track];
                    //if(fseeko(fp,-4*255,SEEK_CUR)) goto fail;
                    new_pos = avio_seek(pb, -4*255, SEEK_CUR);
                    if (new_pos < 0) {
                        return AVERROR_INVALIDDATA;
                    }
                    flagTRL |= 2;
                }
                
                double scale = (double)sacd->sample_rate/ 8.0 / 75.0;
                for(i=0;i<numTracks;i++) {
                    SACDISOTrack *origTrack = &(sacd->tracks[i]);
                    
                    double index = scale * origTrack->index + 0.5;
                    double frames = scale * origTrack->total_frames + 0.5;
                    double gap = scale * origTrack->gap + 0.5;
                    
                    origTrack->index = (int64_t)index;
                    origTrack->total_frames = (int64_t)frames;
                    origTrack->gap = (int64_t)gap;

                    if(i > 0) {
                        SACDISOTrack *prev = &(sacd->tracks[i-1]);
                        if(prev->gap) {
                            origTrack->gap = origTrack->index - (prev->index + prev->total_frames);
                        }
                        else {
                            prev->total_frames = origTrack->index - prev->index;
                        }
                    }
                }
                
                //set metadata
                for(i=0;i<numTracks;i++) {
                    SACDISOTrack *track = &(sacd->tracks[i]);
                    snprintf(key, 255, "track_%d_track_number", i);
                    snprintf(value, 255, "%d", i+1);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_index", i);
                    snprintf(value, 255, "%d", track->index);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_origin_index", i);
                    snprintf(value, 255, "%d", track->origin_index);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d__total_frames", i);
                    snprintf(value, 255, "%lld", track->total_frames);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_origin_frames", i);
                    snprintf(value, 255, "%lld", track->origin_frames);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_dutaion", i);
                    snprintf(value, 255, "%f", track->duration);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_gap", i);
                    snprintf(value, 255, "%lld", track->gap);
                    av_dict_set(&s->metadata, key, value, 0);
                    
                    snprintf(key, 255, "track_%d_origin_gap", i);
                    snprintf(value, 255, "%lld", track->origin_gap);
                    av_dict_set(&s->metadata, key, value, 0);
                }
                
                //if(fseeko(fp,start+2040,SEEK_SET)) goto fail;
                new_pos = avio_seek(pb, start+2040, SEEK_SET);
                if (new_pos < 0) {
                    return AVERROR_INVALIDDATA;
                }
                nSectors--;
            }
            else {
                //if(fseeko(fp,2040,SEEK_CUR)) goto fail;
                new_pos = avio_seek(pb, 2040, SEEK_SET);
                if (new_pos < 0) {
                    return AVERROR_INVALIDDATA;
                }
                nSectors--;
            }
        }
    }
    if(flagTRL != 3){
        return AVERROR_INVALIDDATA;
    }
#endif
    sacd->currentLSN = sacd->trackLSN[0];
    off_t offset = (off_t)sacd->currentLSN * 2048;
    new_pos = avio_seek(pb, offset, SEEK_SET);
    if (new_pos < 0) {
        return AVERROR_INVALIDDATA;
    }
    st->codecpar->channels = 2;
    
    st->codecpar->ch_layout = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    st->codecpar->ch_layout.nb_channels = 2;
    st->codecpar->codec_type   = AVMEDIA_TYPE_AUDIO;
    st->codecpar->sample_rate  = sacd->sample_rate /8 ;
    st->codecpar->codec_id = AV_CODEC_ID_DSD_MSBF;
    st->codecpar->format = AV_SAMPLE_FMT_FLT;
    //st->codecpar->bits_per_raw_sample = 8;
    sacd->block_size = 16384 * 2;
    sacd->total_pcm_samples = totalSamples / 8;
    st->duration = totalSamples;
    sacd->dsd_samples_per_block = sacd->block_size * 8 / 2;
    sacd->dsd_bytes_per_block = sacd->block_size / 2;
    sacd->pcm_sample_per_block = sacd->dsd_bytes_per_block;
    sacd->total_blocks = totalSamples / sacd->dsd_samples_per_block;
    sacd->current_block = 0;
    sacd->last_block_dsd_bytes = (totalSamples - sacd->total_blocks * sacd->dsd_samples_per_block) / 8;
    if (sacd->last_block_dsd_bytes) {
        sacd->total_blocks++;
    }else{
        sacd->last_block_dsd_bytes = sacd->dsd_bytes_per_block;
    }
    //st->codecpar->block_align = 8;
    //st->codecpar->bits_per_coded_sample = 8;
    
    
    //st->codecpar->bit_rate = sacd->sample_rate * st->codecpar->channels;
    sacd->buffer = malloc(588*2*8*8);
    sacd->dstBuffer = malloc(2048*6);
    sacd->bytesInDSTBuffer = 0;
    sacd->bytesInBuffer = 0;
    avpriv_set_pts_info(st, 64, 1, 2822400);
    sacd->dstDecoder = calloc(1,sizeof(ebunch));
    DSTDecoderInit(sacd->dstDecoder,2,64);
    sacd_seek(s, 0);
    return 0;
}

static int sacd_read(AVFormatContext *s, unsigned char* buf, int size){
    SACDISOContext *sacd = s->priv_data;
    AVIOContext *pb = s->pb;
    unsigned char tmp8;
    unsigned short tmp16;
    int read = 0;
    
    if(sacd->bytesInBuffer) {
        if(sacd->bytesInBuffer <= size) {
            memcpy(buf,sacd->buffer,sacd->bytesInBuffer);
            size -= sacd->bytesInBuffer;
            read = sacd->bytesInBuffer;
            sacd->bytesInBuffer = 0;
        }
        else {
            memcpy(buf,sacd->buffer,size);
            sacd->bytesInBuffer -= size;
            memmove(sacd->buffer,sacd->buffer+size,sacd->bytesInBuffer);
            read = size;
            size = 0;
        }
    }
    
    while(size && sacd->currentLSN < sacd->trackLSN[sacd->num_tracks]) {
        int nPacketInfo;
        int nFrameInfo;
        int dstEncoded;
        int frameStart[7] = {0};
        int packetType[7] = {0};
        int packetLength[7] = {0};
        off_t sectorBegin = avio_tell(pb);
        
        //int ret = fread(&tmp8,1,1,fp);
        //if(ret < 1) break;
        tmp8 = avio_r8(pb);
        nPacketInfo = tmp8 >> 5;
        nFrameInfo = (tmp8 >> 2) & 0x07;
        dstEncoded = tmp8 & 1;
        int i;
        for(i=0;i<nPacketInfo;i++) {
//            ret = fread(&tmp16,2,1,fp);
//            if(ret < 1) break;
            tmp16 = avio_rl16(pb);
            tmp16 = OSSwapBigToHostInt16(tmp16);
            frameStart[i] = tmp16 >> 15;
            packetType[i] = (tmp16 >> 11) & 0x7;
            packetLength[i] = tmp16 & 0x7ff;
        }
        for(i=0;i<nFrameInfo;i++) {
            if(dstEncoded) {
                //fseeko(fp,4,SEEK_CUR);
                avio_seek(pb, 4, SEEK_CUR);
            }else {
                avio_seek(pb, 3, SEEK_CUR);
                //fseeko(fp,3,SEEK_CUR);
            }
        }
        int ret = 0;
        for(i=0;i<nPacketInfo;i++) {
            if(packetType[i] == 2) {
                if(!dstEncoded) {
                    if(packetLength[i] <= size) {
                        //ret = fread(buf+read,1,packetLength[i],fp);
                        ret = avio_read(pb, buf+read, packetLength[i]);
                        size -= ret;
                        read += ret;
                        if(ret < packetLength[i]){
                            break;
                        }
                        
                    }
                    else {
                        if(size) {
                            //ret = fread(buf+read,1,size,fp);
                            ret = avio_read(pb, buf+read, size);
                            size -= ret;
                            read += ret;
                            if(size != 0){
                                break;
                            }
                        }else{
                            ret = 0;
                        }
                        //ret = fread(buffer+bytesInBuffer,1,packetLength[i]-ret,fp);
                        ret = avio_read(pb, sacd->buffer+sacd->bytesInBuffer, packetLength[i]-ret);
                        sacd->bytesInBuffer += ret;
                    }
                }
                else {
                    if(frameStart[i] && sacd->bytesInDSTBuffer) {
                        ret = DSTDecoderDecode(sacd->dstDecoder, sacd->dstBuffer, sacd->buffer+sacd->bytesInBuffer, 0, &(sacd->bytesInDSTBuffer));
                        sacd->bytesInBuffer += 588*2*8;
                        sacd->bytesInDSTBuffer = 0;
                        if(sacd->bytesInBuffer <= size) {
                            memcpy(buf+read,sacd->buffer,sacd->bytesInBuffer);
                            size -= sacd->bytesInBuffer;
                            read += sacd->bytesInBuffer;
                            sacd->bytesInBuffer = 0;
                        }
                        else {
                            memcpy(buf+read,sacd->buffer,size);
                            sacd->bytesInBuffer -= size;
                            memmove(sacd->buffer,sacd->buffer+size,sacd->bytesInBuffer);
                            read += size;
                            size = 0;
                        }
                    }
                    //ret = fread(dstBuffer+bytesInDSTBuffer,1,packetLength[i],fp);
                    ret = avio_read(pb, sacd->dstBuffer+sacd->bytesInDSTBuffer, packetLength[i]);
                    sacd->bytesInDSTBuffer += ret;
                }
            }else{
                //fseeko(fp,packetLength[i],SEEK_CUR);
                avio_seek(pb, packetLength[i], SEEK_CUR);
            }
        }
        //fseeko(fp, sectorBegin+2048, SEEK_SET);
        avio_seek(pb, sectorBegin+2048, SEEK_SET);
        sacd->currentLSN++;
    }
    return read;
}

static int sacd_iso_read_packet(AVFormatContext *s, AVPacket *pkt)
{
    FFFormatContext *const si = ffformatcontext(s);
    SACDISOContext *sacd = s->priv_data;
    AVIOContext *pb = s->pb;
    AVStream *st = s->streams[0];
    int64_t pos = avio_tell(pb);
    int channels = st->codecpar->ch_layout.nb_channels;
    int ret;
    int copySize = (int)sacd->block_size;
    uint8_t *dst;
    if (sacd->current_block > sacd->total_blocks){
        return AVERROR_EOF;
    }else if (sacd->current_block < sacd->total_blocks){
        if ((ret = av_new_packet(pkt, copySize)) < 0)
            return ret;

        dst = pkt->data;
        ret = sacd_read(s, dst, copySize);
    }else{
        // ==
        copySize = FFMIN(sacd->last_block_dsd_bytes * channels, sacd->block_size);
        if ((ret = av_new_packet(pkt, copySize)) < 0){
            return ret;
        }
        dst = pkt->data;
        ret = sacd_read(s, dst, copySize);
        
    }
    //8 is 2822400 / 352800, becasue we set avpriv_set_pts_info(st, 64, 1, 2822400), not avpriv_set_pts_info(st, 64, 1, 352800), so here we need to multiply 8 to match the time_base.den
    pkt->pts = sacd->current_block * sacd->pcm_sample_per_block * 8 ;
    
    sacd->current_block++;
        
    pkt->pos = -1;
    pkt->size = ret;
    pkt->stream_index = 0;
    //8 is 2822400 / 352800, becasue we set avpriv_set_pts_info(st, 64, 1, 2822400), not avpriv_set_pts_info(st, 64, 1, 352800), so here we need to multiply 8 to match the time_base.den
    pkt->duration = ret * 8 / channels;
//    double seconds = (double)(pkt->pts) / 2822400.0;
//    double duration_sec = (double)pkt->duration * 8.0 / 2822400.0;
//    av_log(s, AV_LOG_ERROR, "sacd read pkt pts:%f, duration:%f\n", seconds, duration_sec);
    return 0;
}


static int sacd_iso_read_close(AVFormatContext *s)
{
    SACDISOContext *sacd = s->priv_data;
    if(sacd->tracks){
        free(sacd->tracks);
        sacd->tracks = NULL;
    }
    if (sacd->buffer) {
        free(sacd->buffer);
        sacd->buffer = NULL;
    }
    if (sacd->dstBuffer) {
        free(sacd->dstBuffer);
        sacd->dstBuffer = NULL;
    }
    if (sacd->dstDecoder) {
        DSTDecoderClose(sacd->dstDecoder);
        free(sacd->dstDecoder);
    }
    sacd->bytesInDSTBuffer = 0;
    sacd->bytesInBuffer = 0;
    return 0;
}

const AVInputFormat ff_sacd_iso_demuxer = {
    .name           = "sacd-iso",
    .long_name      = NULL_IF_CONFIG_SMALL("SACD ISO"),
    .priv_data_size = sizeof(SACDISOContext),
    .read_probe     = sacd_iso_probe,
    .read_header    = sacd_iso_read_header,
    .read_packet    = sacd_iso_read_packet,
    .read_close     = sacd_iso_read_close,
    .read_seek      = sacd_iso_seek,
    .flags          = AVFMT_GENERIC_INDEX | AVFMT_NO_BYTE_SEEK,
};
