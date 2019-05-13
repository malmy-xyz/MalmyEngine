#include "renderer.h"

#include "engine/array.h"
#include "engine/command_line_parser.h"
#include "engine/crc32.h"
#include "engine/debug/debug.h"
#include "engine/engine.h"
#include "engine/fs/os_file.h"
#include "engine/log.h"
#include "engine/profiler.h"
#include "engine/reflection.h"
#include "engine/resource_manager.h"
#include "engine/string.h"
#include "engine/system.h"
#include "engine/project/component.h"
#include "engine/project/project.h"
#include "renderer/draw2d.h"
#include "renderer/font_manager.h"
#include "renderer/material.h"
#include "renderer/material_manager.h"
#include "renderer/model.h"
#include "renderer/model_manager.h"
#include "renderer/render_scene.h"
#include "renderer/shader.h"
#include "renderer/shader_manager.h"
#include "renderer/terrain.h"
#include "renderer/texture.h"
#include "renderer/texture_manager.h"
#include <bgfx/bgfx.h>
#include <cstdio>


namespace bx
{

	struct AllocatorI
	{
		virtual ~AllocatorI() = default;

		/// Allocated, resizes memory block or frees memory.
		///
		/// @param[in] _ptr If _ptr is NULL new block will be allocated.
		/// @param[in] _size If _ptr is set, and _size is 0, memory will be freed.
		/// @param[in] _align Alignment.
		/// @param[in] _file Debug file path info.
		/// @param[in] _line Debug file line info.
		virtual void* realloc(void* _ptr, size_t _size, size_t _align, const char* _file, uint32_t _line) = 0;
	};

} // namespace bx


namespace bgfx
{

struct PlatformData
{
	void* ndt;			//< Native display type
	void* nwh;			//< Native window handle
	void* context;		//< GL context, or D3D device
	void* backBuffer;   //< GL backbuffer, or D3D render target view
	void* backBufferDS; //< Backbuffer depth/stencil.
};


void setPlatformData(const PlatformData& _pd);

} // namespace bgfx


namespace Malmy
{


static const ComponentType MODEL_INSTANCE_TYPE = Reflection::getComponentType("renderable");
static thread_local bgfx::Encoder* s_encoder = nullptr;

struct BoneProperty : Reflection::IEnumProperty
{
	BoneProperty() 
	{ 
		name = "Bone"; 
		getter_code = "RenderScene::getBoneAttachmentBone";
		setter_code = "RenderScene::setBoneAttachmentBone";
	}


	void getValue(ComponentUID cmp, int index, OutputBlob& stream) const override
	{
		RenderScene* scene = static_cast<RenderScene*>(cmp.scene);
		int value = scene->getBoneAttachmentBone(cmp.gameobject);
		stream.write(value);
	}


	void setValue(ComponentUID cmp, int index, InputBlob& stream) const override
	{
		RenderScene* scene = static_cast<RenderScene*>(cmp.scene);
		int value = stream.read<int>();
		scene->setBoneAttachmentBone(cmp.gameobject, value);
	}


	GameObject getModelInstance(RenderScene* render_scene, GameObject bone_attachment) const
	{
		GameObject parent_gameobject = render_scene->getBoneAttachmentParent(bone_attachment);
		if (parent_gameobject == INVALID_GAMEOBJECT) return INVALID_GAMEOBJECT;
		return render_scene->getProject().hasComponent(parent_gameobject, MODEL_INSTANCE_TYPE) ? parent_gameobject : INVALID_GAMEOBJECT;
	}


	int getEnumValueIndex(ComponentUID cmp, int value) const override  { return value; }
	int getEnumValue(ComponentUID cmp, int index) const override { return index; }


	int getEnumCount(ComponentUID cmp) const override
	{
		RenderScene* render_scene = static_cast<RenderScene*>(cmp.scene);
		GameObject model_instance = getModelInstance(render_scene, cmp.gameobject);
		if (!model_instance.isValid()) return 0;

		auto* model = render_scene->getModelInstanceModel(model_instance);
		if (!model || !model->isReady()) return 0;

		return model->getBoneCount();
	}


	const char* getEnumName(ComponentUID cmp, int index) const override
	{
		RenderScene* render_scene = static_cast<RenderScene*>(cmp.scene);
		GameObject model_instance = getModelInstance(render_scene, cmp.gameobject);
		if (!model_instance.isValid()) return "";

		auto* model = render_scene->getModelInstanceModel(model_instance);
		if (!model) return "";

		return model->getBone(index).name.c_str();
	}
};


static void registerProperties(IAllocator& allocator)
{
	using namespace Reflection;

	static auto rotationModeDesc = enumDesciptor<Terrain::GrassType::RotationMode>(
		MALMY_ENUM_VALUE(Terrain::GrassType::RotationMode::ALL_RANDOM),
		MALMY_ENUM_VALUE(Terrain::GrassType::RotationMode::Y_UP),
		MALMY_ENUM_VALUE(Terrain::GrassType::RotationMode::ALIGN_WITH_NORMAL)
	);
	registerEnum(rotationModeDesc);

	static auto render_scene = scene("renderer", 
		component("bone_attachment",
			property("Parent", MALMY_PROP(RenderScene, BoneAttachmentParent)),
			property("Relative position", MALMY_PROP(RenderScene, BoneAttachmentPosition)),
			property("Relative rotation", MALMY_PROP(RenderScene, BoneAttachmentRotation), 
				RadiansAttribute()),
			BoneProperty()
		),
		component("particle_emitter_spawn_shape",
			property("Radius", MALMY_PROP(RenderScene, ParticleEmitterShapeRadius))
		),
		component("particle_emitter_plane",
			property("Bounce", MALMY_PROP(RenderScene, ParticleEmitterPlaneBounce),
				ClampAttribute(0, 1)),
			array("Planes", &RenderScene::getParticleEmitterPlaneCount, &RenderScene::addParticleEmitterPlane, &RenderScene::removeParticleEmitterPlane, 
				property("GameObject", MALMY_PROP(RenderScene, ParticleEmitterPlaneGameObject))
			)
		),
		component("particle_emitter_attractor",
			property("Force", MALMY_PROP(RenderScene, ParticleEmitterAttractorForce)),
			array("Attractors", &RenderScene::getParticleEmitterAttractorCount, &RenderScene::addParticleEmitterAttractor, &RenderScene::removeParticleEmitterAttractor,
				property("GameObject", MALMY_PROP(RenderScene, ParticleEmitterAttractorGameObject))
			)
		),
		component("particle_emitter_alpha",
			sampled_func_property("Alpha", MALMY_PROP(RenderScene, ParticleEmitterAlpha), &RenderScene::getParticleEmitterAlphaCount, 1)
		),
		component("particle_emitter_random_rotation"),
		component("environment_probe",
			property("Enabled reflection", MALMY_PROP_FULL(RenderScene, isEnvironmentProbeReflectionEnabled, enableEnvironmentProbeReflection)),
			property("Override global size", MALMY_PROP_FULL(RenderScene, isEnvironmentProbeCustomSize, enableEnvironmentProbeCustomSize)),
			property("Radiance size", MALMY_PROP(RenderScene, EnvironmentProbeRadianceSize)),
			property("Irradiance size", MALMY_PROP(RenderScene, EnvironmentProbeIrradianceSize))
		),
		component("particle_emitter_force",
			property("Acceleration", MALMY_PROP(RenderScene, ParticleEmitterAcceleration))
		),
		component("particle_emitter_subimage",
			property("Rows", MALMY_PROP(RenderScene, ParticleEmitterSubimageRows)),
			property("Columns", MALMY_PROP(RenderScene, ParticleEmitterSubimageCols))
		),
		component("particle_emitter_size",
			sampled_func_property("Size", MALMY_PROP(RenderScene, ParticleEmitterSize), &RenderScene::getParticleEmitterSizeCount, 1)
		),
		component("scripted_particle_emitter",
			property("Material", MALMY_PROP(RenderScene, ScriptedParticleEmitterMaterialPath),
				ResourceAttribute("Material (*.mat)", Material::TYPE))
		),
		component("particle_emitter",
			property("Life", MALMY_PROP(RenderScene, ParticleEmitterInitialLife)),
			property("Initial size", MALMY_PROP(RenderScene, ParticleEmitterInitialSize)),
			property("Spawn period", MALMY_PROP(RenderScene, ParticleEmitterSpawnPeriod)),
			property("Autoemit", MALMY_PROP(RenderScene, ParticleEmitterAutoemit)),
			property("Local space", MALMY_PROP(RenderScene, ParticleEmitterLocalSpace)),
			property("Material", MALMY_PROP(RenderScene, ParticleEmitterMaterialPath),
				ResourceAttribute("Material (*.mat)", Material::TYPE)),
			property("Spawn count", MALMY_PROP(RenderScene, ParticleEmitterSpawnCount))
		),
		component("particle_emitter_linear_movement",
			property("x", MALMY_PROP(RenderScene, ParticleEmitterLinearMovementX)),
			property("y", MALMY_PROP(RenderScene, ParticleEmitterLinearMovementY)),
			property("z", MALMY_PROP(RenderScene, ParticleEmitterLinearMovementZ))
		),
		component("camera",
			property("Slot", MALMY_PROP(RenderScene, CameraSlot)),
			property("Orthographic size", MALMY_PROP(RenderScene, CameraOrthoSize), 
				MinAttribute(0)),
			property("Orthographic", MALMY_PROP_FULL(RenderScene, isCameraOrtho, setCameraOrtho)),
			property("FOV", MALMY_PROP(RenderScene, CameraFOV),
				RadiansAttribute()),
			property("Near", MALMY_PROP(RenderScene, CameraNearPlane), 
				MinAttribute(0)),
			property("Far", MALMY_PROP(RenderScene, CameraFarPlane), 
				MinAttribute(0))
		),
		component("renderable",
			property("Enabled", MALMY_PROP_FULL(RenderScene, isModelInstanceEnabled, enableModelInstance)),
			property("Source", MALMY_PROP(RenderScene, ModelInstancePath),
				ResourceAttribute("Mesh (*.msh)", Model::TYPE)),
			const_array("Materials", &RenderScene::getModelInstanceMaterialsCount, 
				property("Source", MALMY_PROP(RenderScene, ModelInstanceMaterial),
					ResourceAttribute("Material (*.mat)", Material::TYPE))
			)
		),
		component("global_light",
			property("Color", MALMY_PROP(RenderScene, GlobalLightColor),
				ColorAttribute()),
			property("Intensity", MALMY_PROP(RenderScene, GlobalLightIntensity), 
				MinAttribute(0)),
			property("Indirect intensity", MALMY_PROP(RenderScene, GlobalLightIndirectIntensity), MinAttribute(0)),
			property("Fog density", MALMY_PROP(RenderScene, FogDensity),
				ClampAttribute(0, 1)),
			property("Fog bottom", MALMY_PROP(RenderScene, FogBottom)),
			property("Fog height", MALMY_PROP(RenderScene, FogHeight), 
				MinAttribute(0)),
			property("Fog color", MALMY_PROP(RenderScene, FogColor),
				ColorAttribute()),
			property("Shadow cascades", MALMY_PROP(RenderScene, ShadowmapCascades))
		),
		component("point_light",
			property("Diffuse color", MALMY_PROP(RenderScene, PointLightColor),
				ColorAttribute()),
			property("Specular color", MALMY_PROP(RenderScene, PointLightSpecularColor),
				ColorAttribute()),
			property("Diffuse intensity", MALMY_PROP(RenderScene, PointLightIntensity), 
				MinAttribute(0)),
			property("Specular intensity", MALMY_PROP(RenderScene, PointLightSpecularIntensity)),
			property("FOV", MALMY_PROP(RenderScene, LightFOV), 
				ClampAttribute(0, 360),
				RadiansAttribute()),
			property("Attenuation", MALMY_PROP(RenderScene, LightAttenuation),
				ClampAttribute(0, 1000)),
			property("Range", MALMY_PROP(RenderScene, LightRange), 
				MinAttribute(0)),
			property("Cast shadows", MALMY_PROP(RenderScene, LightCastShadows), 
				MinAttribute(0))
		),
		component("text_mesh",
			property("Text", MALMY_PROP(RenderScene, TextMeshText)),
			property("Font", MALMY_PROP(RenderScene, TextMeshFontPath),
				ResourceAttribute("Font (*.ttf)", FontResource::TYPE)),
			property("Font Size", MALMY_PROP(RenderScene, TextMeshFontSize)),
			property("Color", MALMY_PROP(RenderScene, TextMeshColorRGBA),
				ColorAttribute()),
			property("Camera-oriented", MALMY_PROP_FULL(RenderScene, isTextMeshCameraOriented, setTextMeshCameraOriented))
		),
		component("decal",
			property("Material", MALMY_PROP(RenderScene, DecalMaterialPath),
				ResourceAttribute("Material (*.mat)", Material::TYPE)),
			property("Scale", MALMY_PROP(RenderScene, DecalScale), 
				MinAttribute(0))
		),
		component("terrain",
			property("Material", MALMY_PROP(RenderScene, TerrainMaterialPath),
				ResourceAttribute("Material (*.mat)", Material::TYPE)),
			property("XZ scale", MALMY_PROP(RenderScene, TerrainXZScale), 
				MinAttribute(0)),
			property("Height scale", MALMY_PROP(RenderScene, TerrainYScale), 
				MinAttribute(0)),
			array("grass", &RenderScene::getGrassCount, &RenderScene::addGrass, &RenderScene::removeGrass,
				property("Mesh", MALMY_PROP(RenderScene, GrassPath),
					ResourceAttribute("Mesh (*.msh)", Model::TYPE)),
				property("Distance", MALMY_PROP(RenderScene, GrassDistance),
					MinAttribute(1)),
				property("Density", MALMY_PROP(RenderScene, GrassDensity)),
				enum_property("Mode", MALMY_PROP(RenderScene, GrassRotationMode), rotationModeDesc)
			)
		)
	);
	registerScene(render_scene);
}


struct BGFXAllocator MALMY_FINAL : public bx::AllocatorI
{

	explicit BGFXAllocator(IAllocator& source)
		: m_source(source)
	{
	}


	static const size_t NATURAL_ALIGNEMENT = 8;


	void* realloc(void* _ptr, size_t _size, size_t _alignment, const char*, u32) override
	{
		if (0 == _size)
		{
			if (_ptr)
			{
				if (NATURAL_ALIGNEMENT >= _alignment)
				{
					m_source.deallocate(_ptr);
					return nullptr;
				}

				m_source.deallocate_aligned(_ptr);
			}

			return nullptr;
		}
		else if (!_ptr)
		{
			if (NATURAL_ALIGNEMENT >= _alignment) return m_source.allocate(_size);

			return m_source.allocate_aligned(_size, _alignment);
		}

		if (NATURAL_ALIGNEMENT >= _alignment) return m_source.reallocate(_ptr, _size);

		return m_source.reallocate_aligned(_ptr, _size, _alignment);
	}


	IAllocator& m_source;
};


struct RendererImpl MALMY_FINAL : public Renderer
{
	struct CallbackStub MALMY_FINAL : public bgfx::CallbackI
	{
		explicit CallbackStub(RendererImpl& renderer)
			: m_renderer(renderer)
		{}


		void fatal(bgfx::Fatal::Enum _code, const char* _str) override
		{
			g_log_error.log("Renderer") << _str;
			if (bgfx::Fatal::DebugCheck == _code || bgfx::Fatal::InvalidShader == _code)
			{
				Debug::debugBreak();
			}
			else
			{
				abort();
			}
		}


		void traceVargs(const char* _filePath,
			u16 _line,
			const char* _format,
			va_list _argList) override
		{
		}


		void screenShot(const char* filePath,
			uint32_t width,
			uint32_t height,
			uint32_t pitch,
			const void* data,
			uint32_t size,
			bool yflip) override
		{
			TGAHeader header;
			setMemory(&header, 0, sizeof(header));
			int bytes_per_pixel = 4;
			header.bitsPerPixel = (char)(bytes_per_pixel * 8);
			header.height = (short)height;
			header.width = (short)width;
			header.dataType = 2;
			header.imageDescriptor = 32;

			FS::OsFile file;
			if(!file.open(filePath, FS::Mode::CREATE_AND_WRITE))
			{
				g_log_error.log("Renderer") << "Failed to save screenshot to " << filePath;
				return;
			}
			file.write(&header, sizeof(header));

			for(u32 i = 0; i < height; ++i)
				file.write((const u8*)data + pitch * i, width * 4);
			file.close();
		}


		void captureBegin(u32,
			u32,
			u32,
			bgfx::TextureFormat::Enum,
			bool) override
		{
			ASSERT(false);
		}


		u32 cacheReadSize(u64) override { return 0; }
		bool cacheRead(u64, void*, u32) override { return false; }
		void cacheWrite(u64, const void*, u32) override {}
		void captureEnd() override { ASSERT(false); }
		void captureFrame(const void*, u32) override { ASSERT(false); }

		void setThreadName()
		{
			if (m_is_thread_name_set) return;
			m_is_thread_name_set = true;
			Profiler::setThreadName("bgfx thread");
		}

		void profilerBegin(
			const char* _name
			, uint32_t _abgr
			, const char* _filePath
			, uint16_t _line
		) override
		{
			setThreadName();
			Profiler::beginBlock("bgfx_dynamic");
		}

		void profilerBeginLiteral(
			const char* _name
			, uint32_t _abgr
			, const char* _filePath
			, uint16_t _line
		) override
		{
			setThreadName();
			Profiler::beginBlock(_name);
		}


		void profilerEnd() override
		{
			Profiler::endBlock();
		}

		bool m_is_thread_name_set = false;
		RendererImpl& m_renderer;
	};


	explicit RendererImpl(Engine& engine)
		: m_engine(engine)
		, m_allocator(engine.getAllocator())
		, m_texture_manager(m_allocator)
		, m_model_manager(*this, m_allocator)
		, m_material_manager(*this, m_allocator)
		, m_shader_manager(*this, m_allocator)
		, m_font_manager(nullptr)
		, m_shader_binary_manager(*this, m_allocator)
		, m_passes(m_allocator)
		, m_shader_defines(m_allocator)
		, m_layers(m_allocator)
		, m_bgfx_allocator(m_allocator)
		, m_callback_stub(*this)
		, m_vsync(true)
		, m_main_pipeline(nullptr)
		, m_encoder_list_mutex(false)
		, m_encoders(m_allocator)
	{
		registerProperties(engine.getAllocator());
		bgfx::PlatformData d;
		void* window_handle = engine.getPlatformData().window_handle;
		void* display = engine.getPlatformData().display;
		if (window_handle)
		{
			setMemory(&d, 0, sizeof(d));
			d.nwh = window_handle;
			d.ndt = display;
			bgfx::setPlatformData(d);
		}
		char cmd_line[4096];
		bgfx::RendererType::Enum renderer_type = bgfx::RendererType::Count;
		getCommandLine(cmd_line, lengthOf(cmd_line));
		CommandLineParser cmd_line_parser(cmd_line);
		m_vsync = true;
		while (cmd_line_parser.next())
		{
			if (cmd_line_parser.currentEquals("-opengl"))
			{
				renderer_type = bgfx::RendererType::OpenGL;
				break;
			}
			else if (cmd_line_parser.currentEquals("-no_vsync"))
			{
				m_vsync = false;
				break;
			}
		}

		bgfx::Init init;
		init.limits.maxEncoders = MT::getCPUsCount() + 1;
		init.type = renderer_type;
		init.callback = &m_callback_stub;
		init.allocator = &m_bgfx_allocator;
		bool res = bgfx::init(init);
		ASSERT(res);
		bgfx::reset(800, 600, m_vsync ? BGFX_RESET_VSYNC : 0);
		bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_PROFILER);

		ResourceManager& manager = engine.getResourceManager();
		m_texture_manager.create(Texture::TYPE, manager);
		m_model_manager.create(Model::TYPE, manager);
		m_material_manager.create(Material::TYPE, manager);
		m_shader_manager.create(Shader::TYPE, manager);
		m_font_manager = MALMY_NEW(m_allocator, FontManager)(*this, m_allocator);
		m_font_manager->create(FontResource::TYPE, manager);
		m_shader_binary_manager.create(ShaderBinary::TYPE, manager);

		m_current_pass_hash = crc32("MAIN");
		m_view_counter = 0;
		m_mat_color_uniform = bgfx::createUniform("u_materialColor", bgfx::UniformType::Vec4);
		m_roughness_metallic_emission_uniform =
			bgfx::createUniform("u_roughnessMetallicEmission", bgfx::UniformType::Vec4);

		m_basic_vertex_decl.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.end();
		m_basic_2d_vertex_decl.begin()
			.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();

		m_default_shader = static_cast<Shader*>(m_shader_manager.load(Path("pipelines/common/default.shd")));
		RenderScene::registerLuaAPI(m_engine.getState());
		m_layers.emplace("default");
		m_layers.emplace("transparent");
		m_layers.emplace("water");
		m_layers.emplace("fur");
	}


	~RendererImpl()
	{
		m_shader_manager.unload(*m_default_shader);
		m_texture_manager.destroy();
		m_model_manager.destroy();
		m_material_manager.destroy();
		m_shader_manager.destroy();
		m_font_manager->destroy();
		MALMY_DELETE(m_allocator, m_font_manager);
		m_shader_binary_manager.destroy();

		bgfx::destroy(m_mat_color_uniform);
		bgfx::destroy(m_roughness_metallic_emission_uniform);
		frame(false);
		frame(false);
		bgfx::shutdown();
	}


	void setMainPipeline(Pipeline* pipeline) override
	{
		m_main_pipeline = pipeline;
	}


	bgfx::Encoder* getEncoder() override
	{
		if (s_encoder == nullptr)
		{
			s_encoder = bgfx::begin();
			ASSERT(s_encoder);
			MT::SpinLock lock(m_encoder_list_mutex);
			m_encoders.push(&s_encoder);
		}
		return s_encoder;
	}


	Pipeline* getMainPipeline() override
	{
		return m_main_pipeline;
	}


	int getLayer(const char* name) override
	{
		for (int i = 0; i < m_layers.size(); ++i)
		{
			if (m_layers[i] == name) return i;
		}
		ASSERT(m_layers.size() < 64);
		m_layers.emplace() = name;
		return m_layers.size() - 1;
	}


	int getLayersCount() const override { return m_layers.size(); }
	const char* getLayerName(int idx) const override { return m_layers[idx]; }


	ModelManager& getModelManager() override { return m_model_manager; }
	MaterialManager& getMaterialManager() override { return m_material_manager; }
	ShaderManager& getShaderManager() override { return m_shader_manager; }
	TextureManager& getTextureManager() override { return m_texture_manager; }
	FontManager& getFontManager() override { return *m_font_manager; }
	const bgfx::VertexDecl& getBasicVertexDecl() const override { return m_basic_vertex_decl; }
	const bgfx::VertexDecl& getBasic2DVertexDecl() const override { return m_basic_2d_vertex_decl; }


	void createScenes(Project& ctx) override
	{
		auto* scene = RenderScene::createInstance(*this, m_engine, ctx, m_allocator);
		ctx.addScene(scene);
	}


	void destroyScene(IScene* scene) override { RenderScene::destroyInstance(static_cast<RenderScene*>(scene)); }
	const char* getName() const override { return "renderer"; }
	Engine& getEngine() override { return m_engine; }
	int getShaderDefinesCount() const override { return m_shader_defines.size(); }
	const char* getShaderDefine(int define_idx) override { return m_shader_defines[define_idx]; }
	const char* getPassName(int idx) override { return m_passes[idx]; }
	const bgfx::UniformHandle& getMaterialColorUniform() const override { return m_mat_color_uniform; }
	const bgfx::UniformHandle& getRoughnessMetallicEmissionUniform() const override { return m_roughness_metallic_emission_uniform; }
	void makeScreenshot(const Path& filename) override { bgfx::requestScreenShot(BGFX_INVALID_HANDLE, filename.c_str()); }
	void resize(int w, int h) override { bgfx::reset(w, h, m_vsync ? BGFX_RESET_VSYNC : 0); }
	int getViewCounter() const override { return m_view_counter; }
	void viewCounterAdd() override { ++m_view_counter; }
	Shader* getDefaultShader() override { return m_default_shader; }


	u8 getShaderDefineIdx(const char* define) override
	{
		for (int i = 0; i < m_shader_defines.size(); ++i)
		{
			if (m_shader_defines[i] == define)
			{
				return i;
			}
		}

		if (m_shader_defines.size() >= MAX_SHADER_DEFINES) {
			ASSERT(false);
			g_log_error.log("Renderer") << "Too many shader defines.";
		}

		m_shader_defines.emplace(define);
		return m_shader_defines.size() - 1;
	}


	int getPassIdx(const char* pass) override
	{
		if(stringLength(pass) > sizeof(m_passes[0].data))
		{
			g_log_error.log("Renderer") << "Pass name \"" << pass << "\" is too long.";
			return 0;
		}
		for (int i = 0; i < m_passes.size(); ++i)
		{
			if (m_passes[i] == pass)
			{
				return i;
			}
		}

		m_passes.emplace(pass);
		return m_passes.size() - 1;
	}


	void frame(bool capture) override
	{
		PROFILE_FUNCTION();
		{
			MT::SpinLock lock(m_encoder_list_mutex);
			for (bgfx::Encoder** encoder : m_encoders)
			{
				bgfx::end(*encoder);
				*encoder = nullptr;
			}
			m_encoders.clear();
		}
		bgfx::frame(capture);
		m_view_counter = 0;
	}


	using ShaderDefine = StaticString<32>;
	using Layer = StaticString<32>;


	Engine& m_engine;
	IAllocator& m_allocator;
	Array<ShaderCombinations::Pass> m_passes;
	Array<ShaderDefine> m_shader_defines;
	Array<Layer> m_layers;
	CallbackStub m_callback_stub;
	TextureManager m_texture_manager;
	MaterialManager m_material_manager;
	FontManager* m_font_manager;
	ShaderManager m_shader_manager;
	ShaderBinaryManager m_shader_binary_manager;
	ModelManager m_model_manager;
	u32 m_current_pass_hash;
	int m_view_counter;
	bool m_vsync;
	Shader* m_default_shader;
	BGFXAllocator m_bgfx_allocator;
	bgfx::VertexDecl m_basic_vertex_decl;
	bgfx::VertexDecl m_basic_2d_vertex_decl;
	bgfx::UniformHandle m_mat_color_uniform;
	bgfx::UniformHandle m_roughness_metallic_emission_uniform;
	Pipeline* m_main_pipeline;
	MT::SpinMutex m_encoder_list_mutex;
	Array<bgfx::Encoder**> m_encoders;
};


extern "C"
{
	MALMY_PLUGIN_ENTRY(renderer)
	{
		return MALMY_NEW(engine.getAllocator(), RendererImpl)(engine);
	}
}


} // namespace Malmy



