#pragma once

#include "library.h"

#include <cstdint>

/*
 * idTech3 / Enemy Territory math helpers and binary file format structures shared by the model loaders and writers.
 * All structures are little-endian and only contain 4 byte fields (or arrays of 2 byte fields in multiples of 4 bytes),
 * so they have no padding and can be read directly with the BinaryReader.
 */
namespace mt::idtech {

	const int MAX_QPATH = 64; ///< max length of a path or a name in the file formats

	constexpr int32_t ident(char a, char b, char c, char d)
	{
		return (int32_t(d) << 24) + (int32_t(c) << 16) + (int32_t(b) << 8) + int32_t(a);
	}

	// Angle indexes
	const int PITCH = 0; ///< up / down
	const int YAW = 1;	 ///< left / right
	const int ROLL = 2;	 ///< fall over

	/**
	 * Convert a 16 bit angle to degrees
	 * @param value short angle (0 - 65535 for a full circle)
	 * @return angle in degrees
	 */
	inline float shortToAngle(int value)
	{
		return float(value) * (360.0f / 65536.0f);
	}

	/**
	 * Convert degrees to a 16 bit angle
	 * @param angle angle in degrees
	 * @return short angle (0 - 65535 for a full circle)
	 */
	inline int16_t angleToShort(float angle)
	{
		return int16_t(int(angle * (65536.0f / 360.0f)) & 65535);
	}

	/**
	 * idTech3 AnglesToAxis: forward, left and up vectors from pitch, yaw and roll angles
	 * @param angles pitch, yaw and roll in degrees
	 * @return axis vectors, axis[0] forward, axis[1] left, axis[2] up
	 */
	glm::mat3 anglesToAxis(const glm::vec3 &angles);

	/**
	 * Unit direction vector from 16 bit pitch and yaw angles (ET LocalIngleVector)
	 * @param pitch pitch as a short angle
	 * @param yaw yaw as a short angle
	 * @return direction vector
	 */
	glm::vec3 ingleVector(int pitch, int yaw);

	/**
	 * Decode an MD3 style latitude / longitude encoded normal
	 * @param normal encoded normal, latitude in the high byte, longitude in the low byte
	 * @return unit normal vector
	 */
	glm::vec3 latLngToNormal(int16_t normal);

	/**
	 * Rotation of an ET skeletal bone matrix. ET applies bone matrices with the axis vectors as rows
	 * (out[i] = dot(in, axis[i])), so the rotation is the transpose of the axis.
	 * @param axis bone axis as returned by anglesToAxis
	 * @return rotation quaternion so that rotation * v gives the same result as the game
	 */
	glm::quat boneRotation(const glm::mat3 &axis);

	/*
	==============================================================================
	MD3 (morph model)
	==============================================================================
	*/

	const int32_t MD3_IDENT = ident('I', 'D', 'P', '3');
	const int32_t MD3_VERSION = 15;
	const float MD3_XYZ_SCALE = 1.0f / 64.0f;

	struct md3Frame_t {
		float bounds[2][3];
		float localOrigin[3];
		float radius;
		char name[16];
	};

	struct md3Tag_t {
		char name[MAX_QPATH];
		float origin[3];
		float axis[3][3];
	};

	struct md3Surface_t {
		int32_t ident;
		char name[MAX_QPATH];
		int32_t flags;
		int32_t numFrames;
		int32_t numShaders;
		int32_t numVerts;
		int32_t numTriangles;
		int32_t ofsTriangles;
		int32_t ofsShaders; ///< offsets are from the start of the surface
		int32_t ofsSt;
		int32_t ofsXyzNormals;
		int32_t ofsEnd;
	};

	struct md3Shader_t {
		char name[MAX_QPATH];
		int32_t shaderIndex;
	};

	struct md3Triangle_t {
		int32_t indexes[3];
	};

	struct md3St_t {
		float st[2];
	};

	struct md3XyzNormal_t {
		int16_t xyz[3];
		int16_t normal;
	};

	struct md3Header_t {
		int32_t ident;
		int32_t version;
		char name[MAX_QPATH];
		int32_t flags;
		int32_t numFrames;
		int32_t numTags;
		int32_t numSurfaces;
		int32_t numSkins;
		int32_t ofsFrames;
		int32_t ofsTags;
		int32_t ofsSurfaces;
		int32_t ofsEnd;
	};

	/*
	==============================================================================
	MDC (compressed MD3)
	==============================================================================
	*/

	const int32_t MDC_IDENT = ident('I', 'D', 'P', 'C');
	const int32_t MDC_VERSION = 2;
	const float MDC_DIST_SCALE = 0.05f;
	const float MDC_MAX_OFS = 127.0f;
	const float MDC_TAG_ANGLE_SCALE = 360.0f / 32700.0f;

	struct mdcTagName_t {
		char name[MAX_QPATH];
	};

	struct mdcTag_t {
		int16_t xyz[3];
		int16_t angles[3];
	};

	struct mdcSurface_t {
		int32_t ident;
		char name[MAX_QPATH];
		int32_t flags;
		int32_t numCompFrames;
		int32_t numBaseFrames;
		int32_t numShaders;
		int32_t numVerts;
		int32_t numTriangles;
		int32_t ofsTriangles;
		int32_t ofsShaders;
		int32_t ofsSt;
		int32_t ofsXyzNormals;
		int32_t ofsXyzCompressed;
		int32_t ofsFrameBaseFrames;
		int32_t ofsFrameCompFrames;
		int32_t ofsEnd;
	};

	struct mdcHeader_t {
		int32_t ident;
		int32_t version;
		char name[MAX_QPATH];
		int32_t flags;
		int32_t numFrames;
		int32_t numTags;
		int32_t numSurfaces;
		int32_t numSkins;
		int32_t ofsFrames;
		int32_t ofsTagNames;
		int32_t ofsTags;
		int32_t ofsSurfaces;
		int32_t ofsEnd;
	};

	/**
	 * Decode an MDC compressed vertex offset
	 * @param ofsVec compressed offset, 8 bits per axis
	 * @return offset to add to the base frame vertex
	 */
	inline glm::vec3 decodeMdcOffset(uint32_t ofsVec)
	{
		return {(float(ofsVec & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE, (float((ofsVec >> 8) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE,
				(float((ofsVec >> 16) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE};
	}

	/*
	==============================================================================
	Shared by the Wolfenstein skeletal formats (MDS, MDM, MDX)
	==============================================================================
	*/

	const int BONEFLAG_TAG = 1; ///< this bone is actually a tag

	struct skelWeight_t {
		int32_t boneIndex;
		float boneWeight;
		float offset[3];
	};

	struct skelTriangle_t {
		int32_t indexes[3];
	};

	struct skelBoneFrameCompressed_t {
		int16_t angles[4];	  ///< pitch, yaw, roll, unused
		int16_t ofsAngles[2]; ///< pitch / yaw direction from the parent bone
	};

	struct skelBoneInfo_t {
		char name[MAX_QPATH];
		int32_t parent;
		float torsoWeight;
		float parentDist;
		int32_t flags;
	};

	/*
	==============================================================================
	MDS (RtCW skeletal model, mesh and animation in one file)
	==============================================================================
	*/

	const int32_t MDS_IDENT = ident('M', 'D', 'S', 'W');
	const int32_t MDS_VERSION = 4;

	struct mdsVertexHeader_t { ///< followed by numWeights skelWeight_t
		float normal[3];
		float texCoords[2];
		int32_t numWeights;
		int32_t fixedParent;
		float fixedDist;
	};

	struct mdsSurface_t {
		int32_t ident;
		char name[MAX_QPATH];
		char shader[MAX_QPATH];
		int32_t shaderIndex;
		int32_t minLod;
		int32_t ofsHeader;
		int32_t numVerts;
		int32_t ofsVerts;
		int32_t numTriangles;
		int32_t ofsTriangles;
		int32_t ofsCollapseMap;
		int32_t numBoneReferences;
		int32_t ofsBoneReferences;
		int32_t ofsEnd;
	};

	struct mdsFrameHeader_t { ///< followed by numBones skelBoneFrameCompressed_t
		float bounds[2][3];
		float localOrigin[3];
		float radius;
		float parentOffset[3];
	};

	struct mdsTag_t {
		char name[MAX_QPATH];
		float torsoWeight;
		int32_t boneIndex;
	};

	struct mdsHeader_t {
		int32_t ident;
		int32_t version;
		char name[MAX_QPATH];
		float lodScale;
		float lodBias;
		int32_t numFrames;
		int32_t numBones;
		int32_t ofsFrames;
		int32_t ofsBones;
		int32_t torsoParent;
		int32_t numSurfaces;
		int32_t ofsSurfaces;
		int32_t numTags;
		int32_t ofsTags;
		int32_t ofsEnd;
	};

	/*
	==============================================================================
	MDM (ET skeletal mesh, the bones come from an MDX)
	==============================================================================
	*/

	const int32_t MDM_IDENT = ident('M', 'D', 'M', 'W');
	const int32_t MDM_VERSION = 3;

	struct mdmVertexHeader_t { ///< followed by numWeights skelWeight_t
		float normal[3];
		float texCoords[2];
		int32_t numWeights;
	};

	struct mdmSurface_t {
		int32_t ident;
		char name[MAX_QPATH];
		char shader[MAX_QPATH];
		int32_t shaderIndex;
		int32_t minLod;
		int32_t ofsHeader;
		int32_t numVerts;
		int32_t ofsVerts;
		int32_t numTriangles;
		int32_t ofsTriangles;
		int32_t ofsCollapseMap;
		int32_t numBoneReferences;
		int32_t ofsBoneReferences;
		int32_t ofsEnd;
	};

	struct mdmTag_t {
		char name[MAX_QPATH];
		float axis[3][3];
		int32_t boneIndex;
		float offset[3];
		int32_t numBoneReferences;
		int32_t ofsBoneReferences;
		int32_t ofsEnd;
	};

	struct mdmHeader_t {
		int32_t ident;
		int32_t version;
		char name[MAX_QPATH];
		float lodScale;
		float lodBias;
		int32_t numSurfaces;
		int32_t ofsSurfaces;
		int32_t numTags;
		int32_t ofsTags;
		int32_t ofsEnd;
	};

	/*
	==============================================================================
	MDX (ET skeletal animation)
	==============================================================================
	*/

	const int32_t MDX_IDENT = ident('M', 'D', 'X', 'W');
	const int32_t MDX_VERSION = 2;

	struct mdxFrame_t { ///< followed by numBones skelBoneFrameCompressed_t
		float bounds[2][3];
		float localOrigin[3];
		float radius;
		float parentOffset[3];
	};

	struct mdxHeader_t {
		int32_t ident;
		int32_t version;
		char name[MAX_QPATH];
		int32_t numFrames;
		int32_t numBones;
		int32_t ofsFrames;
		int32_t ofsBones;
		int32_t torsoParent;
		int32_t ofsEnd;
	};

	inline glm::vec3 toVec3(const float (&values)[3])
	{
		return {values[0], values[1], values[2]};
	}

	inline glm::mat3 toAxis(const float (&values)[3][3])
	{
		return {toVec3(values[0]), toVec3(values[1]), toVec3(values[2])};
	}
}
