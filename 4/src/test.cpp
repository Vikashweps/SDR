#include "SDR_include.h"
#include <iostream>
#include <complex.h>
#include <vector>

using namespace std;

vector <complex <float>> bpsk_mapper(vector<int> array){
    vector <complex <float>> samples;
    for (int i = 0; i < array.size(); i++) {
        complex <float> val;
        if (array[i] == 1) {
            val = 1.0 + 0.0j;
        } else {
            val = -1.0 + 0.0j;
        }
        samples.push_back(val);
    }
    return samples;
}

vector <complex <float>> upsampling(vector<complex <float>> & samples, int samples_per_symbol) {
    vector<complex<float>> upsampled;
    upsampled.resize(samples.size() * samples_per_symbol);
    int j = 0;
    for (int i = 0; i < upsampled.size(); i += samples_per_symbol) 
    {
        upsampled[i] = samples[j];
        j++;

    }
    return upsampled;
}

vector <complex <float>> convolve(vector<complex <float>> upsampled, int samples_per_symbol) {
    vector <complex<float>> convolved;
    vector <float> b(samples_per_symbol, 1.0f);
    for (int i = 0; i < upsampled.size(); i += b.size()) 
        {
            for (int n = 0; n < b.size(); n++) 
            {
                float summ_re = 0.0;
                float summ_im = 0.0;
                for (int k = 0; k < b.size(); k++) 
                {
                    summ_re += upsampled[i+k].real() * b[n-k];
                    summ_im += upsampled[i+k].imag() * b[n-k];
                }

                convolved.push_back(complex<float>(summ_re, summ_im));
            }
        }
    return convolved;
}

vector<float> offset(vector<complex <float>> matched, int samples_per_symbol)
{
    int K1, K2, p1, p2 = 0;
    int BnTs = 0.01;
    int Kp = 0.002;
    float zeta = sqrt(2) / 2;
    float theta = (BnTs / samples_per_symbol) / (zeta + (0.25 / zeta));
    K1 = -4 * zeta * theta / ( (1 + 2 * zeta * theta + pow(theta,2)) * Kp);
    K2 = -4 * pow(theta,2) / ( (1 + 2 * zeta * theta + pow(theta,2))* Kp);
    int tau = 0;
    float err;
    vector<float> errof;


    for (int i = 0; i < matched.size(); i += samples_per_symbol)
    {
        err = (matched[i + samples_per_symbol + tau].real() - matched[i + tau]).real() * matched[i + (samples_per_symbol / 2) + tau].real() + (matched[i + samples_per_symbol + tau].imag() - matched[i + tau]).imag() * matched[i + (samples_per_symbol / 2) + tau].imag(); 
        p1 = err * K1;
        p2 =  p2 + p1 + err * K2;

        if (p2 > 1)
        {
            p2 = p2 - 1;
        }

        if (p2 < -1)
        {
            p2 = p2 + 1;
        }
        tau = ceil(p2 * samples_per_symbol);
        errof.push_back(i + samples_per_symbol + tau);
        }
        return errof;
    }

int main(){
    int size = 10;
    vector<int>array  = {1, 0, 1, 0, 1, 0, 1, 0, 1, 0};
    
    vector<complex<float>> sample = bpsk_mapper(array);
    cout << "samples" << endl;
    for(int i = 0; i < sample.size(); i++) 
    {
        cout << sample[i] << " " ;
    }
    cout << endl;

    cout << "upsampling" << endl;
    vector<complex<float>> upsample = upsampling(sample, size);
     for(int i = 0; i < upsample.size(); i++) 
    {
        cout << upsample[i] << " "  ;
    }
    cout << endl;

    cout << "convolve" << endl;
    vector<complex<float>> convolved = convolve(upsample, size);
     for(int i = 0; i < convolved.size(); i++) 
    {
        cout << convolved[i] << " "  ;
    }
    cout << endl;
    
    cout << "matched filter" << endl;
    vector<complex<float>> matched = convolve(convolved, size);
     for(int i = 0; i < matched.size(); i++) 
    {
        cout << matched[i] << " "  ;
    }
    cout << endl;

    cout << "errof" << endl;
    vector<float> errof = offset(matched, size);
     for(int i = 0; i < size; i++) 
    {
        cout << errof[i] << " "  ;
    }
    cout << endl;
    

}