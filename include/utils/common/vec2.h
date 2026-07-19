#pragma once
#include <stdexcept>
#include <cmath>

struct vec2
{
	float x, y;

	constexpr vec2() : x(0.0f), y(0.0f)
	{
	}

	constexpr vec2(float _x, float _y) : x(_x), y(_y)
	{
	}

	constexpr vec2(const vec2& _other) : x(_other.x), y(_other.y)
	{
	}

	float& operator[](int _index)
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		throw std::out_of_range("Index out of range for vec2");
	}

	const float& operator[](int _index) const
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		throw std::out_of_range("Index out of range for vec2");
	}

	vec2& operator+=(const vec2& _rhs)
	{
		x += _rhs.x;
		y += _rhs.y;
		return *this;
	}

	vec2& operator-=(const vec2& _rhs)
	{
		x -= _rhs.x;
		y -= _rhs.y;
		return *this;
	}

	vec2& operator*=(float _scalar)
	{
		x *= _scalar;
		y *= _scalar;
		return *this;
	}

	vec2& operator/=(float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		x /= _scalar;
		y /= _scalar;
		return *this;
	}

	vec2 operator-() const
	{
		return vec2(-x, -y);
	}

	friend vec2 operator+(const vec2& _lhs, const vec2& _rhs)
	{
		return vec2(_lhs.x + _rhs.x, _lhs.y + _rhs.y);
	}

	friend vec2 operator-(const vec2& _lhs, const vec2& _rhs)
	{
		return vec2(_lhs.x - _rhs.x, _lhs.y - _rhs.y);
	}

	friend vec2 operator*(const vec2& _v, float _scalar)
	{
		return vec2(_v.x * _scalar, _v.y * _scalar);
	}

	friend vec2 operator*(float _scalar, const vec2& _v)
	{
		return _v * _scalar;
	}

	friend vec2 operator/(const vec2& _v, float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		return vec2(_v.x / _scalar, _v.y / _scalar);
	}

	friend bool operator==(const vec2& _lhs, const vec2& _rhs)
	{
		return std::abs(_lhs.x - _rhs.x) < 1e-6
			&& std::abs(_lhs.y - _rhs.y) < 1e-6;
	}

	friend bool operator!=(const vec2& _lhs, const vec2& _rhs)
	{
		return !(_lhs == _rhs);
	}

	float dot(const vec2& _rhs) const
	{
		return x * _rhs.x + y * _rhs.y;
	}

	float distance_to(const vec2& _other) const
	{
		float dx = x - _other.x;
		float dy = y - _other.y;
		return std::sqrt(dx * dx + dy * dy);
	}
};
