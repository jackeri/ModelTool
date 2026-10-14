#pragma once

#include "library.h"
#include "model.h"

namespace mt::model {

	/*! MD3 limits per mesh, used as the default limits when meshes are merged */
	constexpr size_t MERGE_MAX_VERTICES = 4096;
	constexpr size_t MERGE_MAX_TRIANGLES = 8192;

	/**
	 * @param model morph or skeletal model
	 * @return number of meshes (the base level of detail)
	 */
	size_t meshCount(const Model &model);

	/**
	 * Remove a mesh. Morph models lose the mesh with the same name in every level of detail too.
	 * @param model model to change
	 * @param mesh index of the mesh
	 * @throws mt::mt_exception for an invalid index
	 */
	void removeMesh(Model &model, size_t mesh);

	/**
	 * Merge the meshes that use the same material (not empty, the case is ignored) into the first of them, as long as
	 * the merged mesh stays within the limits (like the old Pascal Q3 Model Tool's "Compact"). Every level of detail on
	 * its own. The merged meshes lose their collapse maps (calculate the LOD again). Meshes without a material are
	 * left alone, as a skin names them; bake the skin first to merge them by its shaders.
	 * @param model model to change
	 * @param maxVertices most vertices of a merged mesh
	 * @param maxTriangles most triangles of a merged mesh
	 * @return number of meshes that were merged into others (removed)
	 */
	size_t mergeMeshesByMaterial(Model &model, size_t maxVertices = MERGE_MAX_VERTICES, size_t maxTriangles = MERGE_MAX_TRIANGLES);

	/**
	 * Mirror the model on an axis: vertices, normals, tags, frame bounds, and for skeletal models the bones (bind pose and
	 * every frame), weights and hitboxes, so it animates mirrored. The triangles are turned around so the faces still
	 * point outward. Left and right bones keep their names (a mirrored player has its "L" bones on the right).
	 * @param model model to change
	 * @param axis 0 = X, 1 = Y, 2 = Z
	 */
	void mirrorModel(Model &model, int axis);

	/**
	 * Turn the normals around
	 * @param model model to change
	 * @param mesh index of the mesh (and its levels of detail), -1 for every mesh
	 */
	void flipNormals(Model &model, int mesh = -1);

	/**
	 * Reverse the winding of the triangles (turns the faces around for culling, the normals stay)
	 * @param model model to change
	 * @param mesh index of the mesh (and its levels of detail), -1 for every mesh
	 */
	void reverseWinding(Model &model, int mesh = -1);

	/**
	 * Flip the texture coordinates: u = 1 - u and / or v = 1 - v (the Pascal tool's "Swap UV-ST" is the v flip, for
	 * textures that are upside down)
	 * @param model model to change
	 * @param mesh index of the mesh (and its levels of detail), -1 for every mesh
	 * @param u flip u
	 * @param v flip v
	 */
	void flipTexCoords(Model &model, int mesh, bool u, bool v);
}
