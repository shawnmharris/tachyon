#pragma once

#include <iostream>
#include <string>
#include <map>
#include <mutex>
#include <memory>
#include <list>
#include <regex>

#define MF_ERR_NOTEXIST "ERROR [Nonexistant Factory Class (%s)/ Not Registered]\n"
#define MF_ERR_REGISTERED "ERROR [Duplicate Factory Class (%s)/ Already Registered]\n"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDKDDKVer.h>
#else
#include <dlfcn.h>
#endif

#ifdef _WIN32

#ifdef BUILD_SHARED
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif

#else

#ifdef BUILD_SHARED
#define EXPORT_API __attribute__((visibility("default")))
#else
#define EXPORT_API
#endif

#endif

namespace tachyon
{

class Component
{
public:
	virtual ~Component() {}

protected:
	Component() {}
};


typedef Component *(*Factory)(void);


template <class T>
class sp
{
public:
	sp(const sp &other)
	{
		m_ptr = other.m_ptr;
	}

	sp<T> &operator=(const sp<T> &r)
	{
		m_ptr = r.m_ptr;
		return this;
	}

	sp<T> &operator=(Component *c)
	{
		m_ptr = std::make_shared(dynamic_cast<T *>(c));
		return this;
	}

	T &operator*() const noexcept
	{
		return *m_ptr;
	}

	T *operator->() const noexcept
	{
		return m_ptr.get();
	}

	bool isValid() const noexcept
	{
		return m_ptr.get() != NULL;
	}

protected:
	friend class MasterFactory;

	sp(Component *c) : m_ptr(dynamic_cast<T *>(c)) {}

	std::shared_ptr<T> m_ptr;
};


class MasterFactory
{
public:
	static MasterFactory &Instance()
	{
		static MasterFactory instance;
		return instance;
	}

	template <typename T>
	sp<T> Create(std::string key)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto itr = m_factories.find(key);

		if (itr == m_factories.end())
		{
			// successful call will result in a callback to Register
			LoadShared(key);
		}

		itr = m_factories.find(key);
		if (itr == m_factories.end())
		{
			printf("%s LINE %d\n", __FILE__, __LINE__); 			
			printf(MF_ERR_NOTEXIST, key.c_str());

			return NULL;
		}

		sp<T> sp(m_factories[key]());
		return sp;
	}
	
	
	template <typename T>
	std::list<sp<T>> CreateAll(const std::string &pattern)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		std::list<sp<T>> spl;
		std::regex expr(pattern);
		std::smatch result;
		auto itr = m_factories.begin();

		while (itr != m_factories.end() && std::regex_search(itr->first, result, expr))
		{
			std::cout << "search match: " << itr->first << std::endl;
			sp<T> sp(m_factories[itr->first]());
			spl.push_back(sp);
			itr++;
		}

		return spl;
	}

	void Manage(std::string library)
	{
		LoadShared(library);
	}

	int Register(std::string key, Factory factory)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);
		auto itr = m_factories.find(key);

		if (itr != m_factories.end())
		{
			printf("%s LINE %d\n", __FILE__, __LINE__); 			
			printf(MF_ERR_REGISTERED, key.c_str());

			throw std::runtime_error(MF_ERR_REGISTERED);
		}

		m_factories[key] = factory;
		return 0;
	}

	MasterFactory(MasterFactory const &) = delete;
	void operator=(MasterFactory const &) = delete;

protected:
	MasterFactory() {}

private:
	std::map<std::string, Factory> m_factories;
	std::recursive_mutex m_mutex;

#ifdef _WIN32
	bool LoadShared(std::string key)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);

		char filename[255] = { 0 };
		sprintf_s(filename, 254, "%s.dll", key.c_str());
		HMODULE hModule = LoadLibraryA(filename);

		if (!hModule)
		{
			printf("could not load shared library %s\n", filename);
			return false;
		}

		printf("shared library %s, loaded\n", filename);
		typedef bool(* DLL_INJECT)(tachyon::MasterFactory * m);
		DLL_INJECT injectLibrary = NULL;

		injectLibrary = (DLL_INJECT)GetProcAddress(hModule, "InitLibrary");

		if (!injectLibrary)
		{
			printf("could not find InitLibrary in %s\n", filename);
			FreeLibrary(hModule);
			return false;
		}

		if (!injectLibrary(this))
		{
			printf("InitLibrary for %s failed\n", filename);
			FreeLibrary(hModule);
			return false;
		}

		printf("succesful call to InitLibrary for %s\n", filename);
		return true;
	}
#elif __linux__
	bool LoadShared(std::string key)
	{
		std::lock_guard<std::recursive_mutex> lock(m_mutex);

		char *error;
		char filename[255] = { 0 };
		sprintf(filename, "lib%s.so", key.c_str());

		void *hModule = dlopen(filename, RTLD_LAZY);

		if (!hModule)
		{
			printf("could not load shared library %s\n", filename);

			sprintf(filename, "./lib%s.so", key.c_str());

			hModule = dlopen(filename, RTLD_LAZY);

			if (!hModule)
			{
				printf("could not load shared library %s\n", filename);
				return false;
			}
		}

		printf("shared library %s, loaded\n", filename);
		typedef bool(*DLL_INJECT)(tachyon::MasterFactory * m);
		DLL_INJECT injectLibrary = NULL;

		injectLibrary = (DLL_INJECT)dlsym(hModule, "InitLibrary");
		error = dlerror();

		if (error != NULL)
		{
			printf("could not find InitLibrary in %s\n", filename);
			dlclose(hModule);
			return false;
		}

		if (!injectLibrary)
		{
			printf("could not find InitLibrary in %s\n", filename);
			dlclose(hModule);
			return false;
		}

		if (!injectLibrary(this))
		{
			printf("InitLibrary for %s failed\n", filename);
			dlclose(hModule);
			return false;
		}

		printf("succesful call to InitLibrary for %s\n", filename);
		return true;
	}
#endif
};
} // namespace tachyon
