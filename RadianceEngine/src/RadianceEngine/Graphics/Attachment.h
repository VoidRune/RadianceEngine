#pragma once
#include "Handle.h"
#include "Common.h"
#include <array>

namespace Rdn
{
	class ColorAttachment
	{
	public:

		ImageViewHandle ImageView = {};
		ImageLayout ImageLayout = {};
		AttachmentLoadOp LoadOp = AttachmentLoadOp::Clear;
		AttachmentStoreOp StoreOp = AttachmentStoreOp::Store;
		std::array<float, 4> ClearColor = {};
	};

	class DepthAttachment
	{
	public:

		ImageViewHandle ImageView = {};
		ImageLayout ImageLayout = {};
		AttachmentLoadOp LoadOp = AttachmentLoadOp::Clear;
		AttachmentStoreOp StoreOp = AttachmentStoreOp::Store;
		float ClearDepth = {};
	};
}