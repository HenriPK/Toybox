
#pragma once

#include "extern.hpp"
#include "morton.h"
#include "buffers.hpp"
#include "topics.hpp"

namespace tvx {
	enum class OctCoordCartesian {
			XNYNZN = 0,
			XNYNZP,
			XNYPZN,
			XNYPZP,
			XPYNZN,
			XPYNZP,
			XPYPZN,
			XPYPZP,
	};
	
	class VoxelDword {
			uint8_t data[4] = {};
		public:
			
			[[nodiscard]] uint32_t getBits();
			[[nodiscard]] uint_fast8_t getChildMasked(uint_fast8_t mask) const;
			[[nodiscard]] bool getIsFilled() const;
			[[nodiscard]] bool getIsMetal() const;
			[[nodiscard]] uint_fast8_t getRed() const;
			[[nodiscard]] uint_fast8_t getGreen() const;
			[[nodiscard]] uint_fast8_t getBlue() const;
			[[nodiscard]] uint_fast8_t getNormal() const;
			[[nodiscard]] uint_fast8_t getRoughness() const;
			[[nodiscard]] uint_fast8_t getLightness() const;
			void setBits(uint32_t in);
			void setChildOn(OctCoordCartesian which);
			void setChildOff(OctCoordCartesian which);
			void setChildren(uint8_t in);
			void setIsFilled(bool in);
			void setIsMetal(bool in);
			void setRed(uint_fast8_t in);
			void setGreen(uint_fast8_t in);
			void setBlue(uint_fast8_t in);
			void setNormal(uint_fast8_t in);
			void setRoughness(uint_fast8_t in);
			void setLightness(uint_fast8_t in);
			
			/*
			 * Lightness goes from darkened (occluded) to acting as a light emitter. Also the extreme darkest value codes for
			 * "true black" which reflects no light, while the extreme lightest value codes for "sky."
			 */
	};

	template<uint_fast64_t maxLvl>
	class Voctree {
		public:
			explicit Voctree() {
				buftex = std::make_unique<BufferTexture<totalCount * sizeof(VoxelDword)>>();
			}

			void generate() { fillCpu(true); }
			void recalcInterior() { fillCpu(false); }
			void cascadeChanges(uint_fast64_t accIdx) { nuclearAccumulators[accIdx].cascade(nuclearAccumulators); }
			void sendToGpu(GLenum texUnit) {
				buftex->sendToGpu();
				buftex->use(texUnit);
			}
			struct VoxelRayResult {
				VoxelDword *vox = nullptr;
				glm::uvec3 pos = glm::uvec3(0);
				glm::uvec3 norm = glm::uvec3(0);
				float dist = 2.f; // serves as maxDist
				uint_fast32_t steps = 0;
				bool hit = false;
			};
			VoxelRayResult ray(const glm::vec3 &raySrc, const glm::vec3 &rayDir) {
				VoxelRayResult res;
				rayMarch(raySrc, rayDir, res);
				return res;
			}
			VoxelDword &at(const glm::uvec3 &pos) {
				uint_fast64_t morton = libmorton::morton3D_32_encode(pos.x, pos.y, pos.z);
				uint_fast64_t accumIdx = morton / 8;
				uint_fast8_t voxIdx = morton % 8;
				return *(nuclearAccumulators[accumIdx].voxel - 8 + voxIdx);
			}

			static constexpr uint_fast32_t dimMax = power(2, maxLvl);
			static constexpr float toMeters = 2.f / dimMax;

		private:

			static constexpr int whichScene = 4;
			
			static constexpr uint_fast64_t leafCount = power(8, maxLvl); // FIXME: clang's sprout::pow is off-by-one?
			static constexpr uint_fast64_t scndCount = power(8, maxLvl - 1);
			static constexpr uint_fast64_t valenceCount = leafCount + scndCount;
			static constexpr uint_fast64_t nuclearCount = (power(8, maxLvl - 1) - 1) / 7;
			static constexpr uint_fast64_t totalCount = (power(8, maxLvl + 1) - 1) / 7;
			
			static constexpr uint_fast32_t steps =  100;
			static constexpr uint_fast32_t vox_nil =  0;
			static constexpr uint_fast32_t vox_subd =  1;
			static constexpr uint_fast32_t vox_empty =  2;
			static constexpr uint_fast32_t vox_brick =  3;
			static constexpr float small = 0.001f;

			class Accumulator {
				public:
					
					// parent should never be 0 and be valid - use 0 to mean uninitialized parent.
					uint_fast64_t parentAccum = 0, firstChild = 0;
					VoxelDword *voxel = nullptr;
					void reset(VoxelDword *v, bool isValence = false) {
						voxel = v;
						valence = isValence;
						red = 0;
						green = 0;
						blue = 0;
						metal = 0;
						rough = 0;
						light = 0;
						which = 0;
						count = 0;
					}
					void calculate(std::array<Accumulator, nuclearCount + scndCount> &accumulators) {
						if (valence) {
							for (int i = -8; i < 0; ++i) {
								add(*(voxel + i));
							}
						} else {
							for (int i = 0; i < 8; ++i) {
								add(*accumulators[firstChild + i].voxel);
							}
						}
						avg();
					}
					void cascade(std::array<Accumulator, nuclearCount + scndCount> &accumulators) {
						reset(voxel, valence);
						calculate(accumulators);
						if (parentAccum) {
							accumulators[parentAccum].cascade(accumulators);
						}
					}
					
				private:

					uint_fast8_t red = 0, green = 0, blue = 0, metal = 0, rough = 0, light = 0, which = 0, count = 0;
					bool valence = false;
					void add(const VoxelDword &v) {
						assert(which < 8);
						if (!v.getIsFilled()) {
							voxel->setChildOff(static_cast<OctCoordCartesian>(which++));
							return;
						}
						++count;
						voxel->setChildOn(static_cast<OctCoordCartesian>(which++));
						red += v.getRed();
						green += v.getGreen();
						blue += v.getBlue();
						metal += v.getIsMetal();
						rough += v.getRoughness();
						light += v.getLightness();
					}
					void add(VoxelDword &&v) { add(v); }
					bool isDirty() { return which; }
					void avg() {
						if (!count) {
							voxel->setIsFilled(false);
							return;
						}
						voxel->setRed(red / count);
						voxel->setGreen(green / count);
						voxel->setBlue(blue / count);
						voxel->setNormal(26);
						voxel->setIsFilled(true);
						voxel->setIsMetal(metal / count);
						voxel->setRoughness(rough / count);
						voxel->setLightness(light / count);
					}
			};
			
			std::unique_ptr<BufferTexture<totalCount * sizeof(VoxelDword)>> buftex;
			std::array<Accumulator, nuclearCount + scndCount> nuclearAccumulators;
			
			VoxelDword getMortonColors(uint_fast64_t morton) {
				VoxelDword voxel;
				uint_fast16_t x, y, z;
				libmorton::morton3D_32_decode(morton, x, y, z);
				bool isFilled = ! (morton % 5);
				voxel.setIsFilled(isFilled);
				if (isFilled) {
					float cycler = 0.0043f * morton;
					voxel.setRed((7.f / 2.f) * (1 + sinf(cycler)));
					voxel.setGreen((7.f / 2.f) * (1 + sinf(cycler + (float)M_PI * (2.f / 3.f))));
					voxel.setBlue((7.f / 2.f) * (1 + sinf(cycler + (float)M_PI * (4.f / 3.f))));
					voxel.setRoughness(15);
					voxel.setLightness(0);
				}
				return voxel;
			}
			
			VoxelDword getWorstCase(uint_fast64_t morton) {
				VoxelDword voxel;
				uint_fast16_t x, y, z;
				libmorton::morton3D_32_decode(morton, x, y, z);
				bool isFilled = (y < 1) || (x < 1) || (z < 1);
				voxel.setIsFilled(isFilled);
				if (isFilled) {
					voxel.setRed(x * 0.5f);
					voxel.setGreen(y * 0.5f);
					voxel.setBlue(z * 0.5f);
				}
				return voxel;
			}

			VoxelDword getFlat(uint_fast64_t morton) {
				VoxelDword voxel;
				uint_fast16_t x, y, z;
				libmorton::morton3D_32_decode(morton, x, y, z);
				bool isFilled = (y < pow(2, maxLvl - 2));
				voxel.setIsFilled(isFilled);
				if (isFilled) {
					uint_fast32_t scale = dimMax / 8; 
					voxel.setRed(x / scale);
					voxel.setGreen(z / scale);
					voxel.setBlue(8 - (x + y + z) / scale / 2);
				}
				return voxel;
			}
			
			VoxelDword getAntisphere(uint_fast64_t morton) {
				VoxelDword voxel;
				uint_fast16_t x, y, z;
				libmorton::morton3D_32_decode(morton, x, y, z);
				static constexpr uint_fast32_t lo = dimMax / 4 - 1, hi = dimMax * 3 / 4 - 1, md = dimMax / 2;
				float fromCenter = glm::length(glm::vec3(x, y, z) / static_cast<float>(dimMax) - glm::vec3(0.5f));
				bool isFilled = (x > lo && x <= hi) && (y > lo && y <= hi) && (z > lo && z <= hi) && fromCenter > 0.32f;
				voxel.setIsFilled(isFilled);
				if (isFilled) {
					float radial0 = (fromCenter - .305f) * (dimMax * 0.24);
					float radial1 = (fromCenter - .313f) * (dimMax * 0.24);
					voxel.setRed(radial0);
					voxel.setGreen(0);
					voxel.setBlue(radial1);
				}
				return voxel;
			}

			VoxelDword getDebugCorners(uint_fast64_t morton) {
				VoxelDword voxel;
				uint_fast16_t x, y, z;
				libmorton::morton3D_32_decode(morton, x, y, z);
				bool isFilled = (x == 0 || x == 15) && (y == 0 || y == 15) && (z == 0 || z == 15);
				voxel.setIsFilled(isFilled);
				if (isFilled) {
					voxel.setRed(x * 0.5f);
					voxel.setGreen(y * 0.5f);
					voxel.setBlue(z * 0.5f);
				}
				return voxel;
			}

			void fillCpu(bool generate) {
				uint_fast64_t valenceIdx = 0, leafIdx = 0, nuclearStart = valenceCount / 2, accumIdx = 0;
				int_fast64_t nuclearCrct = 0;
				for (; valenceIdx < valenceCount + nuclearCount; ++valenceIdx) {
					if (valenceIdx == nuclearStart) { // JUMP THE NUCLEUS once the middle of the leaves is reached
						nuclearCrct -= nuclearCount;
						valenceIdx += nuclearCount;
					}
					VoxelDword voxel;
					if ((valenceIdx + nuclearCrct + 1) % 9) { // working on a leaf node
						if ( ! generate) { continue; }
						switch(whichScene) {
							case 0: voxel = getAntisphere(leafIdx++); break;
							case 1: voxel = getMortonColors(leafIdx++); break;
							case 2: voxel = getDebugCorners(leafIdx++); break;
							case 3: voxel = getWorstCase(leafIdx++); break;
							case 4: voxel = getFlat(leafIdx++); break;
							default: break;
						}
						buftex->template writeToCpu<VoxelDword>(valenceIdx, voxel);
					} else { // working on a level 1 interior node (just above the leaves)
						nuclearAccumulators[accumIdx++].reset(buftex->template cpu<VoxelDword>(valenceIdx), true);
					}
				}
				uint_fast64_t nuclearIdx = 0, nuclearInvLvl = 2, nuclearAvgHead = 0;
				for (; nuclearInvLvl <= maxLvl; ++nuclearInvLvl) {
					uint_fast32_t lvlLimit = pow(8, maxLvl - nuclearInvLvl);
					for (uint_fast32_t inLvl = 0; inLvl < lvlLimit; ++inLvl) {
						nuclearAccumulators[accumIdx].firstChild = nuclearAvgHead;
						uint_fast64_t nuclearAvgLimit = nuclearAvgHead + 8;
						for (; nuclearAvgHead < nuclearAvgLimit; ++nuclearAvgHead) {
							nuclearAccumulators[nuclearAvgHead].parentAccum = accumIdx;
						}
						nuclearAccumulators[accumIdx++].reset(buftex->template cpu<VoxelDword>(nuclearStart + nuclearIdx++));
					}
				}
				for (auto &accumulator : nuclearAccumulators) {
					accumulator.calculate(nuclearAccumulators);
				}
			}


			/*
			 * Raymarching routines for picking voxels are as similar as possible to those found in the shader.
			 */
			static inline uint_fast32_t sumOfPowers(uint_fast32_t top) { return (glm::pow(8, top) - 1) / 7; }
			void octDwordGet(VoxelDword *&voxel, uint_fast32_t lvl, glm::vec3 pos) {
				uint_fast32_t invLvl = maxLvl - lvl, lvlOffset = 0, linearIdx = 0;
				float voxInvScale = glm::pow(2, lvl);
				glm::vec3 upos(pos * voxInvScale);
				linearIdx = libmorton::morton3D_32_encode(upos.x, upos.y, upos.z);
				switch(invLvl) {
					case 0: { lvlOffset += (linearIdx < leafCount / 2 ? 0 : nuclearCount) + (linearIdx) / 8; } break;
					case 1: {
						lvlOffset += 8 + ((linearIdx < scndCount / 2) ? 0 : nuclearCount);
						linearIdx *= 9;
					} break;
					default: {
						lvlOffset += (leafCount + scndCount) / 2 + glm::max(0u, sumOfPowers(maxLvl - 1) - sumOfPowers(lvl + 1));
					}
				}
				voxel = buftex->template cpu<VoxelDword>(static_cast<uint_fast64_t>(linearIdx + lvlOffset));
			}
			uint_fast32_t getVoxelEffect(glm::vec3 pos, float size, VoxelDword *&voxel) {
				if (glm::any(glm::greaterThan(glm::abs(pos * 1.0001f - 0.5f), glm::vec3(0.5f)))) { return vox_nil; }
				auto lv = static_cast<uint_fast32_t>(glm::log2(1.f / size));
				octDwordGet(voxel, lv, pos);
				
				if (voxel->getIsFilled()) {
					if (voxel->getChildMasked(0xFF)) {
						return vox_subd;
					}
					return vox_brick;
				}
				return vox_empty;
			}
			glm::vec3 voxelHit(glm::vec3 raySrc, glm::vec3 rayDir, float size) {
				size *= 0.5f;
				glm::vec3 hit = -(glm::sign(rayDir) * (raySrc - size) - size) / glm::max(glm::abs(rayDir), small);
				return hit;
			}
			void rayMarch(glm::vec3 raySrc, glm::vec3 rayDir, VoxelRayResult &res) {
				float childSize = 0.5f, maxdist = res.dist;
				res.dist = 0.f;
				glm::vec3 raySrcInSub = glm::mod(raySrc, childSize), raySrcInCur = raySrc - raySrcInSub, dirs(0), prevDirs(0);
				bool levelUp = false;
				int recur = 0, recurr = 0, recurrr = 0, curVoxEffect = 0;

				if (glm::any(glm::greaterThan(glm::abs(raySrc - 0.5f), glm::vec3(0.5f)))) { return; }
				glm::vec3 hit = voxelHit(raySrcInSub, rayDir, childSize);

				for (; res.steps < steps; ++res.steps) {

					if (res.dist >= maxdist) { break; }
					int nextVoxEffect = 0;
					if (recurrr == recur) { curVoxEffect = int(getVoxelEffect(raySrcInCur, childSize, res.vox)); }
					bool isNil = recurr < recur || nextVoxEffect == vox_nil;
					if (isNil) { nextVoxEffect = curVoxEffect; }

					if (levelUp) { // go up a level

						if (recurr == recur) { recurr--; }
						if (recurrr == recur) { recurrr--; }
						glm::vec3 newfro = glm::floor(raySrcInCur / childSize * 0.5f + 0.25f) * childSize * 2.f;
						raySrcInSub += raySrcInCur - newfro;
						raySrcInCur = newfro;
						recur--;
						childSize *= 2.0;
						hit = voxelHit(raySrcInSub, rayDir, childSize);
						if (recur < 0) break;
						levelUp = (glm::abs(glm::dot(glm::mod(raySrcInCur / childSize + 0.5f, 2.f) -
						                             1.f + dirs * glm::sign(rayDir) * 0.5f, dirs)) < 0.1f);

					} else if (nextVoxEffect == vox_subd) { // subdivide

						recur++;
						if (curVoxEffect == vox_subd) { recurrr++; }
						childSize *= 0.5;
						glm::vec3 mask2 = glm::step(glm::vec3(childSize), raySrcInSub); // which of 8
						raySrcInCur += mask2 * childSize;
						raySrcInSub -= mask2 * childSize;
						hit = voxelHit(raySrcInSub, rayDir, childSize);

					} else if (nextVoxEffect == vox_nil || nextVoxEffect == vox_empty) { // forward
						
						if (hit.x < glm::min(hit.y, hit.z)) { dirs = glm::vec3(1, 0, 0); }
						else if (hit.y < hit.z) { dirs = glm::vec3(0, 1, 0); }
						else { dirs = glm::vec3(0, 0, 1); }
						float len = glm::dot(hit, dirs);
						hit -= len;
						hit += dirs * (1.f / glm::max(glm::abs(rayDir), small)) * childSize;
						raySrcInSub += rayDir * len - dirs * glm::sign(rayDir) * childSize;
						glm::vec3 newfro = raySrcInCur + dirs * glm::sign(rayDir) * childSize;
						res.dist += len;
						levelUp = (glm::floor(newfro / childSize * 0.5f + 0.25f) !=
						           glm::floor(raySrcInCur / childSize * 0.5f + 0.25f));
						raySrcInCur = newfro;
						prevDirs = dirs;

					} else { break; }
				}
				res.pos = (static_cast<float>(dimMax) * raySrcInCur);
				res.norm = glm::uvec3(-dirs * glm::sign(rayDir));
				res.hit = res.steps < steps && res.vox && res.vox->getIsFilled();
			}
	};
}