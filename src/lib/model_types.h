#pragma once

#include "glm/fwd.hpp"
#include "types.h"
#include <vector>

#define MAX_QPATH 64

typedef f32 vec2_t[2];
typedef f32 vec3_t[3];
typedef f32 vec4_t[4];
typedef f32 mat3_t[9];
typedef f32 mat4_t[16];
typedef f32 quat4_t[4];

void vec2_to_glm(const vec2_t in, glm::vec2 &out);
void glm_to_vec2(const glm::vec2 &in, vec2_t out);

void vec3_to_glm(const vec3_t in, glm::vec3 &out);
void glm_to_vec3(const glm::vec3 &in, vec3_t out);

/*
========================================================================
.MD3 triangle model file format
========================================================================
*/

#define MD3_IDENT (('3' << 24) + ('P' << 16) + ('D' << 8) + 'I')
#define MD3_VERSION 15

// limits
#define MD3_MAX_LODS 4
#define MD3_MAX_TRIANGLES 8192 // per surface
#define MD3_MAX_VERTS 4096	   // per surface
#define MD3_MAX_SHADERS 256	   // per surface
#define MD3_MAX_FRAMES 1024	   // per model
#define MD3_MAX_SURFACES 32	   // per model
#define MD3_MAX_TAGS 16		   // per frame

// vertex scales
#define MD3_XYZ_SCALE (1.0f / 64)

typedef struct md3Frame_s {
	vec3_t bounds[2];
	vec3_t localOrigin;
	f32 radius;
	char name[16];
} md3Frame_t;

typedef struct md3Tag_s {
	char name[MAX_QPATH]; // tag name
	vec3_t origin;
	vec3_t axis[3];
} md3Tag_t;

typedef struct {
	i32 ident;

	char name[MAX_QPATH]; // polyset name

	i32 flags;
	i32 numFrames; // all surfaces in a model should have the same

	i32 numShaders; // all surfaces in a model should have the same
	i32 numVerts;

	i32 numTriangles;
	i32 ofsTriangles;

	i32 ofsShaders;	   // offset from start of md3Surface_t
	i32 ofsSt;		   // texture coords are common for all frames
	i32 ofsXyzNormals; // numVerts * numFrames

	i32 ofsEnd; // next surface follows
} md3Surface_t;

typedef struct {
	char name[MAX_QPATH];
	i32 shaderIndex; // for in-game use
} md3Shader_t;

typedef struct {
	i32 indexes[3];
} md3Triangle_t;

typedef struct {
	f32 st[2];
} md3St_t;

typedef struct {
	i16 xyz[3];
	i16 normal;
} md3XyzNormal_t;

typedef struct {
	i32 ident;
	i32 version;

	char name[MAX_QPATH]; // model name

	i32 flags;

	i32 numFrames;
	i32 numTags;
	i32 numSurfaces;

	i32 numSkins;

	i32 ofsFrames;	 // offset for first frame
	i32 ofsTags;	 // numFrames * numTags
	i32 ofsSurfaces; // first surface, others follow

	i32 ofsEnd; // end of file
} md3Header_t;

/*
==============================================================================
MDC file format (MDC files are basically compressed MD3 files which were invented in order to save disk space.)
==============================================================================
*/

#define MDC_DIST_SCALE 0.05f // lower for more accuracy, but less range

// note: we are locked in at 8 or less bits since changing to byte-encoded normals
#define MDC_BITS_PER_AXIS 8
#define MDC_MAX_OFS 127.0 // to be safe

#define MDC_MAX_DIST (MDC_MAX_OFS * MDC_DIST_SCALE)

// optimized version

static inline void mdc_encodexyzcompressed(const vec3_t xyz, i32 &ofsVec)
{
	ofsVec = 0;
	ofsVec |= (i32)(xyz[0] / MDC_DIST_SCALE + MDC_MAX_OFS) & 255;
	ofsVec |= ((i32)(xyz[1] / MDC_DIST_SCALE + MDC_MAX_OFS) & 255) << 8;
	ofsVec |= ((i32)(xyz[2] / MDC_DIST_SCALE + MDC_MAX_OFS) & 255) << 16;
}

#define R_MDC_DecodeXyzCompressed(ofsVec, out)                                                                                             \
	(out)[0] = float(((ofsVec) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;                                                                     \
	(out)[1] = float(((ofsVec >> 8) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;                                                                \
	(out)[2] = float(((ofsVec >> 16) & 255) - MDC_MAX_OFS) * MDC_DIST_SCALE;

#define MDC_IDENT (('C' << 24) + ('P' << 16) + ('D' << 8) + 'I')
#define MDC_VERSION 2

// version history:
// 1 - original
// 2 - changed tag structure so it only lists the names once

typedef struct {
	u32 ofsVec; // offset direction from the last base frame
} mdcXyzCompressed_t;

typedef struct {
	char name[MAX_QPATH]; // tag name
} mdcTagName_t;

#define MDC_TAG_ANGLE_SCALE (360.0 / 32700.0)

typedef struct {
	i16 xyz[3];
	i16 angles[3];
} mdcTag_t;

typedef struct {
	i32 ident;

	char name[MAX_QPATH]; // polyset name

	i32 flags;
	i32 numCompFrames; // all surfaces in a model should have the same
	i32 numBaseFrames; // ditto

	i32 numShaders; // all surfaces in a model should have the same
	i32 numVerts;

	i32 numTriangles;
	i32 ofsTriangles;

	i32 ofsShaders;		  // offset from start of md3Surface_t
	i32 ofsSt;			  // texture coords are common for all frames
	i32 ofsXyzNormals;	  // numVerts * numBaseFrames
	i32 ofsXyzCompressed; // numVerts * numCompFrames

	i32 ofsFrameBaseFrames; // numFrames
	i32 ofsFrameCompFrames; // numFrames

	i32 ofsEnd; // next surface follows
} mdcSurface_t;

typedef struct {
	i32 ident;
	i32 version;

	char name[MAX_QPATH]; // model name

	i32 flags;

	i32 numFrames;
	i32 numTags;
	i32 numSurfaces;

	i32 numSkins;

	i32 ofsFrames;	 // offset for first frame, stores the bounds and localOrigin
	i32 ofsTagNames; // numTags
	i32 ofsTags;	 // numFrames * numTags
	i32 ofsSurfaces; // first surface, others follow

	i32 ofsEnd; // end of file
} mdcHeader_t;

/*
==============================================================================
MDS file format (Wolfenstein Skeletal Format)
==============================================================================
*/

#define MDS_IDENT (('W' << 24) + ('S' << 16) + ('D' << 8) + 'M')
#define MDS_VERSION 4
#define MDS_MAX_VERTS 6000
#define MDS_MAX_TRIANGLES 8192
#define MDS_MAX_BONES 128
#define MDS_MAX_SURFACES 32
#define MDS_MAX_TAGS 128

#define MDS_TRANSLATION_SCALE (1.0 / 64)

typedef struct {
	i32 boneIndex;	// these are indexes into the boneReferences,
	f32 boneWeight; // not the global per-frame bone list
	vec3_t offset;
} mdsWeight_t;

typedef struct {
	vec3_t normal;
	vec2_t texCoords;
	i32 numWeights;
	i32 fixedParent; // stay equi-distant from this parent
	f32 fixedDist;
	mdsWeight_t weights[1]; // variable sized
} mdsVertex_t;

typedef struct {
	i32 indexes[3];
} mdsTriangle_t;

typedef struct {
	i32 ident;

	char name[MAX_QPATH]; // polyset name
	char shader[MAX_QPATH];
	i32 shaderIndex; // for in-game use

	i32 minLod;

	i32 ofsHeader; // this will be a negative number

	i32 numVerts;
	i32 ofsVerts;

	i32 numTriangles;
	i32 ofsTriangles;

	i32 ofsCollapseMap; // numVerts * int

	// Bone references are a set of i32s representing all the bones
	// present in any vertex weights for this surface.  This is
	// needed because a model may have surfaces that need to be
	// drawn at different sort times, and we don't want to have
	// to re-i32erpolate all the bones for each surface.
	i32 numBoneReferences;
	i32 ofsBoneReferences;

	i32 ofsEnd; // next surface follows
} mdsSurface_t;

typedef struct {
	i16 angles[4];	  // to be converted to axis at run-time (this is also better for lerping)
	i16 ofsAngles[2]; // PITCH/YAW, head in this direction from parent to go to the offset position
} mdsBoneFrameCompressed_t;

// NOTE: this only used at run-time
typedef struct {
	f32 matrix[3][3];	// 3x3 rotation
	vec3_t translation; // translation vector
} mdsBoneFrame_t;

typedef struct {
	vec3_t bounds[2];				   // bounds of all surfaces of all LOD's for this frame
	vec3_t localOrigin;				   // midpoint of bounds, used for sphere cull
	f32 radius;						   // dist from localOrigin to corner
	vec3_t parentOffset;			   // one bone is an ascendant of all other bones, it starts the hierachy at this position
	mdsBoneFrameCompressed_t bones[1]; // [numBones]
} mdsFrame_t;

typedef struct {
	i32 numSurfaces;
	i32 ofsSurfaces; // first surface, others follow
	i32 ofsEnd;		 // next lod follows
} mdsLOD_t;

typedef struct {
	char name[MAX_QPATH]; // name of tag
	f32 torsoWeight;
	i32 boneIndex; // our index in the bones
} mdsTag_t;

#define BONEFLAG_TAG 1 // this bone is actually a tag

typedef struct {
	char name[MAX_QPATH]; // name of bone
	i32 parent;			  // not sure if this is required, no harm throwing it in
	f32 torsoWeight;	  // scale torso rotation about torsoParent by this
	f32 parentDist;
	i32 flags;
} mdsBoneInfo_t;

typedef struct {
	i32 ident;
	i32 version;

	char name[MAX_QPATH]; // model name

	f32 lodScale;
	f32 lodBias;

	// frames and bones are shared by all levels of detail
	i32 numFrames;
	i32 numBones;
	i32 ofsFrames;	 // md4Frame_t[numFrames]
	i32 ofsBones;	 // mdsBoneInfo_t[numBones]
	i32 torsoParent; // index of bone that is the parent of the torso

	i32 numSurfaces;
	i32 ofsSurfaces;

	// tag data
	i32 numTags;
	i32 ofsTags; // mdsTag_t[numTags]

	i32 ofsEnd; // end of file
} mdsHeader_t;

/*
==============================================================================
MDM file format (Wolfenstein Skeletal Mesh)

version history:
2 - initial version
3 - removed all frame data, this format is pure mesh and bone references now
==============================================================================
*/

#define MDM_IDENT (('W' << 24) + ('M' << 16) + ('D' << 8) + 'M')
#define MDM_VERSION 3
#define MDM_MAX_VERTS 6000
#define MDM_MAX_TRIANGLES 8192
#define MDM_MAX_SURFACES 32
#define MDM_MAX_TAGS 128

#define MDM_TRANSLATION_SCALE (1.0 / 64)

typedef struct {
	i32 boneIndex;	// these are indexes into the boneReferences,
	f32 boneWeight; // not the global per-frame bone list
	vec3_t offset;
} mdmWeight_t;

typedef struct {
	vec3_t normal;
	vec2_t texCoords;
	i32 numWeights;
	mdmWeight_t weights[1]; // variable sized
} mdmVertex_t;

typedef struct {
	i32 indexes[3];
} mdmTriangle_t;

typedef struct {
	i32 ident;

	char name[MAX_QPATH]; // polyset name
	char shader[MAX_QPATH];
	i32 shaderIndex; // for in-game use

	i32 minLod;

	i32 ofsHeader; // this will be a negative number

	i32 numVerts;
	i32 ofsVerts;

	i32 numTriangles;
	i32 ofsTriangles;

	i32 ofsCollapseMap; // numVerts * int

	// Bone references are a set of i32s representing all the bones
	// present in any vertex weights for this surface.  This is
	// needed because a model may have surfaces that need to be
	// drawn at different sort times, and we don't want to have
	// to re-i32erpolate all the bones for each surface.
	i32 numBoneReferences;
	i32 ofsBoneReferences;

	i32 ofsEnd; // next surface follows
} mdmSurface_t;

typedef struct {
	i32 numSurfaces;
	i32 ofsSurfaces; // first surface, others follow
	i32 ofsEnd;		 // next lod follows
} mdmLOD_t;

// Tags always only have one parent bone
typedef struct {
	char name[MAX_QPATH]; // name of tag
	vec3_t axis[3];

	i32 boneIndex;
	vec3_t offset;

	i32 numBoneReferences;
	i32 ofsBoneReferences;

	i32 ofsEnd; // next tag follows
} mdmTag_t;

typedef struct {
	i32 ident;
	i32 version;

	char name[MAX_QPATH];

	f32 lodScale;
	f32 lodBias;

	i32 numSurfaces;
	i32 ofsSurfaces;

	i32 numTags;
	i32 ofsTags;

	i32 ofsEnd;
} mdmHeader_t;

/*
==============================================================================
MDX file format (Wolfenstein Skeletal Data)

version history:
1 - initial version
2 - moved parentOffset from the mesh to the skeletal data file
==============================================================================
*/

static inline f32 short_to_angle(const i16 &angle)
{
	return ((f32)angle * (360.0f / 65536));
}

static inline i16 angle_to_short(const f32 &angle)
{
	return (i16)(angle / (360.0f / 65536));
}

static inline void angles_short_to_float(const short *sh, float *pf)
{
	*(pf++) = short_to_angle(*(sh++));
	*(pf++) = short_to_angle(*(sh++));
	*(pf++) = short_to_angle(*(sh++));
}

#define MDX_IDENT (('W' << 24) + ('X' << 16) + ('D' << 8) + 'M')
#define MDX_VERSION 2
#define MDX_MAX_BONES 128

typedef struct {
	vec3_t bounds[2];	 // bounds of this frame
	vec3_t localOrigin;	 // midpoint of bounds, used for sphere cull
	f32 radius;			 // dist from localOrigin to corner
	vec3_t parentOffset; // one bone is an ascendant of all other bones, it starts the hierachy at this position
} mdxFrame_t;

typedef struct {
	i16 angles[4];	  // to be converted to axis at run-time (this is also better for lerping)
	i16 ofsAngles[2]; // PITCH/YAW, head in this direction from parent to go to the offset position
} mdxBoneFrameCompressed_t;

// NOTE: this only used at run-time
// FIXME: do we really need this?
typedef struct {
	f32 matrix[3][3];	// 3x3 rotation
	vec3_t translation; // translation vector
} mdxBoneFrame_t;

typedef struct {
	char name[MAX_QPATH]; // name of bone
	i32 parent;			  // not sure if this is required, no harm throwing it in
	f32 torsoWeight;	  // scale torso rotation about torsoParent by this
	f32 parentDist;
	i32 flags;
} mdxBoneInfo_t;

typedef struct {
	i32 ident;
	i32 version;

	char name[MAX_QPATH]; // model name

	// bones are shared by all levels of detail
	i32 numFrames;
	i32 numBones;
	i32 ofsFrames;	 // (mdxFrame_t + mdxBoneFrameCompressed_t[numBones]) * numframes
	i32 ofsBones;	 // mdxBoneInfo_t[numBones]
	i32 torsoParent; // index of bone that is the parent of the torso

	i32 ofsEnd; // end of file
} mdxHeader_t;

/*
==============================================================================
Interim Quake Model (THIS IS THE FUTURE!)
==============================================================================
*/

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

#define IQM_MAX_JOINTS 128

struct iqmHeader {
	char magic[16];
	u32 version;
	u32 filesize;
	u32 flags;
	u32 num_text, ofs_text;
	u32 num_meshes, ofs_meshes;
	u32 num_vertexarrays, num_vertexes, ofs_vertexarrays;
	u32 num_triangles, ofs_triangles, ofs_adjacency;
	u32 num_joints, ofs_joints;
	u32 num_poses, ofs_poses;
	u32 num_anims, ofs_anims;
	u32 num_frames, num_framechannels, ofs_frames, ofs_bounds;
	u32 num_comment, ofs_comment;
	u32 num_extensions, ofs_extensions;
};

struct iqmMesh {
	u32 name;
	u32 material;
	u32 first_vertex, num_vertexes;
	u32 first_triangle, num_triangles;
};

enum
{
	IQM_POSITION = 0,
	IQM_TEXCOORD = 1,
	IQM_NORMAL = 2,
	IQM_TANGENT = 3,
	IQM_BLENDINDEXES = 4,
	IQM_BLENDWEIGHTS = 5,
	IQM_COLOR = 6,
	IQM_CUSTOM = 0x10
};

enum
{
	IQM_BYTE = 0,
	IQM_UBYTE = 1,
	IQM_SHORT = 2,
	IQM_USHORT = 3,
	IQM_INT = 4,
	IQM_UINT = 5,
	IQM_HALF = 6,
	IQM_FLOAT = 7,
	IQM_DOUBLE = 8,
};

struct iqmTriangle {
	u32 vertex[3];
};

struct iqmJoint {
	u32 name;
	i32 parent;
	f32 translate[3], rotate[4], scale[3];
};

struct iqmPose {
	i32 parent;
	u32 mask;
	f32 channeloffset[10];
	f32 channelscale[10];
};

struct iqmAnim {
	u32 name;
	u32 first_frame, num_frames;
	f32 framerate;
	u32 flags;
};

enum
{
	IQM_LOOP = 1 << 0
};

struct iqmVertexArray {
	u32 type;
	u32 flags;
	u32 format;
	u32 size;
	u32 offset;
};

struct iqmBounds {
	f32 bbmin[3], bbmax[3];
	f32 xyradius, radius;
};

// inter-quake-model
struct iqmData {
	i32 num_vertexes;
	i32 num_triangles;
	i32 num_frames;
	i32 num_surfaces;
	i32 num_joints;
	i32 num_poses;
	struct srfIQModel *surfaces;

	f32 *positions;
	f32 *texcoords;
	f32 *normals;
	f32 *tangents;
	byte *blendIndexes;
	union
	{
		float *f;
		byte *b;
	} blendWeights;
	byte *colors;
	i32 *triangles;

	// depending upon the exporter, blend indices and weights might be int/float
	// as opposed to the recommended byte/byte, for example Noesis exports
	// int/float whereas the official IQM tool exports byte/byte
	byte blendWeightsType; // IQM_UBYTE or IQM_FLOAT

	i32 *jointParents;
	f32 *jointMats;
	f32 *poseMats;
	f32 *bounds;
	char *names;
};

/*
==============================================================================
MilkShape3D (for Thunder)
==============================================================================
*/

struct ms3dCount {
	i32 count;
};

struct ms3dVertex {
	byte flags;
	vec3_t vertex;
	char boneId;
	byte referenceCount;

	// Extra
	char boneIds[3];
	byte weights[3];
	u32 extra;
	f32 renderColor[3];
};

struct ms3dTriangle {
	u16 flags;
	u16 vertexIndices[3];
	mat3_t vertexNormals;
	vec3_t s;
	vec3_t t;
	byte smoothingGroup;
	byte groupIndex;
};

struct ms3dMesh {
	byte flags;
	char name[32];
	u16 numTriangles;
	std::vector<u16> meshTriangles;
	char materialIndex;
};

struct ms3dMaterial {
	char name[32];
	vec4_t ambient;
	vec4_t diffuse;
	vec4_t specular;
	vec4_t emissive;
	f32 shininess;
	f32 transparency;
	byte mode;
	char texture[128];
	char alphamap[128];
};

struct ms3dKeyframe {
	f32 time;
	i32 timeInt;
	f32 key[3];
};

struct ms3dJoint {
	byte flags;
	char name[32];
	char parentName[32];
	vec3_t rot;
	vec3_t pos;

	u16 numKeyFramesRot;
	std::vector<ms3dKeyframe> rotationKeys;
	u16 numKeyFramesPos;
	std::vector<ms3dKeyframe> positionKeys;

	// Used in processing
	i32 parentIndex;
	quat4_t localRotation;
	vec3_t localPosition;

	quat4_t globalRotation;
	vec3_t globalPosition;
};

struct ms3dHeader {
	char id[10];
	i32 version;

	u16 num_verts;
	std::vector<ms3dVertex> vertexes;
	u16 num_triangles;
	std::vector<ms3dTriangle> triangles;
	u16 num_meshes;
	std::vector<ms3dMesh> meshes;
	u16 num_materials;
	std::vector<ms3dMaterial> materials;
	u16 num_joints;
	std::vector<ms3dJoint> joints;

	f32 animationFps;
	f32 currentTime;
	i32 totalFrames;
};
