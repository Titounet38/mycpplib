

#include "Debug.h"
#include "math.h"

template <class T> class Complex{
public:

	void ToCartesian() {
		if (_state == Polar) {
			_real = _module*cos(_argument);
			_imag = _module*sin(_argument);
			_state = Both;
		}
	}

	void ToPolar() {
		if (_state == Cartesian) {
			_module = sqrt(_real*_real + _imag*_imag);
			_argument = atan2(_imag, _real);
			_state = Both;
		}
	}
	
	T _module, _argument, _real, _imag;
	
	enum State { Cartesian = 1, Polar = 2, Both = Cartesian + Polar } _state;

	Complex() {
		_module = _argument = _real = _imag = 0;
		_state = Both;
	}

	Complex(T a, T b, State st = Cartesian) {

		if (st == Cartesian) {
			_real = a;
			_imag = b;
		}
		else if (st == Polar) {
			_module = a;
			_argument = b;
		}
		else MY_ERROR("State must be = Polar or Cartesian");

		_state = st;
	}

	T Real() {
		ToCartesian();
		return _real;
	}

	T Imag() {
		ToCartesian();
		return _imag;
	}

	T Module() {
		ToPolar();
		return _module;
	}

	T Argument() {
		ToPolar();
		return _argument;
	}

	Complex & Rotate(T dPhi) {
		ToPolar();
		_argument += dPhi;
		_state = Polar;
		return *this;
	}

	Complex & SetReal(T x) {
		ToCartesian();
		_real = x;
		_state = Cartesian;
		return *this;
	}

	Complex & SetImag(T x) {
		ToCartesian();
		_imag = x;
		_state = Cartesian;
		return *this;
	}

	Complex & SetModule(T x) {
		ToPolar();
		_module = x;
		_state = Polar;
		return *this;
	}

	Complex & SetArgument(T x) {
		ToPolar();
		_argument = x;
		_state = Polar;
		return *this;
	}

	Complex & SetCartesian(T a, T b) {
		_real = a;
		_imag = b;
		_state = Cartesian;
		return *this;
	}

	Complex & SetPolar(T r, T t) {
		_module = r;
		_argument = t;
		_state = Polar;
		return *this;
	}

	void operator+=(Complex<T> & x) {
		x.ToCartesian();
		ToCartesian();
		_real += x._real;
		_imag += x._imag;
		_state = Cartesian;
	}

	void operator-=(Complex<T> & x) {
		x.ToCartesian();
		ToCartesian();
		_real -= x._real;
		_imag -= x._imag;
		_state = Cartesian;
	}

	void operator*=(Complex<T> & x) {
		x.ToPolar();
		ToPolar();
		_module *= x._module;
		_argument += x._argument;
		_state = Polar;
	}

	void operator/=(Complex<T> & x) {
		x.ToPolar();
		ToPolar();
		_module /= x._module;
		_argument -= x._argument;
		_state = Polar;
	}

	void operator+=(T x) {
		ToCartesian();
		_real += x._real;
		_state = Cartesian;
	}

	void operator-=(T x) {
		ToCartesian();
		_real -= x._real;
		_state = Cartesian;
	}

	void operator*=(T x) {
		if (_state == Both) {
			_module *= x;
			_real *= x;
			_imag *= x;
		}
		else if (_state == Polar) {
			_module *= x;
		}
		else {
			_real *= x;
			_imag *= x;
		}
	}

	void operator/=(T x) {
		if (_state == Both) {
			_module /= x;
			_real /= x;
			_imag /= x;
		}
		else if (_state == Polar) {
			_module /= x;
		}
		else {
			_real /= x;
			_imag /= x;
		}
	}

	Complex & operator=(T x) {
		this->_real = x;
		this->_imag = 0;
		this->_state = Cartesian;

		return  *this;
	}
};

template <class T> Complex<T> PolarComplex(T rho, T teta) {
	return Complex<T>(rho, teta, Complex<T>::State::Polar);
}

template <class T> Complex<T> Imag(T x) {
	return Complex<T>(0, x);
}

template <class T> Complex<T> operator+(Complex<T> & x, Complex<T> & y) {
	x.ToCartesian();
	y.ToCartesian();
	return Complex<T>(x._real + y._real, x._imag + y._imag);
}

template <class T> Complex<T> operator*(Complex<T> & x, Complex<T> & y) {
	x.ToPolar();
	y.ToPolar();
	return Complex<T>(x._module * y._module, x._argument + y._argument, Complex<T>::State::Polar);
}

template <class T> Complex<T> operator+(Complex<T> & x, T y) {
	x.ToCartesian();
	return Complex<T>(x._real + y, x._imag);
}

template <class T> Complex<T> operator+(T x, Complex<T> & y) {
	return y + x;
}

template <class T> Complex<T> operator*(Complex<T> & x, T y) {
	if (x._state & Complex<T>::State::Polar) {
		return Complex<T>(y * x._module, x._argument, Complex<T>::State::Polar);
	}
	else return Complex<T>(y * x._real, y * x._imag);
}

template <class T> Complex<T> operator*(T x, Complex<T> & y) {
	return y * x;
}