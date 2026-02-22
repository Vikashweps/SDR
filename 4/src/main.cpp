#include "SDR_include.h"

void bpsk_mapper(int* bits, iq_sample_t* iq_samples, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        bits[i] = rand() % 2;
        if (bits[i] == 1) {
            iq_samples[i].i = 1.0f; 
            iq_samples[i].q = 0.0f;
        } else {
            iq_samples[i].i = -1.0f; 
            iq_samples[i].q = 0.0f;
        }
    }
}

void upsampling(const iq_sample_t* input, iq_sample_t* output, int num_bits) {
    for (int i = 0; i < num_bits; i++) {
        output[i * 10].i = input[i].i;
        output[i * 10].q = input[i].q;
        for (int j = 1; j < 10; j++) {
            output[i * 10 + j].i = 0.0f;
            output[i * 10 + j].q = 0.0f;
        }
    }
}

void convolve(const iq_sample_t* input, iq_sample_t* output, int num_upsampled) {
    const int L = 10;
    float h[L];
    for (int i = 0; i < L; i++) {
        h[i] = 1.0f; 
    }

    for (int n = 0; n < num_upsampled; n++) {
        float a_i = 0.0f;
        float a_q = 0.0f;

        for (int k = 0; k < L; k++) {
            int input_index = n - k;
            if (input_index >= 0) {
                a_i += input[input_index].i * h[k];
                a_q += input[input_index].q * h[k];
            }
        }

        output[n].i = a_i;
        output[n].q = a_q;
    }
}

void matched_filter(const iq_sample_t* input, iq_sample_t* output, int num_samples) {
    const int L = 10;
    float h[L];
    for (int i = 0; i < L; i++) {
        h[i] = 1.0f;
    }

    for (int n = 0; n < num_samples; n++) {
        float a_i = 0.0f;
        float a_q = 0.0f;

        for (int k = 0; k < L; k++) {
            int input_index = n - k;
            if (input_index >= 0 && input_index < num_samples) {
                a_i += input[input_index].i * h[k];
                a_q += input[input_index].q * h[k];
            }
        }

        output[n].i = a_i;
        output[n].q = a_q;
    }
}

void amplify_signal(const iq_sample_t* input, iq_sample_t* output, int num_samples, float amplitude) {
    for (int i = 0; i < num_samples; i++) {
        output[i].i = input[i].i * amplitude;
        output[i].q = input[i].q * amplitude;
    }
}

void convert_iq_to_sdr(const iq_sample_t* iq_samples, int16_t* sdr_buffer, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        sdr_buffer[2*i] = (int16_t)(iq_samples[i].i * 32767.0f);
        sdr_buffer[2*i + 1] = (int16_t)(iq_samples[i].q * 32767.0f);
    }
}

void convert_sdr_to_iq(const int16_t* sdr_buffer, iq_sample_t* iq_samples, int num_samples) {
    for (int i = 0; i < num_samples; i++) {
        iq_samples[i].i = (float)sdr_buffer[2*i] / 32767.0f;
        iq_samples[i].q = (float)sdr_buffer[2*i + 1] / 32767.0f;
    }
}
int symbol_timing_recovery(const iq_sample_t* rx_samples, int num_samples, 
                           int start_skip, int Nsp, float K1, float K2,
                           float signal_scale, iq_sample_t* output_symbols, int max_symbols,
                           float* error_history, int* error_count){

    int ns = 0;
    float p2 = 0.0f;
    *error_count = 0;
    int final_offset = 0;
    
    if (signal_scale < 0.01f) signal_scale = 1.0f;
    float norm = 1.0f / (signal_scale * signal_scale);

    while (ns < max_symbols) {
        int n = start_skip + ns * Nsp + (int)round(p2);
        if (n + Nsp >= num_samples) break;
        
        
        float error = (rx_samples[n + Nsp].i - rx_samples[n].i) * rx_samples[n + Nsp/2].i;
        error *= norm;
        
        if (*error_count < max_symbols && error_history != NULL)
            error_history[(*error_count)++] = error;
        
        
        p2 = p2 + error * K1 + error * K2;
        final_offset = (int)round(p2);

        if (ns < 10) {
            printf("  ns=%d, n=%d, error=%.4f, p2=%.4f, offset=%d\n", 
                   ns, n, error, p2, final_offset);
        }
        int decision_point = n + Nsp/2;
        if (decision_point < num_samples) {
            output_symbols[ns].i = rx_samples[decision_point].i;
            output_symbols[ns].q = rx_samples[decision_point].q;
        }
        ns++;
    }
    printf("Final offset: %d (expected: 9)\n", final_offset);
    return ns;
}

int main() {
    printf("SDR Application Started\n");
    srand((unsigned int)time(NULL));
    
    sdr_device_t *sdr = sdr_init(1);
    if (sdr == NULL) return 1;
    if (sdr_configure(sdr) != 0) { sdr_cleanup(sdr); return 1; }

    int16_t *tx_buff = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
    int16_t *rx_buffer = (int16_t*)malloc(2 * sdr->rx_mtu * sizeof(int16_t));
    if (!tx_buff || !rx_buffer) {
        printf("ERROR: Failed to allocate SDR buffers\n");
        free(tx_buff); free(rx_buffer); sdr_cleanup(sdr); return 1;
    }

    const int Nsp = 10;
    const int num_bits = (sdr->tx_mtu / Nsp) * 0.9; 
    const int num_upsampled = num_bits * Nsp;
    
    printf("TX MTU: %zu, Using %d bits (%d samples)\n", sdr->tx_mtu, num_bits, num_upsampled);
    
    if ((size_t)num_upsampled > sdr->tx_mtu) {
        printf("ERROR: Signal too long! MTU=%zu, Need=%d\n", sdr->tx_mtu, num_upsampled);
        free(tx_buff); free(rx_buffer); sdr_cleanup(sdr); return 1;
    }
    
    int *bits = (int*)malloc(num_bits * sizeof(int));
    iq_sample_t *symbols = (iq_sample_t*)malloc(num_bits * sizeof(iq_sample_t));
    iq_sample_t *upsampled = (iq_sample_t*)malloc(num_upsampled * sizeof(iq_sample_t));
    iq_sample_t *amplified = (iq_sample_t*)malloc(num_upsampled * sizeof(iq_sample_t));
    iq_sample_t *filtered = (iq_sample_t*)malloc(num_upsampled * sizeof(iq_sample_t));
    
    
    bpsk_mapper(bits, symbols, num_bits);
    upsampling(symbols, upsampled, num_bits);
    convolve(upsampled, filtered, num_upsampled);
    amplify_signal(filtered, amplified, num_upsampled, 50.0f);
    convert_iq_to_sdr(amplified, tx_buff, num_upsampled);
    
    FILE *rx_file = fopen("rx_samples.pcm", "wb");
    FILE *tx_file = fopen("tx_samples.pcm", "wb");
    long long timeNs = 0;

    for (size_t i = 0; i < 10; i++) {
        int sr = sdr_read_samples(sdr, rx_buffer, &timeNs);
        if (sr > 0) fwrite(rx_buffer, sizeof(int16_t), 2 * sr, rx_file);
        if (i == 2) {
            printf("Transmitting...\n");
            int16_t *full_tx = (int16_t*)malloc(2 * sdr->tx_mtu * sizeof(int16_t));
            memcpy(full_tx, tx_buff, 2 * num_upsampled * sizeof(int16_t));
            for (size_t j = num_upsampled; j < sdr->tx_mtu; j++) {
                full_tx[2*j] = 0; full_tx[2*j+1] = 0;
            }
            sdr_write_samples(sdr, full_tx, timeNs + 4000000);
            free(full_tx);
        }
    }
    fclose(rx_file); fclose(tx_file);

    FILE *rx_input = fopen("rx_samples.pcm", "rb");
    fseek(rx_input, 0, SEEK_END);
    long file_size = ftell(rx_input);
    fseek(rx_input, 0, SEEK_SET);
    int num_rx = file_size / (2 * sizeof(int16_t));
    
    int16_t *raw_rx = (int16_t*)malloc(file_size);
    fread(raw_rx, 1, file_size, rx_input);
    fclose(rx_input);

    iq_sample_t *rx_iq = (iq_sample_t*)malloc(num_rx * sizeof(iq_sample_t));
    convert_sdr_to_iq(raw_rx, rx_iq, num_rx);

    iq_sample_t *filtered_rx = (iq_sample_t*)malloc(num_rx * sizeof(iq_sample_t));
    matched_filter(rx_iq, filtered_rx, num_rx);

    float max_amp = 0.0f;
    int skip = 0;
    for (int i = 0; i < num_rx - 100; i++) {
        float amp = fabs(filtered_rx[i].i);
        if (amp > max_amp) {
            max_amp = amp;
            skip = i;
        }
    }

    printf("Found signal at %d, amp=%.4f\n", skip, max_amp);

    const float K1 = 0.5;
    const float K2 = 0.5;
    
    
    iq_sample_t *recovered = (iq_sample_t*)malloc(num_bits * sizeof(iq_sample_t));
    float *errors = (float*)malloc(num_bits * sizeof(float));
    int err_count = 0;

    int rec_count = symbol_timing_recovery(filtered_rx, num_rx, skip, Nsp, K1, K2, max_amp, recovered, num_bits, errors, &err_count);

    printf("Recovered: %d symbols\n", rec_count);
    printf("Original : ");
    for (int i = 0; i < 20; i++) printf("%d", bits[i]);
    printf("\nRecovered: ");
    int bit_err = 0;
    for (int i = 0; i < 20 && i < rec_count; i++) {
        int b = (recovered[i].i > 0) ? 1 : 0;
        printf("%d", b);
        if (b != bits[i]) bit_err++;
    }
    printf("\nErrors: %d\n", bit_err);

cleanup:
    free(bits); free(symbols); free(upsampled); free(amplified); free(filtered);
    free(raw_rx); free(rx_iq); free(filtered_rx); 
    free(recovered); free(errors); 
    free(tx_buff); free(rx_buffer);
    sdr_cleanup(sdr);
    return 0;
}