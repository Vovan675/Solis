#pragma once

class RefCounted
{
public:
	virtual ~RefCounted() = default;

	void addRef() const
	{
		ref_count.fetch_add(1, std::memory_order_relaxed);
	}

	void release() const
	{
		int refs = ref_count.fetch_sub(1, std::memory_order_acq_rel);
		if (getRefCount() == 0)
			delete this;
	}

	uint32_t getRefCount() const
	{
		return ref_count.load();
	}
private:
	mutable std::atomic<uint32_t> ref_count = 0;
};


template<typename T>
class Ref
{
public:
	Ref(): reference(nullptr) {}

	Ref(T *in_reference): reference(in_reference)
	{
		if (reference)
			reference->addRef();
	}

	Ref(const Ref<T> &copy) : reference(copy.reference)
	{
		if (reference)
			reference->addRef();
	}

	template<typename CopyT>
	Ref(const Ref<CopyT> &copy) 
	{
		reference = static_cast<T *>(copy.getReference());
		if (reference)
			reference->addRef();
	}

	Ref(Ref<T> &&move) : reference(move.reference)
	{
		move.reference = nullptr;
	}

	template<typename CopyT>
	Ref(Ref<CopyT> &&move) 
	{
		reference = static_cast<T *>(move.getReference());
		move.reference = nullptr;
	}

	~Ref()
	{
		if (reference)
			reference->release();
	}

	Ref &operator =(T *other)
	{
		if (reference != other)
		{
			if (other)
				other->addRef();
			if (reference)
				reference->release();

			reference = other;
		}
		return *this;
	}


	Ref &operator =(const Ref<T> &other)
	{
		return *this = other.reference;
	}

	template<typename OtherT>
	Ref &operator =(const Ref<OtherT> &other)
	{
		return *this = other.reference;
	}

	Ref &operator =(Ref<T> &&other)
	{
		if (this != &other)
		{
			if (reference)
				reference->release();
			reference = other.reference;
			other.reference = nullptr;
		}
		return *this;
	}

	template<typename OtherT>
	Ref &operator =(Ref<OtherT> &&other)
	{
		if (this != &other)
		{
			if (reference)
				reference->release();
			reference = other.reference;
			other.reference = nullptr;
		}
		return *this;
	}

	operator bool() const { return reference != nullptr; }
	T *operator ->() const { return reference; }
	operator T *() const { return reference; }
	T &operator *() const { return *reference; }

	T *getReference() const { return reference; }

	bool operator ==(const Ref<T> &other) const { return reference == other.reference; }
	bool operator ==(T *other) const { return reference == other; }

private:
	template<class OtherT>
	friend class Ref;
	T *reference;
};

