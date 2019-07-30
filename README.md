# tachyon
Cross platform dynamic link library and component interface abstraction.


<H1>PART 1<br>How To Make a Tachyon Component/Module</H1>

Using tachyon is easy.  There is a simple pattern to follow to get up running quickly.

1) Include the tachyon component header : component.hpp

2) Create a new class and inherit from tachyon::Component, if this is a Contract or Interface then you should define all pure virtual functions in this class.


```c++

class ITestContract : public tachyon::Component
{
public:
   virtual ~ITestContract() {}
   virtual bool PostInit() = 0;
   virtual void TestMethod() = 0;
   virtual const char *GetName() = 0;
};

```

3) Always add virtual destructors to every class

4) If using a Contract or Interface, create a second Implementation class and inherit from your Interface.

5) Make sure your constructor is <strong>protected</strong> and make sure you have a static function to create new instances as shown below in the Create method. The static method can be named whatever you like, the important thing is that it takes no parameters and returns a pointer to a tachyon::Component (The importantance of this will become clear later).

```c++

class TestComponent1 : public ITestContract
{
public:
   virtual ~TestComponent1();

   virtual const char *GetName();
   virtual void TestMethod();
   virtual bool PostInit();

   static tachyon::Component *Create();

protected:
   TestComponent1();
};

```

6) Next you need to export a "C" function that tachyon MasterFactory will be able to find when it loads your dynamic link library. This function must have the following signature but can be placed in any h file in your project.

```c++

extern "C"
{
   EXPORT_API bool InitLibrary(tachyon::MasterFactory *m);
}

```

7) Now add your implementation code to one or more .cpp files

```c++

TestComponent1::TestComponent1() 
{
}

TestComponent1::~TestComponent1()
{
}

const char *TestComponent1::GetName()
{
   return "some name";
}

void TestComponent1::TestMethod()
{
   std::cout << "TestComponent1::TestMethod()" << std::endl;
}

bool TestComponent1::PostInit()
{
   return true;
}

tachyon::Component *TestComponent1::Create()
{
   return new TestComponent1();
}

EXPORT_API bool InitLibrary(tachyon::MasterFactory *m)
{
   m->Register("ITestContract::TestComponent1", TestComponent1::Create);
   return true;
}

```
<p>
The InitLibrary method registers with the MasterFactory and provides a searchable string key that callers will use and a static Create method.</p>


<H2>Project / Makefiles</H2>
Make sure to define the preprocessor symbol BUILD_SHARED

<H2>Windows Only</H2>
Make sure to include a standard dllmain.cpp in your project

```c++

#define WIN32_LEAN_AND_MEAN 
#include <windows.h>
#include <SDKDDKVer.h>


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

```

<H1>PART 2<br>Creating a main program that uses tachyon modules</H1>

1) Create your main.cpp file

2) Include component.hpp from tachyon

3) Save a reference to the tachyon::MasterFactory

4) Load your dynamic link library. The module name you pass to MasterFactory will be based on your build system, and OS.  For example on Windows you would pass "component1" to open the file : component1.dll, and on Linux the same would open "libcomponent1.so".  Make sure the shared library is in the same directory as your executable or is in the current path.

```c++

#include <iostream>
#include <tachyon/component.hpp>
#include <itestcontract.hpp>


using namespace tachyon;


int main()
{
   MasterFactory &mf = MasterFactory::Instance();
   mf.Manage("component1");

   // test component 1 is implemented in a shared dll 
   sp<ITestContract> sp1 = mf.Create<ITestContract>("ITestContract.TestComponent1");
   
   // test the smart pointer points to something
   if (!sp1.isValid())
   {
      std::cerr << "Invalid TestComponent1, program abort" << std::endl;
      exit(0);
   }
	
   // call the initialization logic, because the object was created via factory any complex
   // init steps or resource allocation should occur here
   if (!sp1->PostInit())
   {
      std::cerr << "TestComponent1, PostInit failure, program abort" << std::endl;
      exit(0);
   }
}

```

<H1>PART 3<br>Advanced Usage</H1>
<h2>Regular Expression Matching</h2>
<p>
tachyon::MasterFactory can match components using regex and return all matching instances in an array, example shown below</p>

```c++
#include <iostream>
#include <tachyon/component.hpp>
#include <itestcontract.hpp>


using namespace tachyon;


int main()
{
   MasterFactory &mf = MasterFactory::Instance();
   mf.Manage("component1");
   mf.Manage("component2");
   std::string pattern = "ITestContract\\.(.*)";
   std::cout << "TEST find contracts using Regex pattern: " << pattern << std::endl;
	
   auto spList = mf.CreateAll<ITestContract>(pattern);
   for (auto itr = spList.begin(); itr != spList.end(); itr++)
   {
      if (itr->isValid())
      {
         std::cout << "MATCH FOUND : " << (*itr)->GetName() << std::endl;
      }
   }
}

```

<h2>Thread Safety</h2>
<p>The tachyon MasterFactory uses a well known C++ thread safe and lazy instantiation pattern. MasterFactory will be constructed on first usage and destructed at program end. It is important to understand dynamic link library memory segmentation and if you are making a tachyon module that ITSELF needs access to other modules and to MasterFactory then wait until the InitLibrary entry point is called and use or save a reference to the MasterFactory pointer that is passed in.  If you try and access the MasterFactory using the singleton method you will end up with a different instance due to the way dynamic libraries work. In some cases you will want to do the latter, for example if your dynamic library is interfacing with some other application and is serving tachyon components to an external application then in that case you would access the MasterFactory via the Instance method just as in an exe program. </p>

<h2>Smart Pointers, Bifurcation</h2>
<p>The tachyon smart pointer class is thread safe and is a wrapper around std::shared_ptr which contains a thread safe control block.  Tachyon sp class is also safe from bifurcation because only MasterFactory can create them, it does so inside a locked mutex region and the tachyon sp class does not ever expose or provide any way to get at the raw pointer.</p>
  
