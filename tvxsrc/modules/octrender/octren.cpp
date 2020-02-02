
#include "morton.h"
#include "octren.hpp"

namespace tvx {

	uint64_t VoxelDword::mortonize(glm::uvec3 pos) {
		return libmorton::morton3D_32_encode(pos.x, pos.y, pos.z);
	}
	glm::uvec3 VoxelDword::demortonize(uint64_t m) {
		uint_fast32_t x, y, z;
		libmorton::morton3D_32_decode(m, x, y, z);
		return glm::uvec3(x, y, z);
	}
	
	uint32_t VoxelDword::getBits() {
		return *reinterpret_cast<uint32_t*>(this);
	}
	
	[[nodiscard]] uint_fast8_t VoxelDword::getChildMasked(uint_fast8_t mask) const {
		return data[0] & mask;
	}
	[[nodiscard]] bool VoxelDword::getIsFilled() const { return data[1] & 0b00000001u; }
	[[nodiscard]] bool VoxelDword::getIsMetal() const { return data[1] & 0b00000010u; }
	[[nodiscard]] uint_fast8_t VoxelDword::getRed() const { return (data[1] & 0b00011100u) >> 2u; }
	[[nodiscard]] uint_fast8_t VoxelDword::getGreen() const { return (data[1] & 0b11100000u) >> 5u; }
	[[nodiscard]] uint_fast8_t VoxelDword::getBlue() const { return (data[2] & 0b00000111u); }
	[[nodiscard]] uint_fast8_t VoxelDword::getNormal() const { return (data[2] & 0b11111000u) >> 3u; }
	[[nodiscard]] uint_fast8_t VoxelDword::getRoughness() const { return data[3] & 0b00001111u; }
	[[nodiscard]] uint_fast8_t VoxelDword::getLightness() const { return (data[3] & 0b11110000u) >> 4u; }

	void VoxelDword::setBits(uint32_t in) {
		*this = *reinterpret_cast<VoxelDword*>(&in);
	}
	void VoxelDword::setIsFilled(bool in) {
		if (in) {
			data[1] |= 0b00000001u;
		} else {
			data[1] &= 0b11111110u;
		}
	}
	void VoxelDword::setIsMetal(bool in) {
		if (in) {
			data[1] |= 0b00000010u;
		} else {
			data[1] &= 0b11111101u;
		}
	}
	void VoxelDword::setRed(uint_fast8_t in) {
		data[1] = (data[1] & 0b11100011u) | ((uint8_t)(in << 2u) & 0b00011100u);
	}
	void VoxelDword::setGreen(uint_fast8_t in) {
		data[1] = (data[1] & 0b00011111u) | ((uint8_t)(in << 5u) & 0b11100000u);
	}
	void VoxelDword::setBlue(uint_fast8_t in) {
		data[2] = (data[2] & 0b11111000u) | (in & 0b00000111u);
	}
	void VoxelDword::setNormal(uint_fast8_t in) {
		data[2] = (data[2] & 0b00000111u) | ((uint8_t)(in << 3u) & 0b11111000u);
	}
	void VoxelDword::setRoughness(uint_fast8_t in) {
		data[3] = (data[3] & 0b11110000u) | (in & 0b00001111u);
	}
	void VoxelDword::setLightness(uint_fast8_t in) {
		data[3] = (data[3] & 0b00001111u) | ((uint8_t)(in << 4u) & 0b11110000u);
	}
}