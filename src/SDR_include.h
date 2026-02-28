#ifndef SDR_APP_H
#define SDR_APP_H
using namespace std;
#include <SoapySDR/Device.h>
#include <SoapySDR/Formats.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <complex>
#include <math.h>
#include <vector>
#include <time.h>
#include <cstring>
#include <iostream>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>

// Заголовки для GUI
#include "third_party/implot/implot.h"
#include "third_party/imgui/backends/imgui_impl_sdl2.h"
#include "third_party/imgui/backends/imgui_impl_opengl3.h"
#include "third_party/imgui/imgui.h"


// Структура для IQ samples 
typedef struct {
    float i;
    float q;
} iq_sample_t;

// Структура устройства
typedef struct {
    SoapySDRDevice *device;
    SoapySDRStream *rxStream;
    SoapySDRStream *txStream;
    size_t rx_mtu;
    size_t tx_mtu;
    int sample_rate;
    int carrier_freq;
} sdr_device_t;

// Функции работы с устройством
sdr_device_t* sdr_init(int use_usb);
int sdr_configure(sdr_device_t *sdr);
int sdr_read_samples(sdr_device_t *sdr, int16_t *rx_buffer, long long *timeNs);
int sdr_write_samples(sdr_device_t *sdr, int16_t *tx_buff, long long tx_time);
void sdr_cleanup(sdr_device_t *sdr);

// Функции обработки сигналов
void genersdr_bufate_tx_signal(int16_t *tx_buff, size_t tx_mtu, long long tx_time);
void save_to_file(const char *filename, const int16_t *samples, size_t num_samples);

vector<complex<float>> bpsk_mapper(const vector<int>& array);
vector<complex<float>> upsampling(vector<complex<float>>& samples, int samples_per_symbol);
vector<complex<float>> convolve(vector<complex<float>> upsampled, int samples_per_symbol);
vector<float> offset(vector<complex<float>> matched, int samples_per_symbol);
vector<complex<float>> dounsampling(const vector<complex<float>>& matched, const vector<float>& errof);
void sdr_to_complex(const int16_t* sdr_buf, vector<complex<float>>& out, size_t num_samples);
void complex_to_sdr(const vector<complex<float>>& in, int16_t* sdr_buf, size_t num_samples);

// GUI
void run_gui(const std::vector<std::complex<float>>& sample,
             const std::vector<std::complex<float>>& upsample,
             const std::vector<std::complex<float>>& convolved,
             const std::vector<std::complex<float>>& matched,
             const std::vector<float>& errof,
             int samples_per_symbol,
             const std::vector<std::complex<float>> dounsample);

#endif