/*

	FFT libray
	Copyright (C) 2010 Didier Longueville
	Copyright (C) 2014 Enrique Condes

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "arduinoFFT.h"

#define sq(x) ((x)*(x))

uint8_t Exponent(uint16_t value);
void Swap(double *x, double *y);

double *_vReal;
double *_vImag;
uint16_t _samples;
double _samplingFrequency;
uint8_t _power;

void FFT_Init(double *vReal, double *vImag, uint16_t samples, double samplingFrequency)
{
	_vReal = vReal;
	_vImag = vImag;
	_samples = samples;
	_samplingFrequency = samplingFrequency;
	_power = Exponent(samples);
}

void FFT_Compute(uint8_t dir)
{// Computes in-place complex-to-complex FFT /
	// Reverse bits /
	uint16_t j = 0;
	for (uint16_t i = 0; i < (_samples - 1); i++) {
		if (i < j) {
			Swap(&_vReal[i], &_vReal[j]);
			if(dir==FFT_REVERSE)
				Swap(&_vImag[i], &_vImag[j]);
		}
		uint16_t k = (_samples >> 1);
		while (k <= j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}
	// Compute the FFT  /
	double c1 = -1.0;
	double c2 = 0.0;
	uint16_t l2 = 1;
	for (uint8_t l = 0; (l < _power); l++) {
		uint16_t l1 = l2;
		l2 <<= 1;
		double u1 = 1.0;
		double u2 = 0.0;
		for (j = 0; j < l1; j++) {
			 for (uint16_t i = j; i < _samples; i += l2) {
					uint16_t i1 = i + l1;
					double t1 = u1 * _vReal[i1] - u2 * _vImag[i1];
					double t2 = u1 * _vImag[i1] + u2 * _vReal[i1];
					_vReal[i1] = _vReal[i] - t1;
					_vImag[i1] = _vImag[i] - t2;
					_vReal[i] += t1;
					_vImag[i] += t2;
			 }
			 double z = ((u1 * c1) - (u2 * c2));
			 u2 = ((u1 * c2) + (u2 * c1));
			 u1 = z;
		}
		c2 = sqrt((1.0 - c1) / 2.0);
		if (dir == FFT_FORWARD) {
			c2 = -c2;
		}
		c1 = sqrt((1.0 + c1) / 2.0);
	}
	// Scaling for reverse transform /
	if (dir != FFT_FORWARD) {
		for (uint16_t i = 0; i < _samples; i++) {
			 _vReal[i] /= _samples;
			 _vImag[i] /= _samples;
		}
	}
}

void FFT_ComplexToMagnitude()
{ // vM is half the size of vReal and vImag
	for (uint16_t i = 0; i < _samples; i++) {
		_vReal[i] = sqrt(sq(_vReal[i]) + sq(_vImag[i]));
	}
}

void FFT_Windowing(uint8_t windowType, uint8_t dir)
{// Weighing factors are computed once before multiple use of FFT
// The weighing function is symetric; half the weighs are recorded
	double samplesMinusOne = ((double)_samples - 1.0);
	for (uint16_t i = 0; i < (_samples >> 1); i++) {
		double indexMinusOne = (double)i;
		double ratio = (indexMinusOne / samplesMinusOne);
		double weighingFactor = 1.0;
		// Compute and record weighting factor
		switch (windowType) {
		case FFT_WIN_TYP_RECTANGLE: // rectangle (box car)
			weighingFactor = 1.0;
			break;
		case FFT_WIN_TYP_HAMMING: // hamming
			weighingFactor = 0.54 - (0.46 * cos(twoPi * ratio));
			break;
		case FFT_WIN_TYP_HANN: // hann
			weighingFactor = 0.54 * (1.0 - cos(twoPi * ratio));
			break;
		case FFT_WIN_TYP_TRIANGLE: // triangle (Bartlett)
			weighingFactor = 1.0 - ((2.0 * abs(indexMinusOne - (samplesMinusOne / 2.0))) / samplesMinusOne);
			break;
		case FFT_WIN_TYP_BLACKMAN: // blackmann
			weighingFactor = 0.42323 - (0.49755 * (cos(twoPi * ratio))) + (0.07922 * (cos(fourPi * ratio)));
			break;
		case FFT_WIN_TYP_FLT_TOP: // flat top
			weighingFactor = 0.2810639 - (0.5208972 * cos(twoPi * ratio)) + (0.1980399 * cos(fourPi * ratio));
			break;
		case FFT_WIN_TYP_WELCH: // welch
			weighingFactor = 1.0 - sq((indexMinusOne - samplesMinusOne / 2.0) / (samplesMinusOne / 2.0));
			break;
		}
		if (dir == FFT_FORWARD) {
			_vReal[i] *= weighingFactor;
			_vReal[_samples - (i + 1)] *= weighingFactor;
		}
		else {
			_vReal[i] /= weighingFactor;
			_vReal[_samples - (i + 1)] /= weighingFactor;
		}
	}
}

double FFT_MajorPeak()
{
	double maxY = 0;
	uint16_t IndexOfMaxY = 0;
	//If sampling_frequency = 2 * max_frequency in signal,
	//value would be stored at position samples/2
	for (uint16_t i = 1; i < ((_samples >> 1) + 1); i++) {
		if ((_vReal[i-1] < _vReal[i]) && (_vReal[i] > _vReal[i+1])) {
			if (_vReal[i] > maxY) {
				maxY = _vReal[i];
				IndexOfMaxY = i;
			}
		}
	}
	double delta = 0.5 * ((_vReal[IndexOfMaxY-1] - _vReal[IndexOfMaxY+1]) / (_vReal[IndexOfMaxY-1] - (2.0 * _vReal[IndexOfMaxY]) + _vReal[IndexOfMaxY+1]));
	double interpolatedX = ((IndexOfMaxY + delta)  * _samplingFrequency) / (_samples-1);
	if(IndexOfMaxY==(_samples >> 1)) //To improve calculation on edge values
		interpolatedX = ((IndexOfMaxY + delta)  * _samplingFrequency) / (_samples);
	// retuned value: interpolated frequency peak apex
	return(interpolatedX);
}

uint8_t Exponent(uint16_t value)
{
	#warning("This method will not be accessible on future revisions.")
	// Calculates the base 2 logarithm of a value
	uint8_t result = 0;
	while (((value >> result) & 1) != 1) result++;
	return(result);
}

void Swap(double *x, double *y)
{
	double temp = *x;
	*x = *y;
	*y = temp;
}
