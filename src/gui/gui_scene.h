#pragma once


#include "engine/iplugin.h"

namespace bgfx { struct TextureHandle; }


namespace Malmy
{


class GUISystem;
class Path;
class Pipeline;
class string;
struct Vec2;
struct Vec4;
template <typename T> class DelegateList;


class GUIScene : public IScene
{
public:
	enum class TextHAlign : int
	{
		LEFT,
		CENTER,
		RIGHT
	};

	struct Rect
	{
		float x, y, w, h;
	};

	static GUIScene* createInstance(GUISystem& system,
		Project& project,
		struct IAllocator& allocator);
	static void destroyInstance(GUIScene* scene);

	virtual void render(Pipeline& pipeline, const Vec2& canvas_size) = 0;

	virtual bool hasGUI(GameObject gameobject) const = 0;
	virtual Rect getRectOnCanvas(GameObject gameobject, const Vec2& canva_size) const = 0;
	virtual Rect getRect(GameObject gameobject) const = 0;
	virtual GameObject getRectAt(const Vec2& pos, const Vec2& canvas_size) const = 0;

	virtual void enableRect(GameObject gameobject, bool enable) = 0;
	virtual bool isRectEnabled(GameObject gameobject) = 0;
	virtual bool getRectClip(GameObject gameobject) = 0;
	virtual void setRectClip(GameObject gameobject, bool value) = 0;
	virtual float getRectLeftPoints(GameObject gameobject) = 0;
	virtual void setRectLeftPoints(GameObject gameobject, float value) = 0;
	virtual float getRectLeftRelative(GameObject gameobject) = 0;
	virtual void setRectLeftRelative(GameObject gameobject, float value) = 0;

	virtual float getRectRightPoints(GameObject gameobject) = 0;
	virtual void setRectRightPoints(GameObject gameobject, float value) = 0;
	virtual float getRectRightRelative(GameObject gameobject) = 0;
	virtual void setRectRightRelative(GameObject gameobject, float value) = 0;

	virtual float getRectTopPoints(GameObject gameobject) = 0;
	virtual void setRectTopPoints(GameObject gameobject, float value) = 0;
	virtual float getRectTopRelative(GameObject gameobject) = 0;
	virtual void setRectTopRelative(GameObject gameobject, float value) = 0;

	virtual float getRectBottomPoints(GameObject gameobject) = 0;
	virtual void setRectBottomPoints(GameObject gameobject, float value) = 0;
	virtual float getRectBottomRelative(GameObject gameobject) = 0;
	virtual void setRectBottomRelative(GameObject gameobject, float value) = 0;

	virtual Vec4 getButtonNormalColorRGBA(GameObject gameobject) = 0;
	virtual void setButtonNormalColorRGBA(GameObject gameobject, const Vec4& color) = 0;
	virtual Vec4 getButtonHoveredColorRGBA(GameObject gameobject) = 0;
	virtual void setButtonHoveredColorRGBA(GameObject gameobject, const Vec4& color) = 0;

	virtual void enableImage(GameObject gameobject, bool enable) = 0;
	virtual bool isImageEnabled(GameObject gameobject) = 0;
	virtual Vec4 getImageColorRGBA(GameObject gameobject) = 0;
	virtual void setImageColorRGBA(GameObject gameobject, const Vec4& color) = 0;
	virtual Path getImageSprite(GameObject gameobject) = 0;
	virtual void setImageSprite(GameObject gameobject, const Path& path) = 0;

	virtual void setText(GameObject gameobject, const char* text) = 0;
	virtual const char* getText(GameObject gameobject) = 0;
	virtual TextHAlign getTextHAlign(GameObject gameobject) = 0;
	virtual void setTextHAlign(GameObject gameobject, TextHAlign align) = 0;
	virtual void setTextFontSize(GameObject gameobject, int value) = 0;
	virtual int getTextFontSize(GameObject gameobject) = 0;
	virtual Vec4 getTextColorRGBA(GameObject gameobject) = 0;
	virtual void setTextColorRGBA(GameObject gameobject, const Vec4& color) = 0;
	virtual Path getTextFontPath(GameObject gameobject) = 0;
	virtual void setTextFontPath(GameObject gameobject, const Path& path) = 0;

	virtual void setRenderTarget(GameObject gameobject, bgfx::TextureHandle* texture_handle) = 0;

	virtual DelegateList<void(GameObject)>& buttonClicked() = 0;
	virtual DelegateList<void(GameObject)>& rectHovered() = 0;
	virtual DelegateList<void(GameObject)>& rectHoveredOut() = 0;
};


} // namespace Malmy
