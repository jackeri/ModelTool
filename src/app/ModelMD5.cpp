#include "ModelMD5.h"
#include "SkeletalModel.h"
#include "ScriptStream.h"

namespace mt::model {

	using namespace Skeletal;

	Model *loadMd5Model(const Ref<IO::MTFile> &file)
	{
		// MD5 model file is just a text file
		ScriptStream stream(file);

		auto *model = new SkeletalModel;

		while (stream.hasNext())
		{
			std::string token = stream.token();

			switch (hash(token.c_str()))
			{
				// Check that the version matches the one version that we actually support (this should always be the same)
				case hash("MD5Version"):
					if ((int) stream.parse() != MD5_VERSION)
					{
						delete model;
						return nullptr;
					}
					break;

				// This can be ignored
				case hash("commandline"):
					break;

				case hash("numFrames"):
					break;

				case hash("numJoints"):
					break;

				case hash("frameRate"):
					break;

				case hash("numAnimatedComponents"):
					break;

				case hash("numMeshes"):
					break;

				case hash("joints"):
					break;

				case hash("mesh"):
					break;

				case hash("hierarchy"):
					break;

				case hash("bounds"):
					break;

				case hash("baseframe"):
					break;

				case hash("frame"):
					break;

				default:
				{
					// FIXME: log invalid data
					delete model;
					return nullptr;
				}
			}
		}

		return model;
	}

	void loadMD5Anim(Model *baseModel, const Ref<IO::MTFile> &)
	{
		auto *model = static_cast<SkeletalModel*>(baseModel);
		// FIXME: take the anim parts from the above function
	}
}
