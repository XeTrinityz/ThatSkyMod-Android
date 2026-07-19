#pragma once
#include <stdexcept>
#include <cmath>

struct vec4
{
	float x, y, z, w;

	constexpr vec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f)
	{
	}

	constexpr vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w)
	{
	}

	constexpr vec4(const vec4& _other) : x(_other.x), y(_other.y), z(_other.z), w(_other.w)
	{
	}

	float& operator[](int _index)
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		if (_index == 2) return z;
		if (_index == 3) return w;
		throw std::out_of_range("Index out of range for vec4");
	}

	const float& operator[](int _index) const
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		if (_index == 2) return z;
		if (_index == 3) return w;
		throw std::out_of_range("Index out of range for vec4");
	}

	vec4& operator+=(const vec4& _rhs)
	{
		x += _rhs.x;
		y += _rhs.y;
		z += _rhs.z;
		w += _rhs.w;
		return *this;
	}

	vec4& operator-=(const vec4& _rhs)
	{
		x -= _rhs.x;
		y -= _rhs.y;
		z -= _rhs.z;
		w -= _rhs.w;
		return *this;
	}

	vec4& operator*=(float _scalar)
	{
		x *= _scalar;
		y *= _scalar;
		z *= _scalar;
		w *= _scalar;
		return *this;
	}

	vec4& operator/=(float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		x /= _scalar;
		y /= _scalar;
		z /= _scalar;
		w /= _scalar;
		return *this;
	}

	vec4 operator-() const
	{
		return vec4(-x, -y, -z, -w);
	}

	friend vec4 operator+(const vec4& _lhs, const vec4& _rhs)
	{
		return vec4(_lhs.x + _rhs.x, _lhs.y + _rhs.y, _lhs.z + _rhs.z, _lhs.w + _rhs.w);
	}

	friend vec4 operator-(const vec4& _lhs, const vec4& _rhs)
	{
		return vec4(_lhs.x - _rhs.x, _lhs.y - _rhs.y, _lhs.z - _rhs.z, _lhs.w - _rhs.w);
	}

	friend vec4 operator*(const vec4& _v, float _scalar)
	{
		return vec4(_v.x * _scalar, _v.y * _scalar, _v.z * _scalar, _v.w * _scalar);
	}

	friend vec4 operator*(float _scalar, const vec4& _v)
	{
		return _v * _scalar;
	}

	friend vec4 operator/(const vec4& _v, float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		return vec4(_v.x / _scalar, _v.y / _scalar, _v.z / _scalar, _v.w / _scalar);
	}

	friend bool operator==(const vec4& _lhs, const vec4& _rhs)
	{
		return std::abs(_lhs.x - _rhs.x) < 1e-6
			&& std::abs(_lhs.y - _rhs.y) < 1e-6
			&& std::abs(_lhs.z - _rhs.z) < 1e-6
			&& std::abs(_lhs.w - _rhs.w) < 1e-6;
	}

	friend bool operator!=(const vec4& _lhs, const vec4& _rhs)
	{
		return !(_lhs == _rhs);
	}
};
