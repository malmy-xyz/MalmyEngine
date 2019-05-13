#pragma once
#include "malmy.h"

namespace Malmy
{

	class InputBlob;
	class OutputBlob;
	struct Quat;
	struct RigidTransform;
	class string;
	struct Transform;
	struct Vec3;
	struct Vec4;

	struct GameObjectGUID
	{
		u64 value;
		bool operator ==(const GameObjectGUID& rhs) const { return value == rhs.value; }
	};

	const GameObjectGUID INVALID_GAMEOBJECT_GUID = { 0xffffFFFFffffFFFF };
	inline bool isValid(GameObjectGUID guid) { return guid.value != INVALID_GAMEOBJECT_GUID.value; }

	struct ISaveGameObjectGUIDMap
	{
		virtual ~ISaveGameObjectGUIDMap() {}
		virtual GameObjectGUID get(GameObject gameobject) = 0;
	};

	struct ILoadGameObjectGUIDMap
	{
		virtual ~ILoadGameObjectGUIDMap() {}
		virtual GameObject get(GameObjectGUID guid) = 0;
	};

	struct MALMY_ENGINE_API ISerializer
	{
		virtual ~ISerializer() {}

		virtual void write(const char* label, GameObject gameobject) = 0;
		virtual void write(const char* label, const Transform& value) = 0;
		virtual void write(const char* label, const RigidTransform& value) = 0;
		virtual void write(const char* label, const Vec4& value) = 0;
		virtual void write(const char* label, const Vec3& value) = 0;
		virtual void write(const char* label, const Quat& value) = 0;
		virtual void write(const char* label, float value) = 0;
		virtual void write(const char* label, bool value) = 0;
		virtual void write(const char* label, i64 value) = 0;
		virtual void write(const char* label, u64 value) = 0;
		virtual void write(const char* label, i32 value) = 0;
		virtual void write(const char* label, u32 value) = 0;
		virtual void write(const char* label, u16 value) = 0;
		virtual void write(const char* label, i8 value) = 0;
		virtual void write(const char* label, u8 value) = 0;
		virtual void write(const char* label, const char* value) = 0;
		virtual GameObjectGUID getGUID(GameObject gameobject) = 0;
	};

	struct MALMY_ENGINE_API IDeserializer
	{
		virtual ~IDeserializer() {}

		virtual void read(GameObject* gameobject) = 0;
		virtual void read(Transform* value) = 0;
		virtual void read(RigidTransform* value) = 0;
		virtual void read(Vec4* value) = 0;
		virtual void read(Vec3* value) = 0;
		virtual void read(Quat* value) = 0;
		virtual void read(float* value) = 0;
		virtual void read(bool* value) = 0;
		virtual void read(u64* value) = 0;
		virtual void read(i64* value) = 0;
		virtual void read(u32* value) = 0;
		virtual void read(i32* value) = 0;
		virtual void read(u16* value) = 0;
		virtual void read(u8* value) = 0;
		virtual void read(i8* value) = 0;
		virtual void read(char* value, int max_size) = 0;
		virtual void read(string* value) = 0;
		virtual GameObject getGameObject(GameObjectGUID guid) = 0;
	};

	struct MALMY_ENGINE_API TextSerializer MALMY_FINAL : public ISerializer
	{
		TextSerializer(OutputBlob& _blob, ISaveGameObjectGUIDMap& _gameobject_map)
			: blob(_blob)
			, gameobject_map(_gameobject_map)
		{
			//
		}

		void write(const char* label, GameObject gameobject)  override;
		void write(const char* label, const RigidTransform& value)  override;
		void write(const char* label, const Transform& value)  override;
		void write(const char* label, const Vec4& value)  override;
		void write(const char* label, const Vec3& value)  override;
		void write(const char* label, const Quat& value)  override;
		void write(const char* label, float value)  override;
		void write(const char* label, bool value)  override;
		void write(const char* label, i64 value)  override;
		void write(const char* label, u64 value)  override;
		void write(const char* label, i32 value)  override;
		void write(const char* label, u32 value)  override;
		void write(const char* label, u16 value)  override;
		void write(const char* label, i8 value)  override;
		void write(const char* label, u8 value)  override;
		void write(const char* label, const char* value)  override;
		GameObjectGUID getGUID(GameObject gameobject) override;

		OutputBlob& blob;
		ISaveGameObjectGUIDMap& gameobject_map;
	};

	struct MALMY_ENGINE_API TextDeserializer MALMY_FINAL : public IDeserializer
	{
		TextDeserializer(InputBlob& _blob, ILoadGameObjectGUIDMap& _gameobject_map)
			: blob(_blob)
			, gameobject_map(_gameobject_map)
		{
		}

		void read(GameObject* gameobject)  override;
		void read(RigidTransform* value)  override;
		void read(Transform* value)  override;
		void read(Vec4* value)  override;
		void read(Vec3* value)  override;
		void read(Quat* value)  override;
		void read(float* value)  override;
		void read(bool* value)  override;
		void read(u64* value)  override;
		void read(i64* value)  override;
		void read(u32* value)  override;
		void read(i32* value)  override;
		void read(u16* value)  override;
		void read(u8* value)  override;
		void read(i8* value)  override;
		void read(char* value, int max_size)  override;
		void read(string* value)  override;
		GameObject getGameObject(GameObjectGUID guid) override;

		void skip();
		u32 readU32();

		InputBlob& blob;
		ILoadGameObjectGUIDMap& gameobject_map;
	};

}