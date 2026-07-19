#pragma once
#include <stdexcept>
#include <cmath>

struct vec3
{
	float x, y, z;

	constexpr vec3() : x(0.0f), y(0.0f), z(0.0f)
	{
	}

	constexpr vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z)
	{
	}

	constexpr vec3(const vec3& _other) : x(_other.x), y(_other.y), z(_other.z)
	{
	}

	float& operator[](int _index)
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		if (_index == 2) return z;
		throw std::out_of_range("Index out of range for vec3");
	}

	const float& operator[](int _index) const
	{
		if (_index == 0) return x;
		if (_index == 1) return y;
		if (_index == 2) return z;
		throw std::out_of_range("Index out of range for vec3");
	}

	vec3& operator+=(const vec3& _rhs)
	{
		x += _rhs.x;
		y += _rhs.y;
		z += _rhs.z;
		return *this;
	}

	vec3& operator-=(const vec3& _rhs)
	{
		x -= _rhs.x;
		y -= _rhs.y;
		z -= _rhs.z;
		return *this;
	}

	vec3& operator*=(float _scalar)
	{
		x *= _scalar;
		y *= _scalar;
		z *= _scalar;
		return *this;
	}

	vec3& operator/=(float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		x /= _scalar;
		y /= _scalar;
		z /= _scalar;
		return *this;
	}

	vec3 operator-() const
	{
		return vec3(-x, -y, -z);
	}

	friend vec3 operator+(const vec3& _lhs, const vec3& _rhs)
	{
		return vec3(_lhs.x + _rhs.x, _lhs.y + _rhs.y, _lhs.z + _rhs.z);
	}

	friend vec3 operator-(const vec3& _lhs, const vec3& _rhs)
	{
		return vec3(_lhs.x - _rhs.x, _lhs.y - _rhs.y, _lhs.z - _rhs.z);
	}

	friend vec3 operator*(const vec3& _v, float _scalar)
	{
		return vec3(_v.x * _scalar, _v.y * _scalar, _v.z * _scalar);
	}

	friend vec3 operator*(float _scalar, const vec3& v)
	{
		return v * _scalar;
	}

	friend vec3 operator/(const vec3& _v, float _scalar)
	{
		if (_scalar == 0.0f)
		{
			throw std::invalid_argument("Division by zero");
		}
		return vec3(_v.x / _scalar, _v.y / _scalar, _v.z / _scalar);
	}

	friend bool operator==(const vec3& _lhs, const vec3& rhs)
	{
		return std::abs(_lhs.x - rhs.x) < 1e-6
			&& std::abs(_lhs.y - rhs.y) < 1e-6
			&& std::abs(_lhs.z - rhs.z) < 1e-6;
	}

	friend bool operator!=(const vec3& _lhs, const vec3& rhs)
	{
		return !(_lhs == rhs);
	}

	float dot(const vec3& _rhs) const
	{
		return x * _rhs.x + y * _rhs.y + z * _rhs.z;
	}

	vec3 cross(const vec3& _rhs) const
	{
		return vec3(
			y * _rhs.z - z * _rhs.y,
			z * _rhs.x - x * _rhs.z,
			x * _rhs.y - y * _rhs.x
		);
	}

	float distance_to(const vec3& _other) const
	{
		float dx = x - _other.x;
		float dy = y - _other.y;
		float dz = z - _other.z;
		return std::sqrt(dx * dx + dy * dy + dz * dz);
	}
};
