/*
        OpenLase - a realtime laser graphics toolkit

Copyright (C) 2009-2011 Hector Martin "marcan" <hector@marcansoft.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 or version 3.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "libol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <getopt.h>
#include <unistd.h>

#include <jack/jack.h>
#include <fftw3.h>

#define DEFAULT_FFT_SIZE 512
#define DEFAULT_DEPTH 32

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;

typedef struct {
    int fft_size;
    int depth;
    float rot_x;
    float rot_y;
    float rot_z;
    int show_axes;
} SpecConfig;

SpecConfig config;

// JACK ports
jack_port_t *in_l;
jack_port_t *in_r;
jack_port_t *out_x;
jack_port_t *out_y;
jack_port_t *out_r;
jack_port_t *out_g;
jack_port_t *out_b;
nframes_t rate;

// FFT
float *fft_input;
fftwf_complex *fft_output;
fftwf_plan fft_plan;

float **magnitude_buffer;
int buffer_write_pos = 0;
int fft_fill_pos = 0;

void init_fft(void)
{
    fft_input = (float *)fftwf_malloc(sizeof(float) * config.fft_size);
    fft_output = (fftwf_complex *)fftwf_malloc(sizeof(fftwf_complex) * (config.fft_size / 2 + 1));
    fft_plan = fftwf_plan_dft_r2c_1d(config.fft_size, fft_input, fft_output, FFTW_ESTIMATE);

    magnitude_buffer = (float **)malloc(sizeof(float *) * config.depth);
    for (int i = 0; i < config.depth; i++) {
        magnitude_buffer[i] = (float *)calloc(config.fft_size / 2 + 1, sizeof(float));
    }
}

void cleanup_fft(void)
{
    fftwf_destroy_plan(fft_plan);
    fftwf_free(fft_input);
    fftwf_free(fft_output);

    for (int i = 0; i < config.depth; i++) {
        free(magnitude_buffer[i]);
    }
    free(magnitude_buffer);
}

// Simple 3D projection without libol
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y; } Vec2;

Vec2 project_3d(Vec3 p) {
    // Apply rotations
    float cx = cosf(config.rot_x), sx = sinf(config.rot_x);
    float cy = cosf(config.rot_y), sy = sinf(config.rot_y);
    float cz = cosf(config.rot_z), sz = sinf(config.rot_z);

    // Rotate around Y
    float tx = p.x * cy + p.z * sy;
    float tz = -p.x * sy + p.z * cy;
    p.x = tx; p.z = tz;

    // Rotate around X
    float ty = p.y * cx - p.z * sx;
    tz = p.y * sx + p.z * cx;
    p.y = ty; p.z = tz;

    // Rotate around Z
    tx = p.x * cz - p.y * sz;
    ty = p.x * sz + p.y * cz;
    p.x = tx; p.y = ty;

    // Simple perspective
    float scale = 1.0f / (3.0f + p.z * 0.2f);
    Vec2 result = { p.x * scale, p.y * scale };
    return result;
}

int process(nframes_t nframes, void *arg)
{
    sample_t *i_l = (sample_t *) jack_port_get_buffer(in_l, nframes);
    sample_t *i_r = (sample_t *) jack_port_get_buffer(in_r, nframes);
    sample_t *o_x = (sample_t *) jack_port_get_buffer(out_x, nframes);
    sample_t *o_y = (sample_t *) jack_port_get_buffer(out_y, nframes);
    sample_t *o_r = (sample_t *) jack_port_get_buffer(out_r, nframes);
    sample_t *o_g = (sample_t *) jack_port_get_buffer(out_g, nframes);
    sample_t *o_b = (sample_t *) jack_port_get_buffer(out_b, nframes);

    // Process audio input for FFT
    for (nframes_t frm = 0; frm < nframes; frm++) {
        if (fft_fill_pos < config.fft_size) {
            float mono = (*i_l++ + *i_r++) / 2.0f;
            float hann_window = 0.5f * (1.0f - cosf(2.0f * M_PI * fft_fill_pos / (config.fft_size - 1)));
            fft_input[fft_fill_pos] = mono * hann_window;
            fft_fill_pos++;

            if (fft_fill_pos == config.fft_size) {
                fftwf_execute(fft_plan);

                int num_bins = config.fft_size / 2 + 1;
                buffer_write_pos = (buffer_write_pos + 1) % config.depth;

                for (int j = 0; j < num_bins; j++) {
                    float re = fft_output[j][0];
                    float im = fft_output[j][1];
                    float mag = sqrtf(re * re + im * im) / config.fft_size;
                    magnitude_buffer[buffer_write_pos][j] = mag * 5.0f;
                }

                fft_fill_pos = 0;
            }
        }
    }

    // Render 3D spectrogram to output
    int num_bins = config.fft_size / 2 + 1;
    nframes_t out_idx = 0;

    for (int slice = 0; slice < config.depth && out_idx < nframes; slice++) {
        int buffer_idx = (buffer_write_pos - slice + config.depth) % config.depth;

        for (int bin = 0; bin < num_bins && out_idx < nframes; bin += 2) {
            float mag = magnitude_buffer[buffer_idx][bin];

            if (mag > 0.01f) {
                if (mag > 2.0f) mag = 2.0f;

                // Draw vertical line from base to magnitude
                Vec3 p1 = { bin * 0.015f - 3.8f, -0.4f, slice * 0.05f - 0.8f };
                Vec3 p2 = { bin * 0.015f - 3.8f, -0.4f + mag * 0.8f, slice * 0.05f - 0.8f };

                Vec2 proj1 = project_3d(p1);
                Vec2 proj2 = project_3d(p2);

                // Output two points (line)
                if (out_idx < nframes) {
                    o_x[out_idx] = proj1.x;
                    o_y[out_idx] = proj1.y;
                    o_r[out_idx] = 1.0f;
                    o_g[out_idx] = 1.0f;
                    o_b[out_idx] = 1.0f;
                    out_idx++;
                }
                if (out_idx < nframes) {
                    o_x[out_idx] = proj2.x;
                    o_y[out_idx] = proj2.y;
                    o_r[out_idx] = 1.0f;
                    o_g[out_idx] = 1.0f;
                    o_b[out_idx] = 1.0f;
                    out_idx++;
                }
            }
        }
    }

    // Fill remaining with blanking
    for (; out_idx < nframes; out_idx++) {
        o_x[out_idx] = 0.0f;
        o_y[out_idx] = 0.0f;
        o_r[out_idx] = 0.0f;
        o_g[out_idx] = 0.0f;
        o_b[out_idx] = 0.0f;
    }

    return 0;
}

int bufsize(nframes_t nframes, void *arg)
{
    printf("Buffer size: %u\n", nframes);
    return 0;
}

int srate(nframes_t nframes, void *arg)
{
    rate = nframes;
    printf("Sample rate: %u/sec\n", nframes);
    return 0;
}

void jack_shutdown(void *arg)
{
    exit(1);
}

void usage(const char *argv0)
{
    printf("Usage: %s [options]\n\n", argv0);
    printf("3D Waterfall Spectrogram for OpenLase\n\n");
    printf("Options:\n");
    printf("  -f SIZE    FFT size (default: %d, must be power of 2)\n", DEFAULT_FFT_SIZE);
    printf("  -d DEPTH   Number of time slices / waterfall depth (default: %d)\n", DEFAULT_DEPTH);
    printf("  -x ANGLE   X-axis rotation in degrees (default: -35.264 for isometric)\n");
    printf("  -y ANGLE   Y-axis rotation in degrees (default: 45 for isometric)\n");
    printf("  -z ANGLE   Z-axis rotation in degrees (default: 0)\n");
    printf("  -a         Hide axes (axes shown by default)\n");
    printf("  -h         Show this help\n");
}

int main(int argc, char *argv[])
{
    int opt;
    jack_client_t *client;
    jack_status_t jack_status;

    config.fft_size = DEFAULT_FFT_SIZE;
    config.depth = DEFAULT_DEPTH;
    config.rot_x = -35.264f * M_PI / 180.0f;
    config.rot_y = 45.0f * M_PI / 180.0f;
    config.rot_z = 0.0f;
    config.show_axes = 1;

    while ((opt = getopt(argc, argv, "f:d:x:y:z:ah")) != -1) {
        switch (opt) {
            case 'f':
                config.fft_size = atoi(optarg);
                if (config.fft_size <= 0 || (config.fft_size & (config.fft_size - 1)) != 0) {
                    fprintf(stderr, "Error: FFT size must be a power of 2\n");
                    return 1;
                }
                break;
            case 'd':
                config.depth = atoi(optarg);
                if (config.depth <= 0) {
                    fprintf(stderr, "Error: Depth must be positive\n");
                    return 1;
                }
                break;
            case 'x':
                config.rot_x = atof(optarg) * M_PI / 180.0f;
                break;
            case 'y':
                config.rot_y = atof(optarg) * M_PI / 180.0f;
                break;
            case 'z':
                config.rot_z = atof(optarg) * M_PI / 180.0f;
                break;
            case 'a':
                config.show_axes = 0;
                break;
            case 'h':
                usage(argv[0]);
                return 0;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    printf("3D Spectrogram Settings:\n");
    printf("  FFT Size: %d\n", config.fft_size);
    printf("  Depth: %d slices\n", config.depth);
    printf("  Rotation: X=%.1f° Y=%.1f° Z=%.1f°\n",
           config.rot_x * 180.0f / M_PI,
           config.rot_y * 180.0f / M_PI,
           config.rot_z * 180.0f / M_PI);
    printf("  Axes: %s\n", config.show_axes ? "shown" : "hidden");

    // Initialize FFT
    init_fft();

    // Initialize JACK client
    if ((client = jack_client_open("spectrogram3d", JackNullOption, &jack_status)) == 0) {
        fprintf(stderr, "JACK server not running?\n");
        return 1;
    }

    jack_set_process_callback(client, process, 0);
    jack_set_buffer_size_callback(client, bufsize, 0);
    jack_set_sample_rate_callback(client, srate, 0);
    jack_on_shutdown(client, jack_shutdown, 0);

    in_l = jack_port_register(client, "in_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    in_r = jack_port_register(client, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    out_x = jack_port_register(client, "out_x", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_y = jack_port_register(client, "out_y", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_r = jack_port_register(client, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_g = jack_port_register(client, "out_g", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    out_b = jack_port_register(client, "out_b", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (jack_activate(client)) {
        fprintf(stderr, "Cannot activate JACK client\n");
        return 1;
    }

    printf("Running... Press Ctrl+C to exit\n");
    printf("Connect audio sources with:\n");
    printf("  jack_connect <source> spectrogram3d:in_l\n");
    printf("  jack_connect <source> spectrogram3d:in_r\n");

    while (1) {
        sleep(1);
    }

    cleanup_fft();
    jack_client_close(client);

    return 0;
}
