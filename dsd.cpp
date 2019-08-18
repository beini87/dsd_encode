#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct DSDHeader {
    char chunk_header[4];
    uint64_t chunk_size;
    uint64_t file_size;
    uint64_t pointer_metadata;
} __attribute__ ((packed));

struct FMTHeader {
    char chunk_header[4];
    uint64_t chunk_size;
    uint32_t format_version;
    uint32_t format_id;
    uint32_t channel_type;
    uint32_t channel_num;
    uint32_t sample_rate;
    uint32_t bits_per_sample;
    uint64_t sample_count;
    uint32_t block_size_per_channel;
    uint32_t reserved;
} __attribute__ ((packed));

struct DataHeader {
    char chunk_header[4];
    uint64_t chunk_size;
} __attribute__ ((packed));

void write_dsd(size_t length, const uint8_t *bitstream, const char *filename) {
    FILE *file = fopen(filename, "wb");
    size_t total_file_size = 52 + 28 + length / 8 + 12;
    struct DSDHeader dsdheader = {
        {'D', 'S', 'D', ' '},
        28,
        total_file_size,
        0
    };
    fwrite((const void *)&dsdheader, sizeof(struct DSDHeader), 1, file);
    struct FMTHeader fmtheader = {
        {'f', 'm', 't', ' '},
        52,
        1,
        0,
        1,
        1,
        2822400,
        1,
        length,
        4096,
        0
    };
    fwrite((const void *)&fmtheader, sizeof(struct FMTHeader), 1, file);
    struct DataHeader dataheader = {
        {'d', 'a', 't', 'a'},
        12 + length / 8
    };
    fwrite((const void *)&dataheader, sizeof(struct DataHeader), 1, file);
    fwrite((const void*)bitstream, 1, length / 8, file);
    fclose(file);
}


struct ModulatorContext {
    double z1;
    double z2;
    uint8_t z;
};

extern void linear_upsample(const float *in, float *out, int rate, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        double k = 0;
        if (i != length - 1) {
            k = (in[i + 1] - in[i]) / rate;
        }
        for (size_t j = 0; j < rate; ++j) {
            out[i * rate + j] = in[i] + k * j;
        }
    }
}

extern void delta_sigma_modulate(struct ModulatorContext *ctx,
                                 const float *in, uint8_t *out, size_t length) {
    double z1 = ctx->z1;
    double z2 = ctx->z2;
    uint8_t z = ctx->z;
    double tmp_z2;
    for (size_t i = 0; i < length; ++i) {
        tmp_z2=in[i]/2+0.5 - z;
        // tmp_z2=(in[i]+1)/2 - z;
        z2 += tmp_z2 ;
        z1 += z2 - z;
        z = z1 >= 0 ? 1 : 0;
        out[i] = z;
        // printf("int[i]:%10f (in[i]+1)/2-z:%9lf z2:%9lf z1:%9lf z:%d\n",in[i],tmp_z2,z2,z1,z);
    }
    ctx->z1 = z1;
    ctx->z2 = z2;
    ctx->z = z;
}


int main(int arg,char **argv){
    const char *filename=argv[1];
    drwav *wav=drwav_open_file("./single.wav");
    size_t length=(wav->totalSampleCount+511)/512*512;
    // size_t length =64;
    // size_t length=4096;
    float *data=(float *)malloc(sizeof(float) * length);
    drwav_read_f32(wav,length,data);
    size_t length_resampled=length*64;
    float *data_resample=(float*)malloc(sizeof(float) * length_resampled);
    linear_upsample(data,data_resample,64,length);
    struct ModulatorContext ctx={0.0, 0.0, 0};
    uint8_t *out=(uint8_t *)malloc(sizeof(uint8_t) * length_resampled);
    delta_sigma_modulate(&ctx,data_resample,out,length_resampled);
    // for (size_t i=0; i<length;i++) {
    //     printf("%d",out[i]);
    // }
    size_t bytes=length_resampled/8;
    uint8_t *bitstream=(uint8_t*)malloc(bytes*sizeof(uint8_t));
    for(size_t i=0; i<bytes; i++) {
        bitstream[i]=out[i*8+0]<<7;
        bitstream[i]+=out[i*8+1]<<6;
        bitstream[i]+=out[i*8+2]<<5;
        bitstream[i]+=out[i*8+3]<<4;
        bitstream[i]+=out[i*8+4]<<3;
        bitstream[i]+=out[i*8+5]<<2;
        bitstream[i]+=out[i*8+6]<<1;
        bitstream[i]+=out[i*8+7];
    }
    write_dsd(length_resampled,bitstream,"./mydsd");
    // printf("this wav file's channels is %d;\notalSampleCount is :%d",wav->channels,wav->totalSampleCount);
    return 0;
}
