#include "gui_scene.h"
#include "gui_system.h"
#include "sprite_manager.h"
#include "engine/engine.h"
#include "engine/flag_set.h"
#include "engine/iallocator.h"
#include "engine/input_system.h"
#include "engine/log.h"
#include "engine/plugin_manager.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/resource_manager_base.h"
#include "engine/serializer.h"
#include "engine/project/project.h"
#include "renderer/draw2d.h"
#include "renderer/font_manager.h"
#include "renderer/pipeline.h"
#include "renderer/renderer.h"
#include "renderer/render_scene.h"
#include "renderer/texture.h"
#include <SDL.h>

namespace Malmy
{


static const ComponentType GUI_BUTTON_TYPE = Reflection::getComponentType("gui_button");
static const ComponentType GUI_RECT_TYPE = Reflection::getComponentType("gui_rect");
static const ComponentType GUI_RENDER_TARGET_TYPE = Reflection::getComponentType("gui_render_target");
static const ComponentType GUI_IMAGE_TYPE = Reflection::getComponentType("gui_image");
static const ComponentType GUI_TEXT_TYPE = Reflection::getComponentType("gui_text");
static const ComponentType GUI_INPUT_FIELD_TYPE = Reflection::getComponentType("gui_input_field");
static const float CURSOR_BLINK_PERIOD = 1.0f;
static bgfx::TextureHandle EMPTY_RENDER_TARGET = BGFX_INVALID_HANDLE;

struct GUIText
{
	GUIText(IAllocator& allocator) : text("", allocator) {}
	~GUIText() { setFontResource(nullptr); }


	void setFontResource(FontResource* res)
	{
		if (m_font_resource)
		{
			if (m_font)
			{
				m_font_resource->removeRef(*m_font);
				m_font = nullptr;
			}
			m_font_resource->getObserverCb().unbind<GUIText, &GUIText::onFontLoaded>(this);
			m_font_resource->getResourceManager().unload(*m_font_resource);
		}
		m_font_resource = res;
		if (res) res->onLoaded<GUIText, &GUIText::onFontLoaded>(this);
	}


	void onFontLoaded(Resource::State old_state, Resource::State new_state, Resource&)
	{
		if (m_font && new_state != Resource::State::READY)
		{
			m_font_resource->removeRef(*m_font);
			m_font = nullptr;
		}
		if (new_state == Resource::State::READY) m_font = m_font_resource->addRef(m_font_size);
	}

	void setFontSize(int value)
	{
		m_font_size = value;
		if (m_font_resource && m_font_resource->isReady())
		{
			if(m_font) m_font_resource->removeRef(*m_font);
			m_font = m_font_resource->addRef(m_font_size);
		}
	}


	FontResource* getFontResource() const { return m_font_resource; }
	int getFontSize() const { return m_font_size; }
	Font* getFont() const { return m_font; }


	string text;
	GUIScene::TextHAlign horizontal_align = GUIScene::TextHAlign::LEFT;
	u32 color = 0xff000000;

private:
	int m_font_size = 13;
	Font* m_font = nullptr;
	FontResource* m_font_resource = nullptr;
};


struct GUIButton
{
	u32 normal_color = 0xffFFffFF;
	u32 hovered_color = 0xffFFffFF;
};


struct GUIInputField
{
	int cursor = 0;
	float anim = 0;
};


struct GUIImage
{
	enum Flags
	{
		IS_ENABLED = 1 << 1
	};
	Sprite* sprite = nullptr;
	u32 color = 0xffffFFFF;
	FlagSet<Flags, u32> flags;
};


struct GUIRect
{
	enum Flags
	{
		IS_VALID = 1 << 0,
		IS_ENABLED = 1 << 1,
		IS_CLIP = 1 << 2
	};

	struct Anchor
	{
		float points = 0;
		float relative = 0;
	};

	GameObject gameobject;
	FlagSet<Flags, u32> flags;
	Anchor top;
	Anchor right = { 0, 1 };
	Anchor bottom = { 0, 1 };
	Anchor left;

	GUIImage* image = nullptr;
	GUIText* text = nullptr;
	GUIInputField* input_field = nullptr;
	bgfx::TextureHandle* render_target = nullptr;
};


struct GUISceneImpl MALMY_FINAL : public GUIScene
{
	GUISceneImpl(GUISystem& system, Project& context, IAllocator& allocator)
		: m_allocator(allocator)
		, m_project(context)
		, m_system(system)
		, m_rects(allocator)
		, m_buttons(allocator)
		, m_rect_hovered(allocator)
		, m_rect_hovered_out(allocator)
		, m_button_clicked(allocator)
		, m_buttons_down_count(0)
		, m_canvas_size(800, 600)
	{
		context.registerComponentType(GUI_RECT_TYPE
			, this
			, &GUISceneImpl::createRect
			, &GUISceneImpl::destroyRect
			, &GUISceneImpl::serializeRect
			, &GUISceneImpl::deserializeRect);
		context.registerComponentType(GUI_IMAGE_TYPE
			, this
			, &GUISceneImpl::createImage
			, &GUISceneImpl::destroyImage
			, &GUISceneImpl::serializeImage
			, &GUISceneImpl::deserializeImage);
		context.registerComponentType(GUI_RENDER_TARGET_TYPE
			, this
			, &GUISceneImpl::createRenderTarget
			, &GUISceneImpl::destroyRenderTarget
			, &GUISceneImpl::serializeRenderTarget
			, &GUISceneImpl::deserializeRenderTarget);
		context.registerComponentType(GUI_INPUT_FIELD_TYPE
			, this
			, &GUISceneImpl::createInputField
			, &GUISceneImpl::destroyInputField
			, &GUISceneImpl::serializeInputField
			, &GUISceneImpl::deserializeInputField);
		context.registerComponentType(GUI_TEXT_TYPE
			, this
			, &GUISceneImpl::createText
			, &GUISceneImpl::destroyText
			, &GUISceneImpl::serializeText
			, &GUISceneImpl::deserializeText);
		context.registerComponentType(GUI_BUTTON_TYPE
			, this
			, &GUISceneImpl::createButton
			, &GUISceneImpl::destroyButton
			, &GUISceneImpl::serializeButton
			, &GUISceneImpl::deserializeButton);
		m_font_manager = (FontManager*)system.getEngine().getResourceManager().get(FontResource::TYPE);
	}

	void renderTextCursor(GUIRect& rect, Draw2D& draw, const Vec2& pos)
	{
		if (!rect.input_field) return;
		if (m_focused_gameobject != rect.gameobject) return;
		if (rect.input_field->anim > CURSOR_BLINK_PERIOD * 0.5f) return;

		const char* text = rect.text->text.c_str();
		const char* text_end = text + rect.input_field->cursor;
		Font* font = rect.text->getFont();
		float font_size = (float)rect.text->getFontSize();
		Vec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0, text, text_end);
		draw.AddLine({ pos.x + text_size.x, pos.y }, { pos.x + text_size.x, pos.y + text_size.y }, rect.text->color, 1);
	}


	void renderRect(GUIRect& rect, Pipeline& pipeline, const Rect& parent_rect)
	{
		if (!rect.flags.isSet(GUIRect::IS_VALID)) return;
		if (!rect.flags.isSet(GUIRect::IS_ENABLED)) return;

		float l = parent_rect.x + rect.left.points + parent_rect.w * rect.left.relative;
		float r = parent_rect.x + rect.right.points + parent_rect.w * rect.right.relative;
		float t = parent_rect.y + rect.top.points + parent_rect.h * rect.top.relative;
		float b = parent_rect.y + rect.bottom.points + parent_rect.h * rect.bottom.relative;
			 
		Draw2D& draw = pipeline.getDraw2D();
		if (rect.flags.isSet(GUIRect::IS_CLIP)) draw.PushClipRect({ l, t }, { r, b });

		if (rect.image && rect.image->flags.isSet(GUIImage::IS_ENABLED))
		{
			if (rect.image->sprite && rect.image->sprite->getTexture())
			{
				Sprite* sprite = rect.image->sprite;
				Texture* tex = sprite->getTexture();
				if (sprite->type == Sprite::PATCH9)
				{
					struct Quad {
						float l, t, r, b;
					} pos = {
						l + sprite->left,
						t + sprite->top,
						r - tex->width + sprite->right,
						b - tex->height + sprite->bottom
					};
					Quad uvs = {
						sprite->left / (float)tex->width,
						sprite->top / (float)tex->height,
						sprite->right / (float)tex->width,
						sprite->bottom / (float)tex->height
					};

					draw.AddImage(&tex->handle, { l, t }, { pos.l, pos.t }, { 0, 0 }, { uvs.l, uvs.t });
					draw.AddImage(&tex->handle, { pos.l, t }, { pos.r, pos.t }, { uvs.l, 0 }, { uvs.r, uvs.t });
					draw.AddImage(&tex->handle, { pos.r, t }, { r, pos.t }, { uvs.r, 0 }, { 1, uvs.t });

					draw.AddImage(&tex->handle, { l, pos.t }, { pos.l, pos.b }, { 0, uvs.t }, { uvs.l, uvs.b });
					draw.AddImage(&tex->handle, { pos.l, pos.t }, { pos.r, pos.b }, { uvs.l, uvs.t }, { uvs.r, uvs.b });
					draw.AddImage(&tex->handle, { pos.r, pos.t }, { r, pos.b }, { uvs.r, uvs.t }, { 1, uvs.b });

					draw.AddImage(&tex->handle, { l, pos.b }, { pos.l, b }, { 0, uvs.b }, { uvs.l, 1 });
					draw.AddImage(&tex->handle, { pos.l, pos.b }, { pos.r, b }, { uvs.l, uvs.b }, { uvs.r, 1 });
					draw.AddImage(&tex->handle, { pos.r, pos.b }, { r, b }, { uvs.r, uvs.b }, { 1, 1 });

				}
				else
				{
					draw.AddImage(&tex->handle, { l, t }, { r, b });
				}
			}
			else
			{
				draw.AddRectFilled({ l, t }, { r, b }, rect.image->color);
			}
		}

		if (rect.render_target && bgfx::isValid(*rect.render_target))
		{
			draw.AddImage(rect.render_target, { l, t }, { r, b });
		}

		if (rect.text)
		{
			Font* font = rect.text->getFont();
			if (!font) font = m_font_manager->getDefaultFont();

			const char* text_cstr = rect.text->text.c_str();
			float font_size = (float)rect.text->getFontSize();
			Vec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0, text_cstr);
			Vec2 text_pos(l, t);

			switch (rect.text->horizontal_align)
			{
				case TextHAlign::LEFT: break;
				case TextHAlign::RIGHT: text_pos.x = r - text_size.x; break;
				case TextHAlign::CENTER: text_pos.x = (r + l - text_size.x) * 0.5f; break; 
			}

			draw.AddText(font, font_size, text_pos, rect.text->color, text_cstr);
			renderTextCursor(rect, draw, text_pos);
		}

		GameObject child = m_project.getFirstChild(rect.gameobject);
		while (child.isValid())
		{
			int idx = m_rects.find(child);
			if (idx >= 0)
			{
				renderRect(*m_rects.at(idx), pipeline, { l, t, r - l, b - t });
			}
			child = m_project.getNextSibling(child);
		}
		if (rect.flags.isSet(GUIRect::IS_CLIP)) draw.PopClipRect();
	}


	void render(Pipeline& pipeline, const Vec2& canvas_size) override
	{
		if (!m_root) return;

		m_canvas_size = canvas_size;
		renderRect(*m_root, pipeline, {0, 0, canvas_size.x, canvas_size.y});
	}


	Vec4 getButtonNormalColorRGBA(GameObject gameobject) override
	{
		return ABGRu32ToRGBAVec4(m_buttons[gameobject].normal_color);
	}


	void setButtonNormalColorRGBA(GameObject gameobject, const Vec4& color) override
	{
		m_buttons[gameobject].normal_color = RGBAVec4ToABGRu32(color);
	}


	Vec4 getButtonHoveredColorRGBA(GameObject gameobject) override
	{
		return ABGRu32ToRGBAVec4(m_buttons[gameobject].hovered_color);
	}


	void setButtonHoveredColorRGBA(GameObject gameobject, const Vec4& color) override
	{
		m_buttons[gameobject].hovered_color = RGBAVec4ToABGRu32(color);
	}


	void enableImage(GameObject gameobject, bool enable) override { m_rects[gameobject]->image->flags.set(GUIImage::IS_ENABLED, enable); }
	bool isImageEnabled(GameObject gameobject) override { return m_rects[gameobject]->image->flags.isSet(GUIImage::IS_ENABLED); }


	Vec4 getImageColorRGBA(GameObject gameobject) override
	{
		GUIImage* image = m_rects[gameobject]->image;
		return ABGRu32ToRGBAVec4(image->color);
	}


	static Vec4 ABGRu32ToRGBAVec4(u32 value)
	{
		float inv = 1 / 255.0f;
		return {
			((value >> 0) & 0xFF) * inv,
			((value >> 8) & 0xFF) * inv,
			((value >> 16) & 0xFF) * inv,
			((value >> 24) & 0xFF) * inv,
		};
	}


	static u32 RGBAVec4ToABGRu32(const Vec4& value)
	{
		u8 r = u8(value.x * 255 + 0.5f);
		u8 g = u8(value.y * 255 + 0.5f);
		u8 b = u8(value.z * 255 + 0.5f);
		u8 a = u8(value.w * 255 + 0.5f);
		return (a << 24) + (b << 16) + (g << 8) + r;
	}


	Path getImageSprite(GameObject gameobject) override
	{
		GUIImage* image = m_rects[gameobject]->image;
		return image->sprite ? image->sprite->getPath() : Path();
	}


	void setImageSprite(GameObject gameobject, const Path& path) override
	{
		GUIImage* image = m_rects[gameobject]->image;
		if (image->sprite)
		{
			image->sprite->getResourceManager().unload(*image->sprite);
		}
		auto* manager = m_system.getEngine().getResourceManager().get(Sprite::TYPE);
		if (path.isValid())
		{
			image->sprite = (Sprite*)manager->load(path);
		}
		else
		{
			image->sprite = nullptr;
		}
	}


	void setImageColorRGBA(GameObject gameobject, const Vec4& color) override
	{
		GUIImage* image = m_rects[gameobject]->image;
		image->color = RGBAVec4ToABGRu32(color);
	}


	bool hasGUI(GameObject gameobject) const override
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0) return false;
		return m_rects.at(idx)->flags.isSet(GUIRect::IS_VALID);
	}


	GameObject getRectAt(GUIRect& rect, const Vec2& pos, const Rect& parent_rect) const
	{
		if (!rect.flags.isSet(GUIRect::IS_VALID)) return INVALID_GAMEOBJECT;

		Rect r;
		r.x = parent_rect.x + rect.left.points + parent_rect.w * rect.left.relative;
		r.y = parent_rect.y + rect.top.points + parent_rect.h * rect.top.relative;
		float right = parent_rect.x + rect.right.points + parent_rect.w * rect.right.relative;
		float bottom = parent_rect.y + rect.bottom.points + parent_rect.h * rect.bottom.relative;

		r.w = right - r.x;
		r.h = bottom - r.y;

		bool intersect = pos.x >= r.x && pos.y >= r.y && pos.x <= r.x + r.w && pos.y <= r.y + r.h;

		for (GameObject child = m_project.getFirstChild(rect.gameobject); child.isValid(); child = m_project.getNextSibling(child))
		{
			int idx = m_rects.find(child);
			if (idx < 0) continue;

			GUIRect* child_rect = m_rects.at(idx);
			GameObject gameobject = getRectAt(*child_rect, pos, r);
			if (gameobject.isValid()) return gameobject;
		}

		return intersect ? rect.gameobject : INVALID_GAMEOBJECT;
	}


	GameObject getRectAt(const Vec2& pos, const Vec2& canvas_size) const override
	{
		if (!m_root) return INVALID_GAMEOBJECT;

		return getRectAt(*m_root, pos, { 0, 0, canvas_size.x, canvas_size.y });
	}


	static Rect getRectOnCanvas(const Rect& parent_rect, GUIRect& rect)
	{
		float l = parent_rect.x + parent_rect.w * rect.left.relative + rect.left.points;
		float r = parent_rect.x + parent_rect.w * rect.right.relative + rect.right.points;
		float t = parent_rect.y + parent_rect.h * rect.top.relative + rect.top.points;
		float b = parent_rect.y + parent_rect.h * rect.bottom.relative + rect.bottom.points;

		return { l, t, r - l, b - t };
	}


	Rect getRect(GameObject gameobject) const override
	{
		return getRectOnCanvas(gameobject, m_canvas_size);
	}


	Rect getRectOnCanvas(GameObject gameobject, const Vec2& canvas_size) const override
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0) return { 0, 0, canvas_size.x, canvas_size.y };
		GameObject parent = m_project.getParent(gameobject);
		Rect parent_rect = getRectOnCanvas(parent, canvas_size);
		GUIRect* gui = m_rects[gameobject];
		float l = parent_rect.x + parent_rect.w * gui->left.relative + gui->left.points;
		float r = parent_rect.x + parent_rect.w * gui->right.relative + gui->right.points;
		float t = parent_rect.y + parent_rect.h * gui->top.relative + gui->top.points;
		float b = parent_rect.y + parent_rect.h * gui->bottom.relative + gui->bottom.points;

		return { l, t, r - l, b - t };
	}

	void setRectClip(GameObject gameobject, bool enable) override { m_rects[gameobject]->flags.set(GUIRect::IS_CLIP, enable); }
	bool getRectClip(GameObject gameobject) override { return m_rects[gameobject]->flags.isSet(GUIRect::IS_CLIP); }
	void enableRect(GameObject gameobject, bool enable) override { m_rects[gameobject]->flags.set(GUIRect::IS_ENABLED, enable); }
	bool isRectEnabled(GameObject gameobject) override { return m_rects[gameobject]->flags.isSet(GUIRect::IS_ENABLED); }
	float getRectLeftPoints(GameObject gameobject) override { return m_rects[gameobject]->left.points; }
	void setRectLeftPoints(GameObject gameobject, float value) override { m_rects[gameobject]->left.points = value; }
	float getRectLeftRelative(GameObject gameobject) override { return m_rects[gameobject]->left.relative; }
	void setRectLeftRelative(GameObject gameobject, float value) override { m_rects[gameobject]->left.relative = value; }

	float getRectRightPoints(GameObject gameobject) override { return m_rects[gameobject]->right.points; }
	void setRectRightPoints(GameObject gameobject, float value) override { m_rects[gameobject]->right.points = value; }
	float getRectRightRelative(GameObject gameobject) override { return m_rects[gameobject]->right.relative; }
	void setRectRightRelative(GameObject gameobject, float value) override { m_rects[gameobject]->right.relative = value; }

	float getRectTopPoints(GameObject gameobject) override { return m_rects[gameobject]->top.points; }
	void setRectTopPoints(GameObject gameobject, float value) override { m_rects[gameobject]->top.points = value; }
	float getRectTopRelative(GameObject gameobject) override { return m_rects[gameobject]->top.relative; }
	void setRectTopRelative(GameObject gameobject, float value) override { m_rects[gameobject]->top.relative = value; }

	float getRectBottomPoints(GameObject gameobject) override { return m_rects[gameobject]->bottom.points; }
	void setRectBottomPoints(GameObject gameobject, float value) override { m_rects[gameobject]->bottom.points = value; }
	float getRectBottomRelative(GameObject gameobject) override { return m_rects[gameobject]->bottom.relative; }
	void setRectBottomRelative(GameObject gameobject, float value) override { m_rects[gameobject]->bottom.relative = value; }


	void setTextFontSize(GameObject gameobject, int value) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		gui_text->setFontSize(value);
	}
	
	
	int getTextFontSize(GameObject gameobject) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		return gui_text->getFontSize();
	}
	
	
	Vec4 getTextColorRGBA(GameObject gameobject) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		return ABGRu32ToRGBAVec4(gui_text->color);
	}


	void setTextColorRGBA(GameObject gameobject, const Vec4& color) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		gui_text->color = RGBAVec4ToABGRu32(color);
	}


	Path getTextFontPath(GameObject gameobject) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		return gui_text->getFontResource() == nullptr ? Path() : gui_text->getFontResource()->getPath();
	}


	void setTextFontPath(GameObject gameobject, const Path& path) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		FontResource* res = path.isValid() ? (FontResource*)m_font_manager->load(path) : nullptr;
		gui_text->setFontResource(res);
	}


	TextHAlign getTextHAlign(GameObject gameobject) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		return gui_text->horizontal_align;
		
	}


	void setTextHAlign(GameObject gameobject, TextHAlign value) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		gui_text->horizontal_align = value;
	}


	void setText(GameObject gameobject, const char* value) override
	{
		GUIText* gui_text = m_rects[gameobject]->text;
		gui_text->text = value;
	}


	const char* getText(GameObject gameobject) override
	{
		GUIText* text = m_rects[gameobject]->text;
		return text->text.c_str();
	}


	void serializeRect(ISerializer& serializer, GameObject gameobject)
	{
		const GUIRect& rect = *m_rects[gameobject];
		
		serializer.write("flags", rect.flags.base);
		serializer.write("top_pts", rect.top.points);
		serializer.write("top_rel", rect.top.relative);

		serializer.write("right_pts", rect.right.points);
		serializer.write("right_rel", rect.right.relative);

		serializer.write("bottom_pts", rect.bottom.points);
		serializer.write("bottom_rel", rect.bottom.relative);

		serializer.write("left_pts", rect.left.points);
		serializer.write("left_rel", rect.left.relative);
	}


	void deserializeRect(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int idx = m_rects.find(gameobject);
		GUIRect* rect;
		if (idx >= 0)
		{
			rect = m_rects.at(idx);
		}
		else
		{
			rect = MALMY_NEW(m_allocator, GUIRect);
			m_rects.insert(gameobject, rect);
		}
		rect->gameobject = gameobject;
		serializer.read(&rect->flags.base);
		serializer.read(&rect->top.points);
		serializer.read(&rect->top.relative);

		serializer.read(&rect->right.points);
		serializer.read(&rect->right.relative);

		serializer.read(&rect->bottom.points);
		serializer.read(&rect->bottom.relative);

		serializer.read(&rect->left.points);
		serializer.read(&rect->left.relative);
		
		m_root = findRoot();
		
		m_project.onComponentCreated(gameobject, GUI_RECT_TYPE, this);
	}


	void serializeRenderTarget(ISerializer& serializer, GameObject gameobject)
	{
	}


	void deserializeRenderTarget(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			GUIRect* rect = MALMY_NEW(m_allocator, GUIRect);
			rect->gameobject = gameobject;
			idx = m_rects.insert(gameobject, rect);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.render_target = &EMPTY_RENDER_TARGET;
		m_project.onComponentCreated(gameobject, GUI_RENDER_TARGET_TYPE, this);
	}


	void serializeButton(ISerializer& serializer, GameObject gameobject)
	{
		const GUIButton& button = m_buttons[gameobject];
		serializer.write("normal_color", button.normal_color);
		serializer.write("hovered_color", button.hovered_color);
	}

	
	void deserializeButton(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		GUIButton& button = m_buttons.emplace(gameobject);
		serializer.read(&button.normal_color);
		serializer.read(&button.hovered_color);
		m_project.onComponentCreated(gameobject, GUI_BUTTON_TYPE, this);
	}


	void serializeInputField(ISerializer& serializer, GameObject gameobject)
	{
	}


	void deserializeInputField(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			GUIRect* rect = MALMY_NEW(m_allocator, GUIRect);
			rect->gameobject = gameobject;
			idx = m_rects.insert(gameobject, rect);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.input_field = MALMY_NEW(m_allocator, GUIInputField);

		m_project.onComponentCreated(gameobject, GUI_INPUT_FIELD_TYPE, this);
	}


	void serializeImage(ISerializer& serializer, GameObject gameobject)
	{
		const GUIRect& rect = *m_rects[gameobject];
		serializer.write("sprite", rect.image->sprite ? rect.image->sprite->getPath().c_str() : "");
		serializer.write("color", rect.image->color);
		serializer.write("flags", rect.image->flags.base);
	}


	void deserializeImage(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			GUIRect* rect = MALMY_NEW(m_allocator, GUIRect);
			rect->gameobject = gameobject;
			idx = m_rects.insert(gameobject, rect);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.image = MALMY_NEW(m_allocator, GUIImage);
		
		char tmp[MAX_PATH_LENGTH];
		serializer.read(tmp, lengthOf(tmp));
		if (tmp[0] == '\0')
		{
			rect.image->sprite = nullptr;
		}
		else
		{
			auto* manager = m_system.getEngine().getResourceManager().get(Sprite::TYPE);
			rect.image->sprite = (Sprite*)manager->load(Path(tmp));
		}

		serializer.read(&rect.image->color);
		serializer.read(&rect.image->flags.base);
		
		m_project.onComponentCreated(gameobject, GUI_IMAGE_TYPE, this);
	}


	void serializeText(ISerializer& serializer, GameObject gameobject)
	{
		const GUIRect& rect = *m_rects[gameobject];
		serializer.write("font", rect.text->getFontResource() ? rect.text->getFontResource()->getPath().c_str() : "");
		serializer.write("align", (int)rect.text->horizontal_align);
		serializer.write("color", rect.text->color);
		serializer.write("font_size", rect.text->getFontSize());
		serializer.write("text", rect.text->text.c_str());
	}


	void deserializeText(IDeserializer& serializer, GameObject gameobject, int /*scene_version*/)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			GUIRect* rect = MALMY_NEW(m_allocator, GUIRect);
			rect->gameobject = gameobject;
			idx = m_rects.insert(gameobject, rect);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.text = MALMY_NEW(m_allocator, GUIText)(m_allocator);

		char tmp[MAX_PATH_LENGTH];
		serializer.read(tmp, lengthOf(tmp));
		serializer.read((int*)&rect.text->horizontal_align);
		serializer.read(&rect.text->color);
		int font_size;
		serializer.read(&font_size);
		rect.text->setFontSize(font_size);
		serializer.read(&rect.text->text);
		FontResource* res = tmp[0] ? (FontResource*)m_font_manager->load(Path(tmp)) : nullptr;
		rect.text->setFontResource(res);

		m_project.onComponentCreated(gameobject, GUI_TEXT_TYPE, this);
	}


	void clear() override
	{
		for (GUIRect* rect : m_rects)
		{
			MALMY_DELETE(m_allocator, rect->input_field);
			MALMY_DELETE(m_allocator, rect->image);
			MALMY_DELETE(m_allocator, rect->text);
			MALMY_DELETE(m_allocator, rect);
		}
		m_rects.clear();
		m_buttons.clear();
	}


	void hoverOut(const GUIRect& rect)
	{
		int idx = m_buttons.find(rect.gameobject);
		if (idx < 0) return;

		const GUIButton& button = m_buttons.at(idx);

		if (rect.image) rect.image->color = button.normal_color;
		if (rect.text) rect.text->color = button.normal_color;

		m_rect_hovered_out.invoke(rect.gameobject);
	}


	void hover(const GUIRect& rect)
	{
		int idx = m_buttons.find(rect.gameobject);
		if (idx < 0) return;

		const GUIButton& button = m_buttons.at(idx);

		if (rect.image) rect.image->color = button.hovered_color;
		if (rect.text) rect.text->color = button.hovered_color;

		m_rect_hovered.invoke(rect.gameobject);
	}


	void handleMouseAxisEvent(const Rect& parent_rect, GUIRect& rect, const Vec2& mouse_pos, const Vec2& prev_mouse_pos)
	{
		if (!rect.flags.isSet(GUIRect::IS_ENABLED)) return;

		const Rect& r = getRectOnCanvas(parent_rect, rect);

		bool is = contains(r, mouse_pos);
		bool was = contains(r, prev_mouse_pos);
		if (is != was && m_buttons.find(rect.gameobject) >= 0)
		{
			is ? hover(rect) : hoverOut(rect);
		}

		for (GameObject e = m_project.getFirstChild(rect.gameobject); e.isValid(); e = m_project.getNextSibling(e))
		{
			int idx = m_rects.find(e);
			if (idx < 0) continue;
			handleMouseAxisEvent(r, *m_rects.at(idx), mouse_pos, prev_mouse_pos);
		}
	}


	static bool contains(const Rect& rect, const Vec2& pos)
	{
		return pos.x >= rect.x && pos.y >= rect.y && pos.x <= rect.x + rect.w && pos.y <= rect.y + rect.h;
	}


	bool isButtonDown(GameObject e) const
	{
		for(int i = 0, c = m_buttons_down_count; i < c; ++i)
		{
			if (m_buttons_down[i] == e) return true;
		}
		return false;
	}


	void handleMouseButtonEvent(const Rect& parent_rect, GUIRect& rect, const InputSystem::Event& event)
	{
		if (!rect.flags.isSet(GUIRect::IS_ENABLED)) return;
		bool is_up = event.data.button.state == InputSystem::ButtonEvent::UP;

		Vec2 pos(event.data.button.x_abs, event.data.button.y_abs);
		const Rect& r = getRectOnCanvas(parent_rect, rect);
		
		if (contains(r, pos) && contains(r, m_mouse_down_pos))
		{
			if (m_buttons.find(rect.gameobject) >= 0)
			{
				if (is_up && isButtonDown(rect.gameobject))
				{
					m_focused_gameobject = INVALID_GAMEOBJECT;
					m_button_clicked.invoke(rect.gameobject);
				}
				if (!is_up)
				{
					if (m_buttons_down_count < lengthOf(m_buttons_down))
					{
						m_buttons_down[m_buttons_down_count] = rect.gameobject;
						++m_buttons_down_count;
					}
					else
					{
						g_log_error.log("GUI") << "Too many buttons pressed at once";
					}
				}
			}
			
			if (rect.input_field && is_up)
			{
				m_focused_gameobject = rect.gameobject;
				if (rect.text)
				{
					rect.input_field->cursor = rect.text->text.length();
					rect.input_field->anim = 0;
				}
			}
		}

		for (GameObject e = m_project.getFirstChild(rect.gameobject); e.isValid(); e = m_project.getNextSibling(e))
		{
			int idx = m_rects.find(e);
			if (idx < 0) continue;
			handleMouseButtonEvent(r, *m_rects.at(idx), event);
		}
	}


	GUIRect* getInput(GameObject e)
	{
		if (!e.isValid()) return nullptr;

		int rect_idx = m_rects.find(e);
		if (rect_idx < 0) return nullptr;

		GUIRect* rect = m_rects.at(rect_idx);
		if (!rect->text) return nullptr;
		if (!rect->input_field) return nullptr;

		return rect;
	}


	void handleTextInput(const InputSystem::Event& event)
	{
		const GUIRect* rect = getInput(m_focused_gameobject);
		if (!rect) return;
		rect->text->text.insert(rect->input_field->cursor, event.data.text.text);
		rect->input_field->cursor += stringLength(event.data.text.text);
	}


	void handleKeyboardButtonEvent(const InputSystem::Event& event)
	{
		const GUIRect* rect = getInput(m_focused_gameobject);
		if (!rect) return;
		if (event.data.button.state != InputSystem::ButtonEvent::DOWN) return;

		rect->input_field->anim = 0;

		switch (event.data.button.key_id)
		{
			case SDLK_HOME: rect->input_field->cursor = 0; break;
			case SDLK_END: rect->input_field->cursor = rect->text->text.length(); break;
			case SDLK_BACKSPACE:
				if (rect->text->text.length() > 0 && rect->input_field->cursor > 0)
				{
					rect->text->text.eraseAt(rect->input_field->cursor - 1);
					--rect->input_field->cursor;
				}
				break;
			case SDLK_DELETE:
				if (rect->input_field->cursor < rect->text->text.length())
				{
					rect->text->text.eraseAt(rect->input_field->cursor);
				}
				break;
			case SDLK_LEFT:
				if (rect->input_field->cursor > 0) --rect->input_field->cursor;
				break;
			case SDLK_RIGHT:
				if (rect->input_field->cursor < rect->text->text.length()) ++rect->input_field->cursor;
				break;
		}
	}


	void handleInput()
	{
		if (!m_root) return;
		InputSystem& input = m_system.getEngine().getInputSystem();
		const InputSystem::Event* events = input.getEvents();
		int events_count = input.getEventsCount();
		for (int i = 0; i < events_count; ++i)
		{
			const InputSystem::Event& event = events[i];
			switch (event.type)
			{
				case InputSystem::Event::TEXT_INPUT:
					handleTextInput(event);
					break;
				case InputSystem::Event::AXIS:
					if (event.device->type == InputSystem::Device::MOUSE)
					{
						Vec2 pos(event.data.axis.x_abs, event.data.axis.y_abs);
						Vec2 old_pos = pos - Vec2(event.data.axis.x, event.data.axis.y);
						handleMouseAxisEvent({0, 0,  m_canvas_size.x, m_canvas_size.y }, *m_root, pos, old_pos);
					}
					break;
				case InputSystem::Event::BUTTON:
					if (event.device->type == InputSystem::Device::MOUSE)
					{
						if (event.data.button.state == InputSystem::ButtonEvent::DOWN)
						{
							m_mouse_down_pos.x = event.data.button.x_abs;
							m_mouse_down_pos.y = event.data.button.y_abs;
						}
						handleMouseButtonEvent({ 0, 0, m_canvas_size.x, m_canvas_size.y }, *m_root, event);
						if (event.data.button.state == InputSystem::ButtonEvent::UP) m_buttons_down_count = 0;
					}
					else if (event.device->type == InputSystem::Device::KEYBOARD)
					{
						handleKeyboardButtonEvent(event);
					}
					break;
			}
		}
	}


	void blinkCursor(float time_delta)
	{
		GUIRect* rect = getInput(m_focused_gameobject);
		if (!rect) return;

		rect->input_field->anim += time_delta;
		rect->input_field->anim = fmodf(rect->input_field->anim, CURSOR_BLINK_PERIOD);
	}


	void update(float time_delta, bool paused) override
	{
		if (paused) return;

		handleInput();
		blinkCursor(time_delta);
	}


	void createRect(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		GUIRect* rect;
		if (idx >= 0)
		{
			rect = m_rects.at(idx);
		}
		else
		{
			rect = MALMY_NEW(m_allocator, GUIRect);
			m_rects.insert(gameobject, rect);
		}
		rect->gameobject = gameobject;
		rect->flags.set(GUIRect::IS_VALID);
		rect->flags.set(GUIRect::IS_ENABLED);
		m_project.onComponentCreated(gameobject, GUI_RECT_TYPE, this);
		m_root = findRoot();
	}


	void createText(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			createRect(gameobject);
			idx = m_rects.find(gameobject);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.text = MALMY_NEW(m_allocator, GUIText)(m_allocator);

		m_project.onComponentCreated(gameobject, GUI_TEXT_TYPE, this);
	}


	void createRenderTarget(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			createRect(gameobject);
			idx = m_rects.find(gameobject);
		}
		m_rects.at(idx)->render_target = &EMPTY_RENDER_TARGET;
		m_project.onComponentCreated(gameobject, GUI_RENDER_TARGET_TYPE, this);
	}


	void createButton(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			createRect(gameobject);
			idx = m_rects.find(gameobject);
		}
		GUIImage* image = m_rects.at(idx)->image;
		GUIButton& button = m_buttons.emplace(gameobject);
		if (image)
		{
			button.hovered_color = image->color;
			button.normal_color = image->color;
		}
		m_project.onComponentCreated(gameobject, GUI_BUTTON_TYPE, this);
	}


	void createInputField(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			createRect(gameobject);
			idx = m_rects.find(gameobject);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.input_field = MALMY_NEW(m_allocator, GUIInputField);

		m_project.onComponentCreated(gameobject, GUI_INPUT_FIELD_TYPE, this);
	}


	void createImage(GameObject gameobject)
	{
		int idx = m_rects.find(gameobject);
		if (idx < 0)
		{
			createRect(gameobject);
			idx = m_rects.find(gameobject);
		}
		GUIRect& rect = *m_rects.at(idx);
		rect.image = MALMY_NEW(m_allocator, GUIImage);
		rect.image->flags.set(GUIImage::IS_ENABLED);

		m_project.onComponentCreated(gameobject, GUI_IMAGE_TYPE, this);
	}


	GUIRect* findRoot()
	{
		if (m_rects.size() == 0) return nullptr;
		for (int i = 0, n = m_rects.size(); i < n; ++i)
		{
			GUIRect& rect = *m_rects.at(i);
			if (!rect.flags.isSet(GUIRect::IS_VALID)) continue;
			GameObject e = m_rects.getKey(i);
			GameObject parent = m_project.getParent(e);
			if (parent == INVALID_GAMEOBJECT) return &rect;
			if (m_rects.find(parent) < 0) return &rect;
		}
		return nullptr;
	}


	void destroyRect(GameObject gameobject)
	{
		GUIRect* rect = m_rects[gameobject];
		rect->flags.set(GUIRect::IS_VALID, false);
		if (rect->image == nullptr && rect->text == nullptr && rect->input_field == nullptr)
		{
			MALMY_DELETE(m_allocator, rect);
			m_rects.erase(gameobject);
			
		}
		if (rect == m_root)
		{
			m_root = findRoot();
		}
		m_project.onComponentDestroyed(gameobject, GUI_RECT_TYPE, this);
	}


	void destroyButton(GameObject gameobject)
	{
		m_buttons.erase(gameobject);
		m_project.onComponentDestroyed(gameobject, GUI_BUTTON_TYPE, this);
	}


	void destroyRenderTarget(GameObject gameobject)
	{
		GUIRect* rect = m_rects[gameobject];
		rect->render_target = nullptr;
		m_project.onComponentDestroyed(gameobject, GUI_RENDER_TARGET_TYPE, this);
	}


	void destroyInputField(GameObject gameobject)
	{
		GUIRect* rect = m_rects[gameobject];
		MALMY_DELETE(m_allocator, rect->input_field);
		rect->input_field = nullptr;
		m_project.onComponentDestroyed(gameobject, GUI_INPUT_FIELD_TYPE, this);
	}


	void destroyImage(GameObject gameobject)
	{
		GUIRect* rect = m_rects[gameobject];
		MALMY_DELETE(m_allocator, rect->image);
		rect->image = nullptr;
		m_project.onComponentDestroyed(gameobject, GUI_IMAGE_TYPE, this);
	}


	void destroyText(GameObject gameobject)
	{
		GUIRect* rect = m_rects[gameobject];
		MALMY_DELETE(m_allocator, rect->text);
		rect->text = nullptr;
		m_project.onComponentDestroyed(gameobject, GUI_TEXT_TYPE, this);
	}


	void serialize(OutputBlob& serializer) override
	{
		serializer.write(m_rects.size());
		for (GUIRect* rect : m_rects)
		{
			serializer.write(rect->flags);
			serializer.write(rect->gameobject);
			serializer.write(rect->top);
			serializer.write(rect->right);
			serializer.write(rect->bottom);
			serializer.write(rect->left);

			serializer.write(rect->image != nullptr);
			if (rect->image)
			{
				serializer.writeString(rect->image->sprite ? rect->image->sprite->getPath().c_str() : "");
				serializer.write(rect->image->color);
				serializer.write(rect->image->flags.base);
			}

			serializer.write(rect->input_field != nullptr);

			serializer.write(rect->text != nullptr);
			if (rect->text)
			{
				serializer.writeString(rect->text->getFontResource() ? rect->text->getFontResource()->getPath().c_str() : "");
				serializer.write(rect->text->horizontal_align);
				serializer.write(rect->text->color);
				serializer.write(rect->text->getFontSize());
				serializer.write(rect->text->text);
			}
		}

		serializer.write(m_buttons.size());
		for (int i = 0, c = m_buttons.size(); i < c; ++i)
		{
			serializer.write(m_buttons.getKey(i));
			const GUIButton& button = m_buttons.at(i);
			serializer.write(button);
		}

	}


	void deserialize(InputBlob& serializer) override
	{
		clear();
		int count = serializer.read<int>();
		for (int i = 0; i < count; ++i)
		{
			GUIRect* rect = MALMY_NEW(m_allocator, GUIRect);
			serializer.read(rect->flags);
			serializer.read(rect->gameobject);
			serializer.read(rect->top);
			serializer.read(rect->right);
			serializer.read(rect->bottom);
			serializer.read(rect->left);
			m_rects.insert(rect->gameobject, rect);
			if (rect->flags.isSet(GUIRect::IS_VALID))
			{
				m_project.onComponentCreated(rect->gameobject, GUI_RECT_TYPE, this);
			}

			char tmp[MAX_PATH_LENGTH];
			bool has_image = serializer.read<bool>();
			if (has_image)
			{
				rect->image = MALMY_NEW(m_allocator, GUIImage);
				serializer.readString(tmp, lengthOf(tmp));
				if (tmp[0] == '\0')
				{
					rect->image->sprite = nullptr;
				}
				else
				{
					auto* manager = m_system.getEngine().getResourceManager().get(Sprite::TYPE);
					rect->image->sprite = (Sprite*)manager->load(Path(tmp));
				}
				serializer.read(rect->image->color);
				serializer.read(rect->image->flags.base);
				m_project.onComponentCreated(rect->gameobject, GUI_IMAGE_TYPE, this);

			}
			bool has_input_field = serializer.read<bool>();
			if (has_input_field)
			{
				rect->input_field = MALMY_NEW(m_allocator, GUIInputField);
				m_project.onComponentCreated(rect->gameobject, GUI_INPUT_FIELD_TYPE, this);

			}
			bool has_text = serializer.read<bool>();
			if (has_text)
			{
				rect->text = MALMY_NEW(m_allocator, GUIText)(m_allocator);
				GUIText& text = *rect->text;
				serializer.readString(tmp, lengthOf(tmp));
				serializer.read(text.horizontal_align);
				serializer.read(text.color);
				int font_size;
				serializer.read(font_size);
				text.setFontSize(font_size);
				serializer.read(text.text);
				FontResource* res = tmp[0] == 0 ? nullptr : (FontResource*)m_font_manager->load(Path(tmp));
				text.setFontResource(res);
				m_project.onComponentCreated(rect->gameobject, GUI_TEXT_TYPE, this);
			}
		}
		count = serializer.read<int>();
		for (int i = 0; i < count; ++i)
		{
			GameObject e;
			serializer.read(e);
			GUIButton& button = m_buttons.emplace(e);
			serializer.read(button);
		}
		m_root = findRoot();
	}
	

	void setRenderTarget(GameObject gameobject, bgfx::TextureHandle* texture_handle) override
	{
		m_rects[gameobject]->render_target = texture_handle;
	}

	
	DelegateList<void(GameObject)>& buttonClicked() override
	{
		return m_button_clicked;
	}


	DelegateList<void(GameObject)>& rectHovered() override
	{
		return m_rect_hovered;
	}


	DelegateList<void(GameObject)>& rectHoveredOut() override
	{
		return m_rect_hovered_out;
	}


	Project& getProject() override { return m_project; }
	IPlugin& getPlugin() const override { return m_system; }

	IAllocator& m_allocator;
	Project& m_project;
	GUISystem& m_system;
	
	AssociativeArray<GameObject, GUIRect*> m_rects;
	AssociativeArray<GameObject, GUIButton> m_buttons;
	GameObject m_buttons_down[16];
	int m_buttons_down_count;
	GameObject m_focused_gameobject = INVALID_GAMEOBJECT;
	GUIRect* m_root = nullptr;
	FontManager* m_font_manager = nullptr;
	Vec2 m_canvas_size;
	Vec2 m_mouse_down_pos;
	DelegateList<void(GameObject)> m_button_clicked;
	DelegateList<void(GameObject)> m_rect_hovered;
	DelegateList<void(GameObject)> m_rect_hovered_out;
};


GUIScene* GUIScene::createInstance(GUISystem& system,
	Project& project,
	IAllocator& allocator)
{
	return MALMY_NEW(allocator, GUISceneImpl)(system, project, allocator);
}


void GUIScene::destroyInstance(GUIScene* scene)
{
	MALMY_DELETE(static_cast<GUISceneImpl*>(scene)->m_allocator, scene);
}


} // namespace Malmy
