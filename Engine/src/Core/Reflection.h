#pragma once
#include "Assets/Asset.h"

struct ValueInfo;
struct StructInfo;
struct ArrayInfo;

template<typename T>
struct Reflected
{
	static constexpr bool isReflected = false;
};

struct FieldInfo
{
	const char *name;
	size_t offset = 0;

	// Type info
	const ValueInfo *valueInfo = nullptr;
	const StructInfo *structInfo = nullptr;
	const ArrayInfo *arrayInfo = nullptr;

	bool isSerialized = true;

	// Editor UI info
	const char *displayLabel = nullptr;
	const char *tooltipText = nullptr;

	const AssetTypeInfo *const *registeredAssetType = nullptr;
	const char *const *enumItems = nullptr;
	int enumItemsCount = 0;
	bool isColor = false;
	bool isRadio = false;

	const char *valueFormat = nullptr;
	float minValue = 0.0f;
	float maxValue = 0.0f;
	bool isLogarithmic = false;

	bool isReadOnly = false;
	bool isHidden = false;
	bool (*editCondition)(const void *owner) = nullptr;

	constexpr FieldInfo label(const char *text) const { FieldInfo field = *this; field.displayLabel = text; return field; }
	constexpr FieldInfo tooltip(const char *text) const { FieldInfo field = *this; field.tooltipText = text; return field; }
	constexpr FieldInfo format(const char *text) const { FieldInfo field = *this; field.valueFormat = text; return field; }
	template<int Count>
	constexpr FieldInfo items(const char *const (&names)[Count]) const
	{
		FieldInfo field = *this;
		field.enumItems = names;
		field.enumItemsCount = Count;
		return field;
	}
	constexpr FieldInfo range(float minimum, float maximum) const { FieldInfo field = *this; field.minValue = minimum; field.maxValue = maximum; return field; }
	constexpr FieldInfo color() const { FieldInfo field = *this; field.isColor = true; return field; }
	constexpr FieldInfo logarithmic() const { FieldInfo field = *this; field.isLogarithmic = true; return field; }
	constexpr FieldInfo radio() const { FieldInfo field = *this; field.isRadio = true; return field; }
	constexpr FieldInfo readOnly() const { FieldInfo field = *this; field.isReadOnly = true; return field; }

	template<typename T>
	constexpr FieldInfo asset() const { FieldInfo field = *this; field.registeredAssetType = &AssetTypeInfo::registered<T>; return field; }

	constexpr FieldInfo notSerialized() const { FieldInfo field = *this; field.isSerialized = false; return field; }
	constexpr FieldInfo hidden() const { FieldInfo field = *this; field.isHidden = true; return field; }
	constexpr FieldInfo editIf(bool (*condition)(const void *owner)) const { FieldInfo field = *this; field.editCondition = condition; return field; }

	template<typename T>
	static FieldInfo make(const char *name, size_t offset)
	{
		FieldInfo field{name, offset};
		if constexpr (IsVector<T>::value)
			field.arrayInfo = ArrayInfo::get<T>();
		else if constexpr (Reflected<T>::isReflected)
			field.structInfo = &Reflected<T>::getInfo();
		else
			field.valueInfo = &ValueInfo::get<T>();
		return field;
	}

	void *getAddress(void *object) const { return (uint8_t *)object + offset; }
	const void *getAddress(const void *object) const { return (const uint8_t *)object + offset; }

	template<typename T>
	bool isType() const { return valueInfo == &ValueInfo::get<T>(); }

	template<typename T>
	bool isStruct() const { return structInfo == &Reflected<T>::getInfo(); }

	const AssetTypeInfo *getAssetType() const { return registeredAssetType ? *registeredAssetType : nullptr; }
	bool hasRange() const { return minValue != maxValue; }
	bool isEditable(const void *owner) const { return !isReadOnly && (!editCondition || editCondition(owner)); }
	bool isCategory() const { return !valueInfo && !structInfo && !arrayInfo; }
	bool isDefault(const void *value, const void *default_value) const;
};

enum ValueType
{
	VALUE_TYPE_UNDEFINED,
	VALUE_TYPE_BOOL,
	VALUE_TYPE_INT32,
	VALUE_TYPE_UINT32,
	VALUE_TYPE_UINT64,
	VALUE_TYPE_FLOAT,
	VALUE_TYPE_STRING,
};

template<typename T> inline constexpr ValueType value_type = VALUE_TYPE_UNDEFINED;
template<> inline constexpr ValueType value_type<bool> = VALUE_TYPE_BOOL;
template<> inline constexpr ValueType value_type<int32_t> = VALUE_TYPE_INT32;
template<> inline constexpr ValueType value_type<uint32_t> = VALUE_TYPE_UINT32;
template<> inline constexpr ValueType value_type<uint64_t> = VALUE_TYPE_UINT64;
template<> inline constexpr ValueType value_type<float> = VALUE_TYPE_FLOAT;
template<> inline constexpr ValueType value_type<eastl::string> = VALUE_TYPE_STRING;
template<> inline constexpr ValueType value_type<Engine::GUID> = VALUE_TYPE_UINT64;

struct ValueInfo
{
	ValueType type;
	bool (*equals)(const void *left, const void *right);

	template<typename T>
	static constexpr ValueType typeOf()
	{
		if constexpr (std::is_enum_v<T>)
			return value_type<std::underlying_type_t<T>>;
		else
			return value_type<T>;
	}

	template<typename T>
	static const ValueInfo &get()
	{
		static_assert(typeOf<T>() != VALUE_TYPE_UNDEFINED, "reflect this type with REFLECT_BEGIN");
		static const ValueInfo info{typeOf<T>(), [](const void *left, const void *right) { return *(const T *)left == *(const T *)right; }};
		return info;
	}
};

struct StructInfo
{
	const char *name;
	const FieldInfo *fields;
	int fieldsCount;
	const void *defaults;
	size_t size;

	bool isDefault(const void *object, const void *default_object) const;

	const FieldInfo *findField(const char *field_name) const
	{
		for (int i = 0; i < fieldsCount; i++)
			if (strcmp(fields[i].name, field_name) == 0)
				return &fields[i];
		return nullptr;
	}
};

template<typename T>
struct IsVector: std::false_type {};

template<typename T, typename A>
struct IsVector<eastl::vector<T, A>>: std::true_type
{
	using Element = T;
	using Array = eastl::vector<T, A>;
};

struct ArrayInfo
{
	FieldInfo element;
	int (*size)(const void *array);
	void *(*at)(const void *array, int index);
	void (*resize)(void *array, int count);
	void (*remove)(void *array, int index);

	template<typename T>
	static const ArrayInfo *get()
	{
		using Array = typename IsVector<T>::Array;
		static const ArrayInfo info{
			FieldInfo::make<typename IsVector<T>::Element>("Element", 0),
			[](const void *array) { return (int)((const Array *)array)->size(); },
			[](const void *array, int index) { return (void *)&(*(Array *)array)[index]; },
			[](void *array, int count) { ((Array *)array)->resize(count); },
			[](void *array, int index) { ((Array *)array)->erase(((Array *)array)->begin() + index); },
		};
		return &info;
	}
};

inline bool FieldInfo::isDefault(const void *value, const void *default_value) const
{
	if (valueInfo)
		return valueInfo->equals(value, default_value);
	if (arrayInfo)
		return arrayInfo->size(value) == 0;
	return structInfo->isDefault(value, default_value);
}

inline bool StructInfo::isDefault(const void *object, const void *default_object) const
{
	for (int i = 0; i < fieldsCount; i++)
	{
		const FieldInfo &field = fields[i];
		if (field.isCategory() || !field.isSerialized)
			continue;

		if (!field.isDefault(field.getAddress(object), field.getAddress(default_object)))
			return false;
	}
	return true;
}

// This reflection
//  REFLECT_BEGIN(Fog)
//      REFLECT_CATEGORY("Fog"),
//      REFLECT_FIELD(enabled),
//      REFLECT_FIELD(density).range(0.0f, 1.0f).EDIT_IF(owner.enabled),
//  REFLECT_END()
// Expands to
//  template<> struct Reflected<Fog>
//  {
//      static constexpr bool isReflected = true;
//      using Type = Fog;
//      static constexpr const char *name = "Fog";
//      static const StructInfo &getInfo()
//      {
//          static const FieldInfo fields[] = {
//              FieldInfo{"Fog"},
//              FieldInfo::make<decltype(Type::enabled)>("enabled", offsetof(Type, enabled)),
//              FieldInfo::make<decltype(Type::density)>("density", offsetof(Type, density)).range(0.0f, 1.0f)
//                  .editIf([](const void *pointer) { const Type &owner = *(const Type *)pointer; return owner.enabled; }),
//          };
//          static const Type defaults{};
//          static const StructInfo result{name, fields, sizeof(fields) / sizeof(FieldInfo), &defaults, sizeof(Type)};
//          return result;
//      }
//  };
#define REFLECT_BEGIN(T) \
	template<> struct Reflected<T> \
	{ \
		static constexpr bool isReflected = true; \
		using Type = T; \
		static constexpr const char *name = #T; \
		static const StructInfo &getInfo() \
		{ \
			static const FieldInfo fields[] = {

#define REFLECT_FIELD(member) FieldInfo::make<decltype(Type::member)>(#member, offsetof(Type, member))
#define REFLECT_CATEGORY(title) FieldInfo{title}
#define EDIT_IF(expression) editIf([](const void *pointer) { const Type &owner = *(const Type *)pointer; return expression; })

#define REFLECT_END() \
			}; \
			static const Type defaults{}; \
			static const StructInfo result{name, fields, sizeof(fields) / sizeof(FieldInfo), &defaults, sizeof(Type)}; \
			return result; \
		} \
	};

REFLECT_BEGIN(glm::vec3)
	REFLECT_FIELD(x),
	REFLECT_FIELD(y),
	REFLECT_FIELD(z),
REFLECT_END()

REFLECT_BEGIN(glm::vec4)
	REFLECT_FIELD(x),
	REFLECT_FIELD(y),
	REFLECT_FIELD(z),
	REFLECT_FIELD(w),
REFLECT_END()

REFLECT_BEGIN(glm::ivec3)
	REFLECT_FIELD(x),
	REFLECT_FIELD(y),
	REFLECT_FIELD(z),
REFLECT_END()

REFLECT_BEGIN(glm::quat)
	REFLECT_FIELD(x),
	REFLECT_FIELD(y),
	REFLECT_FIELD(z),
	REFLECT_FIELD(w),
REFLECT_END()

REFLECT_BEGIN(AssetReference)
	REFLECT_FIELD(guid),
	REFLECT_FIELD(path),
REFLECT_END()
